#include "wave_generator.h"
#include "dac.h"
#include "tim.h"
#include <math.h>
#include "cmsis_os2.h"
#include "UserDefineManage.h"

/* DAC 通道1 的 DMA 句柄定义在 dac.c，噪声刷新要用 */
extern DMA_HandleTypeDef hdma_dac1;

// DMA缓冲区，所有波形共享
static uint16_t dac1_buffer[MAX_SAMPLE];

// 正在播放的缓冲区长度（0 表示 DAC 通道1 / DMA 未启动）
static uint16_t s_active_n = 0;
// 上一次选用的采样点数（用于频率微调时的迟滞，避免 N 频繁变化导致相位跳变）
static uint16_t s_cur_n = 0;

// 噪声连续刷新相关：标志 + 幅度上下限
static volatile uint8_t s_noise_on = 0;
static uint16_t s_noise_lo = 0;
static uint16_t s_noise_hi = DAC_MAX;

// ------------------- 内部静态函数声明 -------------------
static HAL_StatusTypeDef WaveGen_CommonInit(float freq, uint16_t* N, float amplitude, float bias, float* amp_code);
static HAL_StatusTypeDef wavegen_commit(uint16_t N);
static void noise_fill(uint16_t start, uint16_t count);
static void noise_dma_irq_enable(void);
static void noise_dma_irq_disable(void);

// ------------------- 内部函数实现 -------------------

/**
 * @brief 通用初始化函数，用于所有波形的前置配置
 * @param freq: 频率
 * @param N: 计算出的采样点数
 * @param amplitude: 振幅
 * @param bias: 偏置
 * @param amp_code: 计算出的振幅DAC值
 * @return HAL状态
 * @note  N 的选法：在 [WAVEGEN_N_MIN, min(WAVEGEN_N_MAX, TRIG_MAX_FREQ/freq)] 内
 *        穷举，挑“实际输出频率最接近目标频率”的 (N, K) 组合，误差相同时取更大的 N
 *        （点更多，波形更细腻、占空比分辨率更高）。
 *        旧实现只能取 2 的幂，且 K 直接四舍五入，实测 77% 的频点误差超过 0.1%
 *        （10kHz 会变成 9955.75Hz），现在典型误差 < 0.05%，整数频点基本完全精确。
 */
static HAL_StatusTypeDef WaveGen_CommonInit(float freq, uint16_t* N, float amplitude, float bias, float* amp_code) {
    // 入参合法性检查
    if (freq <= 0 || freq > 50000 ||
        amplitude < MIN_AMPLITUDE || amplitude > MAX_AMPLITUDE ||
        bias < MIN_BIAS || bias > MAX_BIAS) {
        return HAL_ERROR;
    }

    // 遍历所有候选采样点数，挑误差最小的组合
    // 注意：先按 float 比较再转 uint16_t，否则 1e6/freq 超过 65535 时会截断成小数
    // （10~20Hz 附近曾经会算出 0 导致直接返回错误）
    float n_max_f = (float)TRIG_MAX_FREQ / freq;
    uint16_t n_max = (n_max_f >= (float)WAVEGEN_N_MAX) ? WAVEGEN_N_MAX : (uint16_t)n_max_f;
    if (n_max < WAVEGEN_N_MIN) { return HAL_ERROR; }

    uint16_t best_n = 0;
    uint32_t best_k = 0;
    float best_err = 1.0f;

    // 迟滞：上一次用的 N 仍然合法且误差 <= 0.1% 时继续沿用。
    // 一格一格调频率时 N 不变，就不需要重启 DMA，波形相位连续、听不到咔哒声
    // （实测：不加迟滞相邻 1Hz 步进有 23% 会换 N，加上之后只有 3%）
    if (s_cur_n >= WAVEGEN_N_MIN && s_cur_n <= n_max) {
        uint32_t k = (uint32_t)roundf(TIM_FREQ / ((float)s_cur_n * freq));
        if (k >= 1 && k <= 65536) {
            float err = fabsf(TIM_FREQ / ((float)k * (float)s_cur_n) - freq) / freq;
            if (err <= 0.001f) {
                best_n = s_cur_n;
                best_k = k;
                best_err = err;
            }
        }
    }

    if (best_n == 0) {
        for (uint16_t n = WAVEGEN_N_MIN; n <= n_max; n++) {
            // 触发频率 f_trig = N * freq，定时器分频系数 K = TIM_FREQ / f_trig
            uint32_t k = (uint32_t)roundf(TIM_FREQ / ((float)n * freq));
            if (k < 1 || k > 65536) { continue; }
            // 实际输出频率（精确有理数）
            float actual = TIM_FREQ / ((float)k * (float)n);
            float err = fabsf(actual - freq) / freq;
            // 误差更小，或误差相同但 N 更大（点更多）时更新
            if (err <= best_err) {
                best_err = err;
                best_n = n;
                best_k = k;
            }
        }
    }
    if (best_n == 0) { return HAL_ERROR; }
    s_cur_n = best_n;

    *N = best_n;

    // 采样点数变了就必须重启 DMA；长度不变时保持运行，
    // 这样只改频率时波形相位是连续的，不会出现咔哒声
    if (s_active_n != 0 && s_active_n != *N) {
        HAL_TIM_Base_Stop(&TIM_TRGO_HANDLE);
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        s_active_n = 0;
    }

    // 设定定时器参数
    TIM_TypeDef *tim = TIM_TRGO_HANDLE.Instance;
    if (s_active_n == 0) {
        // 没在输出：停掉定时器再改参数，并用 UG 把 PSC/ARR 立刻装入影子寄存器，
        // 否则启动后的第一个周期还在用上一次的分频（会听到一下咔哒）
        HAL_TIM_Base_Stop(&TIM_TRGO_HANDLE);
        tim->PSC = 0;                                   // 候选组合里 K 恒 <= 65536，分频全交给 ARR
        tim->ARR = (uint16_t)(best_k - 1);
        tim->CNT = 0;
        tim->EGR = TIM_EGR_UG;
        tim->SR = 0;                                    // 清掉 UG 产生的更新标志
    } else {
        // 正在输出且缓冲长度没变（只改了频率/幅度/占空比）：只改 ARR，计数器不动，
        // 波形相位保持连续，不会重新从 0 相位开始
        tim->ARR = (uint16_t)(best_k - 1);
    }

    // 计算振幅，并配置DAC2
    *amp_code = (amplitude * DAC_MAX) / (UserParam.DDS_Factor * VREF); // （UserParam.DDS_Factor 是硬件设计放大倍数）

    // 计算DAC2的偏置对应的dac值,带偏移校准值
    float dac2_val = 2047.5f - (bias * DAC_MAX) / (UserParam.DDS_Factor * VREF) - (float)UserParam.DDS_Original;
    dac2_val = (dac2_val < 0) ? 0 : (dac2_val > DAC_MAX) ? DAC_MAX : dac2_val;
    if (HAL_DAC_SetValue(&hdac, DAC_CHANNEL_2, DAC_ALIGN_12B_R, (uint16_t)roundf(dac2_val)) != HAL_OK)
        return HAL_ERROR;
    if (HAL_DAC_Start(&hdac, DAC_CHANNEL_2) != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}

/**
 * @brief 启动（或保持）DAC DMA 输出
 * @param N: 本次要播放的采样点数
 * @return HAL状态
 * @note  所有波形函数最后都调用它，省掉 14 份重复的“启动 DMA + 启动定时器 + 失败回滚”。
 *        长度与正在播放的一致时直接返回：缓冲区已经原地更新好了，DMA 继续循环播放即可，
 *        这样改幅度/占空比时波形不会重新从相位 0 开始。
 */
static HAL_StatusTypeDef wavegen_commit(uint16_t N) {
    // 除噪声外都走静态缓冲区：必须同时关掉 DMA 中断，否则高频波形（如 50kHz、N=20）
    // 时 DMA 半传输/传输完成中断会以 200k 次/秒的频率空转，白占 20% CPU
    if (s_noise_on) { s_noise_on = 0; noise_dma_irq_disable(); }

    if (s_active_n == N) { return HAL_OK; }

    if (s_active_n != 0) {
        HAL_TIM_Base_Stop(&TIM_TRGO_HANDLE);
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        s_active_n = 0;
    }

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }

    s_active_n = N;
    return HAL_OK;
}

// ------------------- 波形生成函数实现 -------------------

HAL_StatusTypeDef WaveGen_Sin(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    for (uint16_t i = 0; i < N; i++) {
        float theta = 2 * (float)M_PI * i / N;
        float val = 2047.5f + amp_code * sinf(theta);
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_Square(float freq, float amplitude, float bias, float duty_cycle) {
    if (duty_cycle < 0 || duty_cycle > 1.0f) return HAL_ERROR;

    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    uint16_t high_samples = (uint16_t)roundf(N * duty_cycle);
    uint16_t high_level = (uint16_t)roundf(2047.5f + amp_code);
    uint16_t low_level = (uint16_t)roundf(2047.5f - amp_code);

    high_level = (high_level > DAC_MAX) ? DAC_MAX : high_level;
    low_level = (low_level < 0) ? 0 : low_level;

    for (uint16_t i = 0; i < N; i++) {
        dac1_buffer[i] = (i < high_samples) ? high_level : low_level;
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_Sawtooth(float freq, float amplitude, float bias, float duty_cycle) {
    // 原来 0% 和 100% 直接返回错误（界面上改到 100% 会“设了没反应”），这里改成钳位
    float duty = duty_cycle;
    if (duty < 0.01f) { duty = 0.01f; }
    if (duty > 0.99f) { duty = 0.99f; }

    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    // 折点必须落在 [1, N-1]：否则 (breakpoint-1) 可能为 0，出现 0/0 = NaN
    uint16_t breakpoint = (uint16_t)roundf((float)N * duty);
    if (breakpoint < 1) { breakpoint = 1; }
    if (breakpoint > (uint16_t)(N - 1)) { breakpoint = (uint16_t)(N - 1); }

    float total_amp_span = 2 * amp_code; // 从最低到最高的总跨度
    float rise_div = (breakpoint > 1) ? (float)(breakpoint - 1) : 1.0f;
    float fall_div = (float)(N - breakpoint - 1);
    if (fall_div < 1.0f) { fall_div = 1.0f; }

    for (uint16_t i = 0; i < N; i++) {
        float val;
        if (i < breakpoint) {
            // 上升沿
            val = 2047.5f - amp_code + ((float)i / rise_div) * total_amp_span;
        } else {
            // 下降沿
            val = 2047.5f + amp_code - (((float)(i - breakpoint)) / fall_div) * total_amp_span;
        }
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_HalfWaveRect(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    for (uint16_t i = 0; i < N; i++) {
        float theta = 2 * (float)M_PI * i / N;
        float sin_val = sinf(theta);
        // 负半周削波为0
        float val = 2047.5f + amp_code * ((sin_val > 0) ? sin_val : 0);
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_FullWaveRect(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    for (uint16_t i = 0; i < N; i++) {
        float theta = (float)M_PI * i / N;
        // 取绝对值，实现全波整流
        float val = 2047.5f + amp_code * fabsf(sinf(theta));
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_UpStep(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    const uint8_t NUM_STEPS = 8;
    uint16_t step_size = N / NUM_STEPS;
    float step_voltage = (2 * amp_code) / NUM_STEPS; // 每个台阶的电压增量

    for (uint16_t i = 0; i < N; i++) {
        uint8_t step_number = i / step_size;
        if (step_number >= NUM_STEPS) step_number = NUM_STEPS - 1;

        float val = 2047.5f - amp_code + step_number * step_voltage;
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_DownStep(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    const uint8_t NUM_STEPS = 8;
    uint16_t step_size = N / NUM_STEPS;
    float step_voltage = (2 * amp_code) / NUM_STEPS;

    for (uint16_t i = 0; i < N; i++) {
        uint8_t step_number = i / step_size;
        if (step_number >= NUM_STEPS) step_number = NUM_STEPS - 1;

        float val = 2047.5f + amp_code - step_number * step_voltage;
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_ExpRise(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    // 时间常数：tau 越小，增长越快；N/8 是经验值，可调整
    float tau = N / 8.0f;
    // 预计算归一化因子：避免 t=N 时 exp(t/tau) 过大导致溢出
    float norm_factor = expf(N / tau) - 1.0f;

    for (uint16_t i = 0; i < N; i++) {
        float t = (float)i;
        // 正确的指数增长模型：(exp(t/tau) - 1) / (exp(N/tau) - 1)，归一化到 [0, 1]
        float exp_val = (expf(t / tau) - 1.0f) / norm_factor;
        // 映射到 DAC 电压范围（从 MIN 到 MAX）
        float val = 2047.5f - amp_code + exp_val * 2 * amp_code;

        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_ExpDecay(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    // 时间常数：tau 越小，下降越快；N/8 是经验值，可调整
    float tau = N / 8.0f;
    // 预计算归一化因子
    float norm_factor = expf(N / tau) - 1.0f;

    for (uint16_t i = 0; i < N; i++) {
        float t = (float)i;
        // 正确的指数下降模型：1 - (exp(t/tau) - 1)/(exp(N/tau) - 1)，归一化到 [0, 1]
        float exp_val = 1.0f - (expf(t / tau) - 1.0f) / norm_factor;
        // 映射到 DAC 电压范围（从 MAX 到 MIN）
        float val = 2047.5f - amp_code + exp_val * 2 * amp_code;

        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_DC(float freq, float amplitude, float bias, float duty_cycle) {
    (void)freq; (void)amplitude; (void)duty_cycle;
    // DC信号不依赖频率，但CommonInit需要一个有效值，这里传入1000Hz
    uint16_t N;
    float amp_code;
    if (WaveGen_CommonInit(1000.0f, &N, 0, bias, &amp_code) != HAL_OK)
        return HAL_ERROR;

    // 直流时，DAC1应输出中点电压（1.65V），对应12位DAC码值为2047.5，取整为2048
    uint16_t dc_code = (uint16_t)roundf(2047.5f);  // 即2048

    // 填充整个缓冲区
    for (uint16_t i = 0; i < N; i++) {
        dac1_buffer[i] = dc_code;
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_MultiTone(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    // 谐波次数必须是整数（原先第三次谐波误用了校准系数 DDS_Factor，
    // 校准后该系数不是 3.0，会冒出非谐波杂散）
    float harmonics[] = {1.0f, 2.0f, 3.0f}; // 基波, 2次谐波, 3次谐波
    float ratios[] = {1.0f, 0.5f, 0.3f};   // 各次谐波的振幅比例
    uint8_t num_harmonics = sizeof(harmonics) / sizeof(harmonics[0]);

    // 计算总振幅比例，用于归一化，防止过载
    float total_ratio = 0;
    for (uint8_t i = 0; i < num_harmonics; i++) {
        total_ratio += ratios[i];
    }

    for (uint16_t i = 0; i < N; i++) {
        float theta = 2 * (float)M_PI * i / N;
        float combined_val = 0.0f;
        for (uint8_t h = 0; h < num_harmonics; h++) {
            combined_val += sinf(harmonics[h] * theta) * ratios[h];
        }
        // 归一化并转换为DAC码值
        float val = 2047.5f + (amp_code / total_ratio) * combined_val;
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_Sinc(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    for (uint16_t i = 0; i < N; i++) {
        // 将x范围映射到 -N/2 到 N/2，以在中心生成主瓣
        float x = (float)(i - N / 2) / (N / 4.0f); // N/4.0f 控制脉冲宽度
        float sinc_val;

        if (fabsf(x) < 1e-6) { // 避免除以零
            sinc_val = 1.0f;
        } else {
            sinc_val = sinf((float)M_PI * x) / ((float)M_PI * x);
        }

        float val = 2047.5f + amp_code * sinc_val;
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

HAL_StatusTypeDef WaveGen_Lorenz(float freq, float amplitude, float bias, float duty_cycle) {
    (void)duty_cycle;
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    for (uint16_t i = 0; i < N; i++) {
        // 将x范围映射到 -N/2 到 N/2
        float x = (float)(i - N / 2) / (N / 8.0f); // N/8.0f 控制钟形宽度

        // 洛伦兹函数: 1 / (1 + x^2)
        float lorenz_val = 1.0f / (1.0f + x * x);

        // 将0~1范围的lorenz_val映射到 -amp_code ~ +amp_code
        float val = 2047.5f - amp_code + lorenz_val * 2 * amp_code;
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    return wavegen_commit(N);
}

// ------------------- 噪声 -------------------
#define LCG_A 1664525
#define LCG_C 1013904223
#define LCG_M 0xFFFFFFFFU
static uint32_t lcg_seed = 0x12345678U;

/**
 * @brief 重新填充一段噪声样本
 * @param start: 起始下标
 * @param count: 样本数
 * @note  在 DMA 半传输/传输完成中断里调用，填充刚播放完的那一半，
 *        这样 DAC 输出的是连续不断的随机序列，而不是一段固定噪声反复重放
 */
static void noise_fill(uint16_t start, uint16_t count) {
    // 全整数实现：这是最低优先级中断里跑的，512 点 @1MHz 触发时每 256 点进一次，
    // 用浮点乘除会让中断耗时翻倍（uint32 取模同样是除法，LCG 天然按 2^32 回绕，直接乘加即可）
    uint32_t span = (uint32_t)(s_noise_hi - s_noise_lo);
    uint16_t lo = s_noise_lo;

    for (uint16_t i = 0; i < count; i++) {
        lcg_seed = LCG_A * lcg_seed + LCG_C;
        uint32_t r = (lcg_seed >> 16) & 0xFFFFU;
        dac1_buffer[start + i] = (uint16_t)(lo + (uint16_t)((r * span) >> 16));
    }
}

/**
 * @brief 打开 DAC 的 DMA 中断（噪声刷新用，只需要打开一次）
 */
static void noise_dma_irq_enable(void) {
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 15, 0);   // 最低优先级，不打扰控制环
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

static void noise_dma_irq_disable(void) {
    HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
}

HAL_StatusTypeDef WaveGen_Noise(float freq, float amplitude, float bias, float duty_cycle) {
    (void)freq; (void)duty_cycle;
    if (amplitude < MIN_AMPLITUDE || amplitude > MAX_AMPLITUDE) {
        return HAL_ERROR;
    }

    uint16_t N;
    float amp_code;
    // 噪声不看频率参数：固定用最长缓冲 + 最高触发率（1MHz），噪声带宽最宽
    if (WaveGen_CommonInit((float)TRIG_MAX_FREQ / (float)WAVEGEN_N_MAX, &N, amplitude, bias, &amp_code) != HAL_OK) {
        return HAL_ERROR;
    }

    // 噪声的直流偏置也要用上（原实现把 bias 传成了 0，偏置旋钮对噪声无效）
    float lo = 2047.5f - amp_code;
    float hi = 2047.5f + amp_code;
    s_noise_lo = (uint16_t)roundf((lo < 0.0f) ? 0.0f : lo);
    s_noise_hi = (uint16_t)roundf((hi > (float)DAC_MAX) ? (float)DAC_MAX : hi);

    lcg_seed = osKernelGetSysTimerCount();
    noise_fill(0, N);

    if (wavegen_commit(N) != HAL_OK) {
        s_noise_on = 0;
        return HAL_ERROR;
    }

    noise_dma_irq_enable();
    s_noise_on = 1;
    return HAL_OK;
}

// ------------------- 噪声刷新中断 -------------------
/* DAC 通道1 的 DMA 是循环模式：半传输时前半段已播完，可以重填；传输完成时重填后半段 */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle) {
    (void)hdac_handle;
    if (!s_noise_on) { return; }
    noise_fill(0, (uint16_t)(s_active_n / 2));
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle) {
    (void)hdac_handle;
    if (!s_noise_on) { return; }
    uint16_t half = (uint16_t)(s_active_n / 2);
    noise_fill(half, (uint16_t)(s_active_n - half));
}

/**
 * @brief DAC 通道1 的 DMA 中断入口
 * @note  只用到 DMA1_Stream5（DAC1），其它 DMA1 流在工程里未使用，
 *        所以放在这里不会和 CubeMX 生成的 stm32f4xx_it.c 冲突
 */
void DMA1_Stream5_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_dac1);
}

// ------------------- 停止波形生成函数实现 -------------------
void WaveGen_Stop(void) {
    s_noise_on = 0;
    noise_dma_irq_disable();
    s_active_n = 0;
    s_cur_n = 0;
    HAL_TIM_Base_Stop(&TIM_TRGO_HANDLE);
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
    HAL_DAC_Stop(&hdac, DAC_CHANNEL_1);
    HAL_DAC_Stop(&hdac, DAC_CHANNEL_2);
}
