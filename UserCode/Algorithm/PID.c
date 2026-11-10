#include "PID.h"
#include <math.h>
#include <stdint.h>

#include "cmsis_os2.h"
#include "UserDefineManage.h"
#include "UserTask.h"

volatile float Target_Voltage = 5.0f;       // 目标电压
volatile float Target_Current = 2.0f;       // 目标电流

// 静态计数器：防抖+调节限速
static uint8_t cv_adjust_cnt = 0;          // 防抖计数器
static uint8_t cv_adjust_timer = 0;        // 调节限速计数器（记录中断次数）
static uint8_t cc_exit_delay_cnt = 0;
static float    Target_Current_Prev = 0.0f;    // 记录上一次CC模式下的设定值
#define CC_EXIT_DELAY_THRESHOLD 100        // 连续 100ms（100次PID调用）才确认退出
bool cc_extend_mode = false;               // CC下电流调整模式，0表减小，1表增大

/* ========================== 内部辅助函数 ========================== */

/**
 * @brief  目标电压 → DAC 设定值（含硬件限幅）
 * @note   DAC 与输出电压呈负相关（标定系数为负）：DAC 越小，输出电压越高。
 *         凡是由电压反推 DAC 的地方都走这里，避免各处限幅写法不一致。
 * @param  voltage: 目标电压（V，内部会钳到允许范围）
 * @retval 0~4095 的 DAC 值
 */
static uint16_t voltage_to_dac(float voltage) {
    if (voltage > MAX_OUTPUT_VOLTAGE) voltage = MAX_OUTPUT_VOLTAGE;
    if (voltage < MIN_OUTPUT_VOLTAGE) voltage = MIN_OUTPUT_VOLTAGE;

    int32_t dac = (int32_t) lroundf(UserParam.DPS_Voltage_DAC_Coefficient * voltage +
                                    UserParam.DPS_Voltage_DAC_Constant);
    if (dac < 0)    dac = 0;
    if (dac > 4095) dac = 4095;
    return (uint16_t) dac;
}

/**
 * @brief  恒流模式下 DAC 的下限：目标电压所对应的 DAC
 * @note  DAC 再往下走就等于把输出电压设定到目标值之上。负载突然断开时，
 *        电流环会一路把 DAC 往小推（等于把输出电压往大推），必须以这个下限兜住。
 */
static uint16_t cc_dac_floor(void) {
    return voltage_to_dac(Target_Voltage);
}

/**
 * @brief  过压回退阈值（相对目标电压的余量）
 * @note  取「目标电压的 5%」与「0.5V」中较大者：低压档位不会被标定误差误触发，高压档位也不会放得过宽。
 */
static float ovp_margin(void) {
    float margin = Target_Voltage * 0.05f;
    return (margin < 0.5f) ? 0.5f : margin;
}

/**
 * @brief  复位控制环状态：回到恒压模式，DAC 直接置于目标电压位置
 * @note  电源开启前调用，避免沿用上一次（可能是恒流模式压得很低）的 DAC 值。
 */
void PID_ResetOutput(void) {
    pid_controller.mode            = 0;
    pid_controller.current_dac     = voltage_to_dac(Target_Voltage);
    pid_controller.prev_error      = 0.0f;
    pid_controller.prev_prev_error = 0.0f;
    pid_controller.result          = 0.0f;
    cv_adjust_cnt                  = 0;
    cv_adjust_timer                = 0;
    cc_exit_delay_cnt              = 0;
    PowerMode                      = false;
    PID_InvalidateDacCache();      // 允许 PID 任务立刻把新值写进 MCP4725
}

void SetTargetVoltage(float Voltage) {
    // 目标电压限幅
    Voltage = fmaxf(fminf(Voltage, MAX_OUTPUT_VOLTAGE), MIN_OUTPUT_VOLTAGE);
    Target_Voltage = Voltage;

    // CV模式下立即重置基准DAC
    if (pid_controller.mode == 0) {
        pid_controller.current_dac = voltage_to_dac(Voltage);   // 根据校准数据计算基础DAC值
        cv_adjust_cnt = 0;        // 重置防抖计数
        cv_adjust_timer = 0;      // 重置限速计数
    }
    osDelay(20);
}

void SetTargetCurrent(float Current) {
    if (Current > Target_Current_Prev){cc_extend_mode = false;}
    else {cc_extend_mode = true;}
    Target_Current_Prev = Current;
    Target_Current = Current;
}

PID pid_controller = {0};

// DAC 缓存失效标志：置 1 时 PID 任务无条件写一次 MCP4725
volatile uint8_t pid_dac_force_write = 1;

/**
 * @brief 使 DAC 缓存失效，下一次 PID 运算会强制写一次 MCP4725
 * @note  外部（校准流程、上电初始化）直接改写 DAC 后必须调用
 */
void PID_InvalidateDacCache(void) {
    pid_dac_force_write = 1;
}

void PID_Init(void) {
    PID_InvalidateDacCache();                   // 初始化后强制写一次 DAC
    pid_controller.mode          = 0;           // 初始为 CV模式
    pid_controller.hysteresis_A  = 0.05f;       // 死区电流
    pid_controller.hysteresis_V  = 0.05f;       // 死区电压
    pid_controller.integral_max  = 200.0f;      // 积分项最大值
    pid_controller.integral_min  = -200.0f;     // 积分项最小值
    pid_controller.result        = 0.0f;        // CC模式PID增量
    // 初始化基准DAC（5V）
    pid_controller.current_dac   = voltage_to_dac(5.0f);
    pid_controller.prev_error     = 0.0f;
    pid_controller.prev_prev_error = 0.0f;
    // 初始化计数器
    cv_adjust_cnt = 0;
    cv_adjust_timer = 0;
    Target_Current_Prev = 0.0f;
}

uint16_t PID_Calculate(float measured_current, float measured_voltage) {
    const uint16_t dac_floor = cc_dac_floor();      // 目标电压对应的 DAC：输出电压不允许超过它

    // ========================================
    // 过压快速回退（保护环）
    // ========================================
    // 正常控制下 DAC 一直被钳在 dac_floor 之上，输出电压不可能超过目标值。
    // 一旦真的超了，说明环路异常（负载突卸后又灌入、DAC 掉电复位、外部串入等），
    // 立刻把 DAC 拉回目标电压位置并退出恒流模式，不等模式判定的 100ms 防抖。
    if (measured_voltage > (Target_Voltage + ovp_margin())) {
        pid_controller.current_dac     = dac_floor;
        pid_controller.prev_error      = 0.0f;
        pid_controller.prev_prev_error = 0.0f;
        pid_controller.result          = 0.0f;
        cv_adjust_cnt                  = 0;
        cv_adjust_timer                = 0;
        cc_exit_delay_cnt              = 0;
        if (pid_controller.mode != 0) {
            pid_controller.mode = 0;
            PowerMode = false;
        }
        return pid_controller.current_dac;
    }

    // 模式切换逻辑
    float cc_enter_threshold = Target_Current + pid_controller.hysteresis_A;  // 进入CC模式的电流 ： 目标电流 + 死区电流
    float cc_exit_threshold  = Target_Current - pid_controller.hysteresis_A;  // 退出CC模式的电流 ： 目标电流 - 死区电流
    cc_enter_threshold = fminf(cc_enter_threshold, MAX_OUTPUT_CURRENT);     // 限幅
    cc_exit_threshold  = fmaxf(cc_exit_threshold, MIN_OUTPUT_CURRENT);

    if (pid_controller.mode == 0) { // CV→CC
        if (measured_current >= cc_enter_threshold) {
            pid_controller.mode = 1;
            pid_controller.prev_error = 0.0f;
            pid_controller.prev_prev_error = 0.0f;
            pid_controller.result = 0.0f;
            PowerMode = true;
            cv_adjust_cnt = 0;
            cv_adjust_timer = 0;
        }
    } else { // CC→CV
        // 检测退出条件
        bool voltage_exceed = (measured_voltage >= (Target_Voltage + pid_controller.hysteresis_V));
        bool current_below_exit = (measured_current <= cc_exit_threshold);

        if (current_below_exit || voltage_exceed) {
            cc_exit_delay_cnt++;
            if (cc_exit_delay_cnt >= CC_EXIT_DELAY_THRESHOLD) {
                // 确认退出
                pid_controller.mode = 0;
                PowerMode = false;
                cv_adjust_cnt = 0;
                cv_adjust_timer = 0;
                pid_controller.current_dac = voltage_to_dac(Target_Voltage);
                cc_exit_delay_cnt = 0;  // 重置计数器
            }
        } else {
            // 条件不满足，立即清零计数器
            cc_exit_delay_cnt = 0;
        }
    }

    // PID计算逻辑
    uint16_t target_dac;

    if (pid_controller.mode == 0) {   // CV模式 —— 直接微调DAC
        // 用有符号中间量微调：current_dac 是 uint16_t，在它上面直接 -= 1 会在 0 处回绕成 65535
        int32_t dac_temp = (int32_t) pid_controller.current_dac;

        // 计算电压误差（实测 - 目标）
        float voltage_error = measured_voltage - Target_Voltage;

        // 误差分层判断
        if (fabs(voltage_error) > CV_MAX_ADJUST_ERROR) {
            // 情况1：误差超过±0.15V → 直接重置为基准值
            dac_temp = (int32_t) voltage_to_dac(Target_Voltage);
            cv_adjust_cnt = 0;        // 重置防抖计数
            cv_adjust_timer = 0;      // 重置限速计数
        } else if (fabs(voltage_error) > CV_VOLTAGE_TOLERANCE) {
            // 情况2：误差在±0.02~±0.15V之间 → 先防抖，再限速调节
            cv_adjust_cnt++;
            if (cv_adjust_cnt >= 2) { // 防抖计数达标
                // 限速计数器累加（1ms/次，累计到 CV_ADJUST_INTERVAL 才调节）
                cv_adjust_timer++;
                if (cv_adjust_timer >= CV_ADJUST_INTERVAL) {    // 这里暂定为0，如果你有过冲/震荡，可以将CV_ADJUST_INTERVAL调大
                    // 执行±1微调
                    if (voltage_error < -CV_VOLTAGE_TOLERANCE) {    //(DAC与输出电压呈现负相关)
                        dac_temp -= 1; // 电压偏低，DAC - 1
                    } else if (voltage_error > CV_VOLTAGE_TOLERANCE) {
                        dac_temp += 1; // 电压偏高，DAC + 1
                    }
                    cv_adjust_cnt = 0;        // 重置防抖计数
                    cv_adjust_timer = 0;      // 重置限速计数
                }
            }
        } else {
            // 情况3：误差≤±0.02V → 不调节，重置所有计数器
            cv_adjust_cnt = 0;
            cv_adjust_timer = 0;
        }

        // DAC硬件限幅
        if (dac_temp > 4095) dac_temp = 4095;
        if (dac_temp < 0)    dac_temp = 0;
        target_dac = (uint16_t) dac_temp;

    } else { // CC模式 ——  增量式PID计算
        float temp = cc_extend_mode ? 0.01f : -0.01f;
        float error = measured_current - Target_Current + temp;
        float increment = UserParam.DPS_Loop_P * (error - pid_controller.prev_error)
                        + UserParam.DPS_Loop_I * error
                        + UserParam.DPS_Loop_D * (error - 2.0f * pid_controller.prev_error + pid_controller.prev_prev_error);
        increment = fmaxf(fminf(increment, pid_controller.integral_max), pid_controller.integral_min);
        pid_controller.result = increment;

        int32_t dac_temp = (int32_t) pid_controller.current_dac + (int32_t) lroundf(increment);
        if (dac_temp > 4095) dac_temp = 4095;
        if (dac_temp < 0)    dac_temp = 0;

        // ★ 恒流模式的安全下限：只允许把 DAC 往上推（= 把输出电压往下压），
        //   绝不允许低于「目标电压对应的 DAC」——否则等于主动把输出电压设定到设定值之上。
        //   负载突然断开时电流瞬间归零，电流环会在几十毫秒内把 DAC 一路推到 0，
        //   对应输出电压接近 40V（标定曲线外推），这就是限流下卸负载电压飙到 30 多伏的根因。
        if (dac_temp < (int32_t) dac_floor) dac_temp = (int32_t) dac_floor;

        target_dac = (uint16_t) dac_temp;

        pid_controller.prev_prev_error = pid_controller.prev_error;
        pid_controller.prev_error = error;
    }

    // 存储本次DAC值，供下一次计算使用
    pid_controller.current_dac = target_dac;
    return target_dac;
}