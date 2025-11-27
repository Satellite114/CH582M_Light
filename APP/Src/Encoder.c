/********************************** (C) COPYRIGHT *******************************
 * File Name          : Encoder.c
 * Author             : User
 * Version            : V1.0
 * Date               : 2025/11/27
 * Description        : 旋转编码器驱动程序
 *******************************************************************************/

#include "Encoder.h"

// 私有变量
static volatile Encoder_Status encoder_status = {0};
static volatile uint8_t last_state = 0;
static volatile uint32_t pulse_count_per_second = 0;
static volatile uint32_t last_pulse_time = 0;

// 速度阈值定义 (脉冲/秒)
#define SPEED_THRESHOLD_SLOW    10   // < 10 脉冲/秒 = 慢速
#define SPEED_THRESHOLD_MEDIUM  50   // 10-50 脉冲/秒 = 中速
#define SPEED_THRESHOLD_FAST    50   // > 50 脉冲/秒 = 快速

// 超时时间 (毫秒)
#define ENCODER_TIMEOUT_MS      200  // 200ms无脉冲则认为停止

/*********************************************************************
 * @fn      Encoder_Init
 *
 * @brief   初始化编码器
 *
 * @return  none
 */
void Encoder_Init(void)
{
    // 配置PA8(Encoder B)和PA9(Encoder A)为上拉输入
    GPIOA_ModeCfg(ENCODER_A_PIN | ENCODER_B_PIN, GPIO_ModeIN_PU);
    
    // 配置PB0(SW按钮)为上拉输入
    GPIOB_ModeCfg(ENCODER_SW_PIN, GPIO_ModeIN_PU);
    
    // 读取初始状态
    last_state = 0;
    if(GPIOA_ReadPortPin(ENCODER_A_PIN)) last_state |= 0x01;
    if(GPIOA_ReadPortPin(ENCODER_B_PIN)) last_state |= 0x02;
    
    // 初始化状态结构体
    encoder_status.count = 0;
    encoder_status.dir = ENCODER_DIR_NONE;
    encoder_status.speed = ENCODER_SPEED_STOP;
    encoder_status.rpm = 0;
    encoder_status.sw_pressed = false;
    encoder_status.last_update = 0;
    
    pulse_count_per_second = 0;
    last_pulse_time = 0;
    
    // 配置PA8和PA9的GPIO中断（上升沿和下降沿触发）
    GPIOA_ITModeCfg(ENCODER_A_PIN | ENCODER_B_PIN, GPIO_ITMode_FallEdge | GPIO_ITMode_RiseEdge);
    PFIC_EnableIRQ(GPIO_A_IRQn);
    
    // 启用SysTick用于时间测量
    SysTick_Config(GetSysClock() / 1000); // 1ms中断
}

/*********************************************************************
 * @fn      Encoder_UpdateSpeed
 *
 * @brief   更新编码器速度
 *
 * @return  none
 */
static void Encoder_UpdateSpeed(void)
{
    uint32_t current_time = SysTick->CNT;
    
    // 计算时间间隔 (毫秒)
    uint32_t delta_time = encoder_status.last_update > 0 ? 
                         (encoder_status.last_update - current_time) / (GetSysClock() / 1000) : 0;
    
    if(delta_time > ENCODER_TIMEOUT_MS) {
        // 超时，认为停止
        encoder_status.speed = ENCODER_SPEED_STOP;
        encoder_status.rpm = 0;
        encoder_status.dir = ENCODER_DIR_NONE;
        pulse_count_per_second = 0;
    } else if(delta_time > 0) {
        // 计算RPM (脉冲/秒)
        encoder_status.rpm = pulse_count_per_second;
        
        // 根据RPM判断速度等级
        if(encoder_status.rpm < SPEED_THRESHOLD_SLOW) {
            encoder_status.speed = ENCODER_SPEED_SLOW;
        } else if(encoder_status.rpm < SPEED_THRESHOLD_MEDIUM) {
            encoder_status.speed = ENCODER_SPEED_MEDIUM;
        } else {
            encoder_status.speed = ENCODER_SPEED_FAST;
        }
    }
}

/*********************************************************************
 * @fn      Encoder_IRQHandler
 *
 * @brief   编码器GPIO中断处理函数
 *
 * @return  none
 */
void Encoder_IRQHandler(void)
{
    uint8_t current_state = 0;
    int8_t delta = 0;
    static const int8_t encoder_table[16] = {
        0, -1, 1, 0,  // 00 -> 00, 01, 10, 11
        1, 0, 0, -1,  // 01 -> 00, 01, 10, 11
        -1, 0, 0, 1,  // 10 -> 00, 01, 10, 11
        0, 1, -1, 0   // 11 -> 00, 01, 10, 11
    };
    
    // 读取当前状态
    if(GPIOA_ReadPortPin(ENCODER_A_PIN)) current_state |= 0x01;
    if(GPIOA_ReadPortPin(ENCODER_B_PIN)) current_state |= 0x02;
    
    // 使用查表法确定方向
    delta = encoder_table[(last_state << 2) | current_state];
    
    if(delta != 0) {
        // 更新计数
        encoder_status.count += delta;
        
        // 更新方向
        encoder_status.dir = (delta > 0) ? ENCODER_DIR_CW : ENCODER_DIR_CCW;
        
        // 计算速度
        uint32_t current_time = SysTick->CNT;
        if(last_pulse_time > 0) {
            uint32_t time_diff = (last_pulse_time - current_time) / (GetSysClock() / 1000000); // 微秒
            if(time_diff > 0) {
                pulse_count_per_second = 1000000 / time_diff; // 转换为每秒脉冲数
            }
        }
        last_pulse_time = current_time;
        encoder_status.last_update = current_time;
        
        // 更新速度等级
        Encoder_UpdateSpeed();
    }
    
    last_state = current_state;
    
    // 清除GPIO中断标志
    GPIOA_ClearITFlagBit(ENCODER_A_PIN | ENCODER_B_PIN);
}

/*********************************************************************
 * @fn      Encoder_GetCount
 *
 * @brief   获取编码器累计计数
 *
 * @return  计数值
 */
int32_t Encoder_GetCount(void)
{
    return encoder_status.count;
}

/*********************************************************************
 * @fn      Encoder_ResetCount
 *
 * @brief   重置编码器计数
 *
 * @return  none
 */
void Encoder_ResetCount(void)
{
    encoder_status.count = 0;
}

/*********************************************************************
 * @fn      Encoder_GetDirection
 *
 * @brief   获取编码器旋转方向
 *
 * @return  方向
 */
Encoder_Direction Encoder_GetDirection(void)
{
    // 检查是否超时
    uint32_t current_time = SysTick->CNT;
    uint32_t delta_time = encoder_status.last_update > 0 ? 
                         (encoder_status.last_update - current_time) / (GetSysClock() / 1000) : ENCODER_TIMEOUT_MS + 1;
    
    if(delta_time > ENCODER_TIMEOUT_MS) {
        return ENCODER_DIR_NONE;
    }
    
    return encoder_status.dir;
}

/*********************************************************************
 * @fn      Encoder_GetSpeed
 *
 * @brief   获取编码器旋转速度等级
 *
 * @return  速度等级
 */
Encoder_Speed Encoder_GetSpeed(void)
{
    // 检查是否超时
    uint32_t current_time = SysTick->CNT;
    uint32_t delta_time = encoder_status.last_update > 0 ? 
                         (encoder_status.last_update - current_time) / (GetSysClock() / 1000) : ENCODER_TIMEOUT_MS + 1;
    
    if(delta_time > ENCODER_TIMEOUT_MS) {
        return ENCODER_SPEED_STOP;
    }
    
    return encoder_status.speed;
}

/*********************************************************************
 * @fn      Encoder_GetRPM
 *
 * @brief   获取编码器转速（脉冲/秒）
 *
 * @return  转速
 */
uint32_t Encoder_GetRPM(void)
{
    // 检查是否超时
    uint32_t current_time = SysTick->CNT;
    uint32_t delta_time = encoder_status.last_update > 0 ? 
                         (encoder_status.last_update - current_time) / (GetSysClock() / 1000) : ENCODER_TIMEOUT_MS + 1;
    
    if(delta_time > ENCODER_TIMEOUT_MS) {
        return 0;
    }
    
    return encoder_status.rpm;
}

/*********************************************************************
 * @fn      Encoder_IsButtonPressed
 *
 * @brief   检查按钮是否按下
 *
 * @return  true=按下, false=未按下
 */
bool Encoder_IsButtonPressed(void)
{
    // 读取PB0引脚状态，低电平表示按下（上拉输入）
    return (GPIOB_ReadPortPin(ENCODER_SW_PIN) == 0);
}

/*********************************************************************
 * @fn      Encoder_GetStatus
 *
 * @brief   获取编码器完整状态
 *
 * @param   status - 状态结构体指针
 *
 * @return  none
 */
void Encoder_GetStatus(Encoder_Status *status)
{
    if(status != NULL) {
        // 更新按钮状态
        encoder_status.sw_pressed = Encoder_IsButtonPressed();
        
        // 检查超时
        uint32_t current_time = SysTick->CNT;
        uint32_t delta_time = encoder_status.last_update > 0 ? 
                             (encoder_status.last_update - current_time) / (GetSysClock() / 1000) : ENCODER_TIMEOUT_MS + 1;
        
        if(delta_time > ENCODER_TIMEOUT_MS) {
            encoder_status.speed = ENCODER_SPEED_STOP;
            encoder_status.rpm = 0;
            encoder_status.dir = ENCODER_DIR_NONE;
        }
        
        // 拷贝状态
        *status = encoder_status;
    }
}

/*********************************************************************
 * @fn      Encoder_Test
 *
 * @brief   编码器测试函数
 *
 * @return  none
 */
void Encoder_Test(void)
{
    uint8_t test_buffer[100];
    Encoder_Status status;
    int32_t last_count = 0;
    
    // 初始化编码器
    Encoder_Init();
    
    UART1_SendString((uint8_t*)"\r\n=== Encoder Test Start ===\r\n", 31);
    UART1_SendString((uint8_t*)"Rotate the encoder and press the button...\r\n", 46);
    UART1_SendString((uint8_t*)"Hardware Config:\r\n", 19);
    UART1_SendString((uint8_t*)"  Encoder A: PA9\r\n", 20);
    UART1_SendString((uint8_t*)"  Encoder B: PA8\r\n", 20);
    UART1_SendString((uint8_t*)"  Button SW: PB0\r\n\r\n", 23);
    
    while(1) {
        // 获取编码器状态
        Encoder_GetStatus(&status);
        
        // 检查计数是否变化
        if(status.count != last_count) {
            // 格式化输出
            int len = sprintf((char*)test_buffer, 
                            "Count: %ld | Dir: %s | Speed: %s | RPM: %lu\r\n",
                            status.count,
                            status.dir == ENCODER_DIR_CW ? "CW(\u987a\u65f6\u9488)" : 
                            (status.dir == ENCODER_DIR_CCW ? "CCW(\u9006\u65f6\u9488)" : "NONE"),
                            status.speed == ENCODER_SPEED_STOP ? "STOP" :
                            (status.speed == ENCODER_SPEED_SLOW ? "SLOW" :
                            (status.speed == ENCODER_SPEED_MEDIUM ? "MEDIUM" : "FAST")),
                            status.rpm);
            
            UART1_SendString(test_buffer, len);
            last_count = status.count;
        }
        
        // 检查按钮状态
        if(status.sw_pressed) {
            UART1_SendString((uint8_t*)">>> Button PRESSED! Count Reset. <<<\r\n", 41);
            Encoder_ResetCount();
            last_count = 0;
            
            // 防抖动延时
            DelayMs(200);
        }
        
        // 小延时，减少CPU负载
        DelayMs(50);
    }
}

/*********************************************************************
 * @fn      GPIO_A_IRQHandler
 *
 * @brief   GPIOA中断处理函数
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void GPIO_A_IRQHandler(void)
{
    // 调用编码器中断处理
    Encoder_IRQHandler();
}