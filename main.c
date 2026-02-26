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
// --- 新增防抖变量 ---
    static uint32_t last_beat_time = 0; // 记录上一次心跳的时间点
    static uint32_t current_time_ms = 0;

    while(1)
    {
        current_time_ms += 20; // 对应 delay_ms(20)
        
        max30102_FIFO_ReadBytes(REG_FIFO_DATA, temp);
        ir_val = (long)((long)((long)temp[3] & 0x03)<<16) |(long)temp[4]<<8 | (long)temp[5];

        if(ir_val > 55000) // 基础手指检测阈值
        {
            if(finger_status == 0) { OLED_Clear(); finger_status = 1; }
            
            // 动态阈值更新逻辑
            if(ir_val > ir_max) ir_max = ir_val;
            if(ir_val < ir_min) ir_min = ir_val;
            if(timer_count % 10 == 0) { 
                dynamic_threshold = (ir_max + ir_min) / 2;
                ir_max = ir_val; ir_min = ir_val; 
            }

            // --- 核心改进：带死区的波峰捕捉 ---
            // 只有当超过阈值，且距离上一次心跳超过 300ms 时才计一次数 (对应最高 200BPM)
            if(ir_val > dynamic_threshold && pre_beat_flag == 0) 
            {
                if(current_time_ms - last_beat_time > 300) 
                {
                    beat_count++;
                    last_beat_time = current_time_ms;
                    GPIO_ResetBits(GPIOC, GPIO_Pin_13); // 蓝灯闪烁
                }
                pre_beat_flag = 1; 
            }
            if(ir_val < dynamic_threshold) 
            {
                pre_beat_flag = 0; 
                GPIO_SetBits(GPIOC, GPIO_Pin_13); 
            }

            // --- 每 3 秒计算一次心率 (150 * 20ms = 3000ms) ---
            if(timer_count++ > 150) 
            {
                uint16_t current_hr = beat_count * 20; // 3秒数据乘以20
                OLED_ShowString(0, 0, "Link-C: Stabilized", 16, 1);
                
                // 简单的平滑滤波：如果这次算出来太离谱，就显示 ---
                if(current_hr >= 50 && current_hr <= 160) {
                    OLED_ShowString(0, 4, "Heart:     BPM ", 16, 1);
                    OLED_ShowNum(55, 4, current_hr, 3, 16, 1);
                } else {
                    OLED_ShowString(55, 4, "Heart: ANALYZING", 16, 1);
                }
                beat_count = 0;
                timer_count = 0;
            }
        }
        else 
        {
            finger_status = 0; timer_count = 0; beat_count = 0;
            OLED_ShowString(0, 0, "Link-C: Standby   ", 16, 1);
            OLED_ShowString(0, 2, "Searching...      ", 16, 1);
            GPIO_SetBits(GPIOC, GPIO_Pin_13);
        }
        delay_ms(20);
    }
}
