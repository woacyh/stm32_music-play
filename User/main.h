#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f10x.h"
#include "music_data.h" // 必须包含这个来获取总时长
#include <stdio.h>      // 用于 sprintf

/* --- 1. 播放器状态机 --- */
typedef enum {
    STATE_STOPPED,
    STATE_PLAYING,
    STATE_PAUSED
} Music_PlayState;

/* --- 2. 全局变量 (volatile 用于多线程/中断访问) --- */
extern volatile Music_PlayState g_music_state;
extern volatile uint32_t g_system_millis;      // 系统运行的毫秒数 (Systick提供)
extern volatile uint32_t g_current_time_ms;  // 歌曲播放的毫秒数
extern volatile uint16_t g_delay_counter;      // 音乐事件的延时计数器
extern volatile uint8_t  g_oled_update_flag;   // 1: 提示主循环更新OLED

/* --- 3. 音乐播放器原型 --- */
void Music_SysTick_Handler(void);
void Play_Music_Loop(void);
void Set_Buzzer_Freq(uint8_t buzzer_index, uint16_t freq);
void All_Buzzers_Off(void);

/* --- 4. 按键处理原型 --- */
void Poll_Keys(void);

/* --- 5. OLED 显示原型 --- */
void OLED_Update_Screen(void);

/* --- 6. 硬件初始化原型 --- */
void Peripherals_GPIO_Config(void);
void Buzzer_TIM_Config(void);
/* * 注意：不再需要 I2C_OLED_Config() 
 * 因为您提供的 OLED.c 中的 OLED_Init() 会自行初始化 PB8 和 PB9 
 */

#endif /* __MAIN_H */
