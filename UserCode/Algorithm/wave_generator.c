#include "wave_generator.h"
#include "dac.h"
#include "tim.h"
#include <math.h>
#include "UserDefineManage.h"

// DMA缓冲区，所有波形共享
uint16_t dac1_buffer[MAX_SAMPLE];


// ------------------- 内部静态函数声明 -------------------
static HAL_StatusTypeDef WaveGen_CommonInit(float freq, uint16_t* N, float amplitude, float bias, float* amp_code);
static uint32_t find_best_divisor(uint32_t K);

// ------------------- 内部函数实现 -------------------

/**
 * @brief 通用初始化函数，用于所有波形的前置配置
 * @param freq: 频率
 * @param N: 计算出的采样点数
 * @param amplitude: 振幅
 * @param bias: 偏置
 * @param amp_code: 计算出的振幅DAC值
 * @return HAL状态
 */
static HAL_StatusTypeDef WaveGen_CommonInit(float freq, uint16_t* N, float amplitude, float bias, float* amp_code) {
    // 入参合法性检查
    if (freq <= 0 || freq > 50000 ||
        amplitude < MIN_AMPLITUDE || amplitude > MAX_AMPLITUDE ||
        bias < MIN_BIAS || bias > MAX_BIAS) {
        return HAL_ERROR;
    }

    // 计算采样点数 N
    *N = 0;
    // 从最大采样点开始向下查找合适的N
    for (int k = (int)log2(MAX_SAMPLE); k >= 4; k--) { // k from log2(128)=7 down to 4 (N=16)
        uint16_t candidate = 1 << k; // 2^k
        if (candidate * freq <= TRIG_MAX_FREQ) {
            *N = candidate;
            break;
        }
    }
    if (*N == 0) return HAL_ERROR; // 未找到合适的N  <= 一般不会出现

    // 计算&配置定时器参数 (PSC, ARR)
    uint32_t f_trig = (uint32_t)roundf(*N * freq);
    if (f_trig == 0 || f_trig > TRIG_MAX_FREQ) return HAL_ERROR;

    uint32_t K = (uint32_t)roundf(TIM_FREQ / f_trig);
    uint16_t PSC = 0, ARR = 0;

    if (K <= 65536) {
        PSC = 0;
        ARR = (uint16_t)(K - 1);
    } else {
        uint32_t a = find_best_divisor(K);
        if (a == 0) return HAL_ERROR; // 分解失败
        PSC = (uint16_t)(a - 1);
        ARR = (uint16_t)((K / a) - 1);
    }

    // 设定定时器配置
    HAL_TIM_Base_Stop(&TIM_TRGO_HANDLE);    // 先停止定时器
    TIM_TRGO_HANDLE.Instance->PSC = PSC;    // 配置分频系数
    TIM_TRGO_HANDLE.Instance->ARR = ARR;    // 配置重装载值
    TIM_TRGO_HANDLE.Instance->CNT = 0;      // 清零计数器

    // 计算振幅&和偏置，并配置DAC2
    *amp_code = (amplitude * DAC_MAX) / (UserParam.DDS_Factor * VREF); // （UserParam.DDS_Factor 是硬件设计放大倍数）

    // 计算DAC2的偏置对应的dac值,带偏移校准值
    float dac2_val = 2047.5f - (bias * DAC_MAX) / (UserParam.DDS_Factor * VREF) - (float)UserParam.DDS_Original;
    dac2_val = (dac2_val < 0) ? 0 : (dac2_val > DAC_MAX) ? DAC_MAX : dac2_val;
    if (HAL_DAC_SetValue(&hdac, DAC_CHANNEL_2, DAC_ALIGN_12B_R, (uint16_t)roundf(dac2_val)) != HAL_OK)
        return HAL_ERROR;
    if (HAL_DAC_Start(&hdac, DAC_CHANNEL_2) != HAL_OK)
        return HAL_ERROR;

    // 停止当前可能正在运行的DMA
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);

    return HAL_OK;
}

/**
 * @brief 查找K的最佳因子分解 (PSC+1) * (ARR+1) = K
 * @param K: 需分解的值
 * @return 因子 a (PSC+1 = a)，若找不到返回0
 */
static uint32_t find_best_divisor(uint32_t K) {
    // 从平方根开始向下查找，以找到最接近平方根的因子
    uint32_t max_divisor = (uint32_t)sqrtf((float)K);
    for (uint32_t a = max_divisor; a >= 1; a--) {
        if (K % a == 0) {
            uint32_t b = K / a;
            if (a <= 65535 && b <= 65535) {
                return a; // 返回较大的因子作为PSC+1
            }
        }
    }
    return 0; // 未找到符合条件的因子
}

// ------------------- 波形生成函数实现 -------------------

HAL_StatusTypeDef WaveGen_Sin(float freq, float amplitude, float bias, float duty_cycle) {
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    for (uint16_t i = 0; i < N; i++) {
        float theta = 2 * (float)M_PI * i / N;
        float val = 2047.5f + amp_code * sinf(theta);
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
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

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_Sawtooth(float freq, float amplitude, float bias, float duty_cycle) {
    if (duty_cycle <= 0 || duty_cycle >= 1.0f) return HAL_ERROR;

    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    uint16_t breakpoint = (uint16_t)roundf(N * duty_cycle);
    float total_amp_span = 2 * amp_code; // 从最低到最高的总跨度

    for (uint16_t i = 0; i < N; i++) {
        float val;
        if (i < breakpoint) {
            // 上升沿
            val = 2047.5f - amp_code + ((float)i / (breakpoint - 1)) * total_amp_span;
        } else {
            // 下降沿
            val = 2047.5f + amp_code - (((float)(i - breakpoint)) / (N - breakpoint - 1)) * total_amp_span;
        }
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_HalfWaveRect(float freq, float amplitude, float bias, float duty_cycle) {
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

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_FullWaveRect(float freq, float amplitude, float bias, float duty_cycle) {
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    for (uint16_t i = 0; i < N; i++) {
        float theta = (float)M_PI * i / N;
        // 取绝对值，实现全波整流
        float val = 2047.5f + amp_code * fabsf(sinf(theta));
        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_UpStep(float freq, float amplitude, float bias, float duty_cycle) {
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

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_DownStep(float freq, float amplitude, float bias, float duty_cycle) {
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

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_ExpRise(float freq, float amplitude, float bias, float duty_cycle) {
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    // 时间常数：tau 越小，增长越快；N/8 是经验值，可调整
    float tau = N / 8.0f;
    // 预计算归一化因子：避免 t=N 时 exp(t/tau) 过大导致溢出
    float norm_factor = exp(N / tau) - 1.0f;

    for (uint16_t i = 0; i < N; i++) {
        float t = (float)i;
        // 正确的指数增长模型：(exp(t/tau) - 1) / (exp(N/tau) - 1)，归一化到 [0, 1]
        float exp_val = (exp(t / tau) - 1.0f) / norm_factor;
        // 映射到 DAC 电压范围（从 MIN 到 MAX）
        float val = 2047.5f - amp_code + exp_val * 2 * amp_code;

        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_ExpDecay(float freq, float amplitude, float bias, float duty_cycle) {
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    // 时间常数：tau 越小，下降越快；N/8 是经验值，可调整
    float tau = N / 8.0f;
    // 预计算归一化因子
    float norm_factor = exp(N / tau) - 1.0f;

    for (uint16_t i = 0; i < N; i++) {
        float t = (float)i;
        // 正确的指数下降模型：1 - (exp(t/tau) - 1)/(exp(N/tau) - 1)，归一化到 [0, 1]
        float exp_val = 1.0f - (exp(t / tau) - 1.0f) / norm_factor;
        // 映射到 DAC 电压范围（从 MAX 到 MIN）
        float val = 2047.5f - amp_code + exp_val * 2 * amp_code;

        dac1_buffer[i] = (uint16_t)roundf(val);
        dac1_buffer[i] = (dac1_buffer[i] > DAC_MAX) ? DAC_MAX : (dac1_buffer[i] < 0) ? 0 : dac1_buffer[i];
    }

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_DC(float freq, float amplitude, float bias, float duty_cycle) {
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

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK)
        return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_MultiTone(float freq, float amplitude, float bias, float duty_cycle) {
    uint16_t N; float amp_code;
    if (WaveGen_CommonInit(freq, &N, amplitude, bias, &amp_code) != HAL_OK) return HAL_ERROR;

    // 定义谐波次数和对应的振幅比例
    float harmonics[] = {1.0f, 2.0f, UserParam.DDS_Factor}; // 基波, 2次谐波, 3次谐波
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

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_Sinc(float freq, float amplitude, float bias, float duty_cycle) {
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

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef WaveGen_Lorenz(float freq, float amplitude, float bias, float duty_cycle) {
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

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) return HAL_ERROR;
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }
    return HAL_OK;
}






#define LCG_A 1664525
#define LCG_C 1013904223
#define LCG_M 0xFFFFFFFFU
static uint32_t lcg_seed = 0x12345678U;

static float rand_float(void) {
    lcg_seed = (LCG_A * lcg_seed + LCG_C) % LCG_M;
    return (float)lcg_seed / (float)LCG_M;
}


HAL_StatusTypeDef WaveGen_Noise(float freq, float amplitude, float bias, float duty_cycle) {
    if (amplitude < MIN_AMPLITUDE || amplitude > MAX_AMPLITUDE) {
        return HAL_ERROR;
    }

    lcg_seed = osKernelGetSysTimerCount();

    uint16_t N;
    float amp_code;
    if (WaveGen_CommonInit(1000.0f, &N, amplitude, 0.0f, &amp_code) != HAL_OK) {
        return HAL_ERROR;
    }

    float noise_min = 2047.5f - amp_code;
    float noise_max = 2047.5f + amp_code;
    float noise_span = noise_max - noise_min;

    for (uint16_t i = 0; i < N; i++) {
        float rand_val = rand_float();
        float dac_val = noise_min + rand_val * noise_span;

        dac_val = (dac_val < 0) ? 0 : (dac_val > DAC_MAX) ? DAC_MAX : dac_val;
        dac1_buffer[i] = (uint16_t)roundf(dac_val);
    }

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)dac1_buffer, N, DAC_ALIGN_12B_R) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_TIM_Base_Start(&TIM_TRGO_HANDLE) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return HAL_ERROR;
    }

    return HAL_OK;
}

// ------------------- 停止波形生成函数实现 -------------------
void WaveGen_Stop(void) {

    HAL_TIM_Base_Stop(&TIM_TRGO_HANDLE);
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
    HAL_DAC_Stop(&hdac, DAC_CHANNEL_1);
    HAL_DAC_Stop(&hdac, DAC_CHANNEL_2);
}