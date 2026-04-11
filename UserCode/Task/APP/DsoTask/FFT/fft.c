#include "fft.h"
#include <string.h>
#include <math.h>

#include "UserDefineManage.h"

// ==================== 内部静态参数 ====================
// 中部1024点截取起始索引：(2881-1024)/2 = 928.5 → 向下取整928
#define MID_START_IDX   928U

// ==================== 外部静态缓冲区（避免动态内存开销） ====================
static float fft_real[FFT_N];    // FFT实部缓冲区（1024点）
static float fft_imag[FFT_N];    // FFT虚部缓冲区（1024点）
static float fft_amp_sq[FFT_N/2];// 幅度平方缓冲区（0~FFT_N/2-1，512点）

// ==================== 内部核心函数实现 ====================

/**
 * @brief  基2时间抽取FFT（修正版：正确计算旋转因子与蝶形运算）
 * @param  x: 实部指针（输入输出）
 * @param  y: 虚部指针（输入输出）
 * @param  n: FFT点数，必须是2的幂次
 */
static void FFT_Base2(float *x, float *y, int n)
{
    int i, j, k, step;
    float cos_w, sin_w;
    float temp_real, temp_imag, mul_real, mul_imag;

    // 1. 位倒序重排
    j = 0;
    for (i = 0; i < n; i++)
    {
        if (i < j)
        {
            // 交换实部和虚部
            float tx = x[i]; x[i] = x[j]; x[j] = tx;
            float ty = y[i]; y[i] = y[j]; y[j] = ty;
        }
        int m = n / 2;
        while (j >= m && m > 0)
        {
            j -= m;
            m /= 2;
        }
        j += m;
    }

    // 2. 蝶形运算（基2时间抽取，正确旋转因子 W_k^j = e^(-j*2π*j/k)）
    for (k = 2; k <= n; k <<= 1)  // k为每级蝶形宽度
    {
        step = k / 2;
        for (i = 0; i < n; i += k)  // i为蝶形组起始索引
        {
            for (j = 0; j < step; j++)  // j为蝶形对偏移
            {
                // 计算旋转因子
                float angle = -2.0f * (float)M_PI * (float)j / (float)k;
                cos_w = cosf(angle);
                sin_w = sinf(angle);

                // 取出蝶形第二个节点并乘以旋转因子
                temp_real = x[i + j + step];
                temp_imag = y[i + j + step];
                mul_real = temp_real * cos_w + temp_imag * sin_w;
                mul_imag = temp_imag * cos_w - temp_real * sin_w;

                // 完成蝶形运算
                x[i + j + step] = x[i + j] - mul_real;
                y[i + j + step] = y[i + j] - mul_imag;
                x[i + j] += mul_real;
                y[i + j] += mul_imag;
            }
        }
    }
}

/**
 * @brief  应用汉宁窗（减少频谱泄漏，提升主频点辨识度）
 * @param  x: 输入输出数据指针
 * @param  n: 数据长度
 */
static void Apply_Hanning_Window(float *x, int n)
{
    for (int i = 0; i < n; i++)
    {
        float window = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * (float)i / (float)(n - 1));
        x[i] *= window;
    }
}

/**
 * @brief  二次抛物线插值（用幅度拟合，带数值稳定性保护）
 * @param  amp_sq: 幅度平方数组
 * @param  max_k: 初始最大频点索引
 * @param  n_half: FFT点数的一半
 * @retval 校正后的频点索引（带小数偏移）
 */
static float Quadratic_Interpolation(float *amp_sq, int max_k, int n_half)
{
    // 1. 边界检查：若max_k在两端，不插值
    if (max_k <= 0 || max_k >= n_half - 1)
        return (float)max_k;

    // 2. 取三点幅度（关键：用sqrt(amp_sq)代替amp_sq，拟合更准确）
    float A = sqrtf(amp_sq[max_k - 1]);
    float B = sqrtf(amp_sq[max_k]);
    float C = sqrtf(amp_sq[max_k + 1]);

    // 3. 数值稳定性保护1：检查分母是否过小
    float denominator = A - 2.0f * B + C;
    if (fabsf(denominator) < 1e-6f || (fabsf(B) > 1e-6f && fabsf(denominator) < 0.01f * fabsf(B)))
    {
        return (float)max_k;
    }

    // 4. 数值稳定性保护2：确保B是真正的峰值（比两侧大10%以上）
    if (B < 1.1f * A || B < 1.1f * C)
    {
        return (float)max_k;
    }

    // 5. 计算偏移量δ
    float delta = 0.5f * (A - C) / denominator;

    // 6. 物理范围限制：delta必然在[-0.5, 0.5]之间
    if (delta > 0.5f) delta = 0.5f;
    if (delta < -0.5f) delta = -0.5f;

    return (float)max_k + delta;
}

// ==================== 对外接口实现 ====================
float Get_Main_AC_Freq(const uint8_t *raw_sorted_buf, uint32_t sample_rate)
{
    int i;
    float max_amp_sq = 0.0f;
    int max_k = 0;
    float freq_res;
    float corrected_k;

    // 1. 清空缓冲区
    memset(fft_real, 0, sizeof(fft_real));
    memset(fft_imag, 0, sizeof(fft_imag));
    memset(fft_amp_sq, 0, sizeof(fft_amp_sq));

    // 2. 从2881点原始排序数据中截取中部1024点并转换为float
    for (i = 0; i < FFT_N; i++)
        fft_real[i] = (float)raw_sorted_buf[MID_START_IDX + i];

    // 3. 去除直流分量
    float dc_offset = 127.0f - (float) UserParam.OSC_Original;
    for (i = 0; i < FFT_N; i++)
        fft_real[i] -= dc_offset;

    // 4. 应用汉宁窗（减少频谱泄漏）
    Apply_Hanning_Window(fft_real, FFT_N);

    // 5. 执行FFT（虚部初始化为0）
    FFT_Base2(fft_real, fft_imag, FFT_N);

    // 6. 计算幅度平方（跳过直流分量k=0）
    for (i = 1; i < FFT_N / 2; i++)
        fft_amp_sq[i] = fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i];

    // 7. 寻找能量最大的交流频点
    for (i = 1; i < FFT_N / 2; i++)
    {
        if (fft_amp_sq[i] > max_amp_sq)
        {
            max_amp_sq = fft_amp_sq[i];
            max_k = i;
        }
    }

    // 8. 二次插值校正频率（提升精度）
    corrected_k = Quadratic_Interpolation(fft_amp_sq, max_k, FFT_N / 2);

    // 9. 计算并返回最终频率
    freq_res = (float)sample_rate / (float)FFT_N;
    return corrected_k * freq_res;
}