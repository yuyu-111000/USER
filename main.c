#include "led.h"
#include "delay.h"
#include "OLED.h"
#include "max30102.h"

// --- 全局变量声明 ---
uint8_t temp[6];
uint32_t ir_val;
static uint32_t ir_max = 0, ir_min = 200000;
static uint32_t dynamic_threshold = 0;
static uint8_t  pre_beat_flag = 0; 
static uint8_t  finger_status = 0; 
static uint16_t beat_count = 0;
static uint32_t timer_count = 0;

static uint32_t last_beat_time = 0; 
static uint32_t current_time_ms = 0;
static uint8_t  x_pos = 0;
static uint8_t  last_y = 40;

int main(void)
{
    // 1. 硬件初始化
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
        current_time_ms += 20; // 模拟时间轴
        
        // 2. 读取传感器原始数据
        max30102_FIFO_ReadBytes(REG_FIFO_DATA, temp);
        ir_val = (long)((long)((long)temp[3] & 0x03)<<16) |(long)temp[4]<<8 | (long)temp[5];

        // 3. 【核心修复】手指检测开关
        if(ir_val > 55000) 
        {
            if(finger_status == 0) { 
                OLED_Clear(); 
                finger_status = 1; 
                x_pos = 0; // 重置绘图起点
            }
            
            // --- A. 动态阈值与脉搏捕捉 ---
            if(ir_val > ir_max) ir_max = ir_val;
            if(ir_val < ir_min) ir_min = ir_val;
            if(timer_count % 10 == 0) { 
                dynamic_threshold = (ir_max + ir_min) / 2;
                ir_max = ir_val; ir_min = ir_val; 
            }

            // 捕捉脉搏（带 300ms 死区保护防止重计）
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

            // --- B. 丝滑波形绘制逻辑 ---
            uint8_t current_y = 60 - (uint8_t)((float)(ir_val - ir_min) / (ir_max - ir_min + 1) * 35);
            if(current_y > 63) current_y = 63;
            if(current_y < 20) current_y = 20;

            // 连线绘图（消除散点）
            OLED_DrawLine(x_pos == 0 ? 0 : x_pos - 1, last_y, x_pos, current_y, 1);
            last_y = current_y;

            // 扫描式清理（防止卡顿）
            for(uint8_t i = 20; i < 64; i++) {
                OLED_DrawPoint(x_pos + 1, i, 0); 
                OLED_DrawPoint(x_pos + 2, i, 0); 
            }

            x_pos++;
            if(x_pos >= 127) x_pos = 0;

            // --- C. 3秒定时显示心率数字 ---
            if(timer_count++ > 150) 
            {
                uint16_t final_hr = beat_count * 20; 
                OLED_ShowString(0, 0, "HR:    BPM", 16, 1);
                if(final_hr >= 45 && final_hr <= 180) {
                    OLED_ShowNum(24, 0, final_hr, 3, 16, 1);
                }
                beat_count = 0;
                timer_count = 0;
            }
        }
        else // 未检测到手指
        {
            if(finger_status == 1) { OLED_Clear(); finger_status = 0; }
            OLED_ShowString(0, 0, "Link-C Standby", 16, 1);
            OLED_ShowString(0, 2, "Put Finger On!", 16, 1);
            x_pos = 0; timer_count = 0; beat_count = 0;
            GPIO_SetBits(GPIOC, GPIO_Pin_13);
        }
        
        delay_ms(20); 
    }
}