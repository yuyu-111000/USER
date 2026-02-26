#include "led.h"
#include "delay.h"
#include "OLED.h"
#include "max30102.h"

// --- 全局变量声明 ---
uint8_t temp[6];
uint32_t ir_val;

// --- 算法核心变量 (使用 static 保证在循环中数值不丢失) ---
static uint32_t ir_max = 0, ir_min = 200000;
static uint32_t dynamic_threshold = 0;
static uint8_t  pre_beat_flag = 0; 
static uint8_t  finger_status = 0; // 修复：定义手指状态变量
static uint16_t beat_count = 0;
static uint32_t timer_count = 0;

int main(void)
{
    // 1. 硬件初始化
    delay_init(72);	  
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); 
    LED_Init();		  
    OLED_Init();
    OLED_Clear();
    
    OLED_ShowString(0, 0, "Link-C v3.0", 16, 1);
    MAX30102_Init();
    delay_ms(100);

    while(1)
    {
        // 2. 读取传感器原始数据
        max30102_FIFO_ReadBytes(REG_FIFO_DATA, temp);
        ir_val = (long)((long)((long)temp[3] & 0x03)<<16) |(long)temp[4]<<8 | (long)temp[5];

        // 3. 动态手指检测与心跳捕捉逻辑
        if(ir_val > 50000) 
        {
            // 如果是刚检测到手指，清屏并切换状态
            if(finger_status == 0) { 
                OLED_Clear(); 
                finger_status = 1; 
            }
            
            // --- 动态自适应算法 ---
            if(ir_val > ir_max) ir_max = ir_val;
            if(ir_val < ir_min) ir_min = ir_val;
            
            // 每 10 次采样更新一次动态阈值
            if(timer_count % 10 == 0) { 
                dynamic_threshold = (ir_max + ir_min) / 2;
                ir_max = ir_val; 
                ir_min = ir_val; 
            }

            // 捕捉脉搏：当数值冲过动态阈值时计一次数
            if(ir_val > dynamic_threshold && pre_beat_flag == 0) 
            {
                beat_count++;
                pre_beat_flag = 1; 
                GPIO_ResetBits(GPIOC, GPIO_Pin_13); // 蓝灯闪烁反馈
            }
            if(ir_val < dynamic_threshold) 
            {
                pre_beat_flag = 0; 
                GPIO_SetBits(GPIOC, GPIO_Pin_13); 
            }

            // 4. 每 1 秒刷新一次屏幕数字 (50 * 20ms = 1s)
            if(timer_count++ > 50) 
            {
                uint16_t current_hr = beat_count * 60; 
                OLED_ShowString(0, 0, "Link-C: Sensing", 16, 1);
                OLED_ShowString(0, 4, "Heart:     BPM", 16, 1);
                
                if(current_hr >= 45 && current_hr <= 180) { // 限制合理心率区间
                    OLED_ShowNum(55, 4, current_hr, 3, 16, 1);
                } else {
                    OLED_ShowString(55, 4, "---", 16, 1);
                }
                beat_count = 0;
                timer_count = 0;
            }
        }
        else 
        {
            // 未检测到手指时重置所有状态
            if(finger_status == 1) {
                OLED_Clear();
                finger_status = 0;
            }
            timer_count = 0;
            beat_count = 0;
            OLED_ShowString(0, 0, "Link-C: Standby", 16, 1);
            OLED_ShowString(0, 2, "Put Finger On! ", 16, 1);
            GPIO_SetBits(GPIOC, GPIO_Pin_13); // 关灯
        }
        
        delay_ms(20); // 50Hz 采样率
    }
}