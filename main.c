#include "led.h"
#include "delay.h"
#include "OLED.h"
#include "max30102.h"

// --- 全局变量 ---
uint8_t temp[6];
uint32_t ir_val;
static uint32_t ir_max = 0, ir_min = 200000;
static uint32_t dynamic_threshold = 0;
static uint8_t  pre_beat_flag = 0; 
static uint8_t  finger_status = 0; 
static uint16_t beat_count = 0;
static uint32_t timer_count = 0;

// --- 绘图与防抖变量 ---
static uint32_t last_beat_time = 0; 
static uint32_t current_time_ms = 0;
static uint8_t  x_pos = 0;
static uint8_t  last_y = 40;

int main(void)
{
    // 1. 初始化
    delay_init(72);	  
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); 
    LED_Init();		  
    OLED_Init();
    OLED_Clear();
    
    OLED_ShowString(0, 0, "Link-C Project", 16, 1);
    MAX30102_Init();
    delay_ms(100);

    while(1)
    {
        current_time_ms += 20; // 累加时间刻度
        
        // 2. 读取数据
        max30102_FIFO_ReadBytes(REG_FIFO_DATA, temp);
        ir_val = (long)((long)((long)temp[3] & 0x03)<<16) |(long)temp[4]<<8 | (long)temp[5];

        if(ir_val > 55000) // 手指在位检测
        {
            if(finger_status == 0) { OLED_Clear(); finger_status = 1; }
            
            // --- 动态算法与心跳捕捉 ---
            if(ir_val > ir_max) ir_max = ir_val;
            if(ir_val < ir_min) ir_min = ir_val;
            if(timer_count % 10 == 0) { 
                dynamic_threshold = (ir_max + ir_min) / 2;
                ir_max = ir_val; ir_min = ir_val; 
            }

            // 带 300ms 死区保护的捕捉逻辑
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

            // --- 实时绘图逻辑 ---
            // 将 ir_val 映射到屏幕 20-60 像素高度
            uint8_t current_y = 60 - (uint8_t)((float)(ir_val - ir_min) / (ir_max - ir_min + 1) * 35);
            if(current_y > 63) current_y = 63;
            if(current_y < 20) current_y = 20;

            // 使用 DrawPoint 替代 DrawLine，兼容性更好
            OLED_DrawPoint(x_pos, current_y, 1);
            x_pos++;

            if(x_pos >= 127) {
                x_pos = 0;
                OLED_Clear(); // 满屏后清空重画
            }

            // --- 3 秒显示一次准确心率 ---
            if(timer_count++ > 150) 
            {
                uint16_t final_hr = beat_count * 20; 
                OLED_ShowString(0, 0, "HR:    BPM", 16, 1);
                if(final_hr >= 45 && final_hr <= 180) {
                    OLED_ShowNum(24, 0, final_hr, 3, 16, 1);
                } else {
                    OLED_ShowString(24, 0, "---", 16, 1);
                }
                beat_count = 0;
                timer_count = 0;
            }
        }
        else // 手指离开
        {
            if(finger_status == 1) { OLED_Clear(); finger_status = 0; }
            OLED_ShowString(0, 0, "Link-C Standby", 16, 1);
            OLED_ShowString(0, 2, "Searching...", 16, 1);
            x_pos = 0; timer_count = 0; beat_count = 0;
            GPIO_SetBits(GPIOC, GPIO_Pin_13);
        }
        
        delay_ms(20); 
    }
}