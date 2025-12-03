#include "main.h"
#include "oled.h" // 包含您提供的 OLED 库
#include <string.h> // 用于 sprintf 和 strcpy

/* --- 1. 全局变量定义 --- */
volatile Music_PlayState g_music_state = STATE_STOPPED;
volatile uint32_t g_system_millis = 0;
volatile uint32_t g_current_time_ms = 0;
volatile uint16_t g_delay_counter = 0;
volatile uint8_t  g_oled_update_flag = 1; // 1: 提示主循环更新OLED

/* --- 私有变量 --- */
static uint32_t music_index = 0;
static uint32_t last_oled_update_millis = 0;
static uint32_t oled_update_interval = 250; // 毫秒

/**
  * @brief  规范化GPIO配置
  * - 蜂鸣器 (AF_PP): PA8, PA0, PA6, PB6
  * - 按键 (IPU): PA1, PA2
  * - *OLED引脚 (PB8, PB9) 将由 OLED_Init() 自行配置*
  * - *PA11, PA12, PA13, PA14 均未启用*
  */
void Peripherals_GPIO_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能 GPIOA, GPIOB 和 AFIO 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* 1. 蜂鸣器引脚 (AF_PP) */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_0 | GPIO_Pin_6;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 2. 按键引脚 (Input Pull-up) */
    /* PA1 -> Key 1 (Start/Stop) */
    /* PA2 -> Key 2 (Pause/Play) */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    /* * 注意: PB8 和 PB9 (OLED) 将在 OLED_Init() 中被配置为 GPIO_Mode_Out_OD 
     * 我们不需要在这里配置它们
     */
}

/**
  * @brief  配置4个蜂鸣器定时器 (与您V2版本一致)
  */
void Buzzer_TIM_Config(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    /* --- 配置 TIM1 (APB2, 72MHz) --- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    TIM_TimeBaseStructure.TIM_Prescaler = 71; 
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 1000; 
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0; 
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable; 
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure); 
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE); 
    TIM_Cmd(TIM1, ENABLE);

    /* --- 配置 TIM2 (APB1, 36MHz) --- */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseStructure.TIM_Prescaler = 35; 
    TIM_TimeBaseStructure.TIM_Period = 1000; 
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &TIM_OCInitStructure); 
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
    
    /* --- 配置 TIM3 (APB1, 36MHz) --- */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); 
    TIM_OC1Init(TIM3, &TIM_OCInitStructure); 
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
    
    /* --- 配置 TIM4 (APB1, 36MHz) --- */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure); 
    TIM_OC1Init(TIM4, &TIM_OCInitStructure); 
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);
    TIM_Cmd(TIM4, ENABLE);
}

/**
  * @brief  设置蜂鸣器频率 (V2版本，已修复BUG)
  */
void Set_Buzzer_Freq(uint8_t buzzer_index, uint16_t freq) {
    TIM_TypeDef* tim;
    switch (buzzer_index) {
        case 0: tim = TIM1; break;
        case 1: tim = TIM2; break;
        case 2: tim = TIM3; break;
        case 3: tim = TIM4; break;
        default: return;
    }
    if (freq == 0) {
        TIM_CCxCmd(tim, TIM_Channel_1, TIM_CCx_Disable);
    } else {
        uint32_t period_ticks_32 = 1000000 / freq; 
        if (period_ticks_32 < 2) period_ticks_32 = 2;
        if (period_ticks_32 > 65536) period_ticks_32 = 65536;
        uint16_t arr = (uint16_t)(period_ticks_32 - 1);
        uint16_t ccr = (uint16_t)(period_ticks_32 / 2);
        if (ccr == 0) ccr = 1; 
        TIM_SetAutoreload(tim, arr);
        TIM_SetCompare1(tim, ccr);
        TIM_CCxCmd(tim, TIM_Channel_1, TIM_CCx_Enable);
    }
}

/**
 * @brief 关闭所有蜂鸣器 (用于暂停或停止)
 */
void All_Buzzers_Off(void) {
    Set_Buzzer_Freq(0, 0);
    Set_Buzzer_Freq(1, 0);
    Set_Buzzer_Freq(2, 0);
    Set_Buzzer_Freq(3, 0);
}

/**
  * @brief  音乐Systick处理 (在 stm32f10x_it.c 中被调用)
  */
void Music_SysTick_Handler(void) {
    /* 累加全局系统时钟 */
    g_system_millis++;
    
    /* 只有在播放时才处理音乐逻辑 */
    if (g_music_state == STATE_PLAYING) {
        if (g_delay_counter > 0) {
            g_delay_counter--;
        }
        g_current_time_ms++; // 累加播放时间
    }
}

/**
  * @brief  音乐播放主循环 (在 main() 中被调用)
  */
void Play_Music_Loop(void) {
    /* 1. 检查状态 */
    if (g_music_state != STATE_PLAYING) {
        /* 如果不是播放状态 (暂停或停止)，确保蜂鸣器是关的 */
        All_Buzzers_Off();
        return;
    }
    
    /* 2. 检查延时 */
    if (g_delay_counter == 0) {
        
        /* 3. 检查是否播放完毕 */
        if (music_index >= MUSIC_DATA_LENGTH) {
            g_music_state = STATE_STOPPED; // 播放完毕，进入停止状态
            music_index = 0;
            g_current_time_ms = 0;
            g_oled_update_flag = 1; // 更新OLED显示为 "停止"
            return;
        }

        /* 4. 获取下一个事件 */
        const MusicEvent* event = &music_data[music_index];

        Set_Buzzer_Freq(0, event->freq[0]);
        Set_Buzzer_Freq(1, event->freq[1]);
        Set_Buzzer_Freq(2, event->freq[2]);
        Set_Buzzer_Freq(3, event->freq[3]);
        
        g_delay_counter = event->delay_ms;
        music_index++;

        if(g_delay_counter == 0) {
             g_delay_counter = 1; // 强制最小延时 1ms
        }
    }
}

/* --- 4. 按键处理 --- */
#define DEBOUNCE_TIME_MS 50 // 50ms 消抖

void Poll_Keys(void) {
    static uint32_t key1_press_time = 0;
    static uint32_t key2_press_time = 0;
    static uint8_t key1_state = 1; // 1 = 释放 (上拉)
    static uint8_t key2_state = 1; // 1 = 释放 (上拉)
    uint8_t key1_pressed = (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == 0);
    uint8_t key2_pressed = (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2) == 0);

    /* --- 处理按键1 (PA1) -> Start/Stop --- */
    if (key1_pressed && key1_state == 1 && (g_system_millis - key1_press_time > DEBOUNCE_TIME_MS)) {
        // 触发 Start/Stop
        if (g_music_state == STATE_PLAYING || g_music_state == STATE_PAUSED) {
            g_music_state = STATE_STOPPED;
            music_index = 0;
            g_current_time_ms = 0;
        } else {
            g_music_state = STATE_PLAYING;
            music_index = 0; // 重新开始
            g_current_time_ms = 0;
            g_delay_counter = 1; // 立即加载第一个音符
        }
        g_oled_update_flag = 1; // 立即更新OLED
        key1_press_time = g_system_millis;
        key1_state = 0; // 标记为已按下
    } else if (!key1_pressed) {
        key1_state = 1; // 标记为已释放
    }
    
    /* --- 处理按键2 (PA2) -> Pause/Play --- */
    if (key2_pressed && key2_state == 1 && (g_system_millis - key2_press_time > DEBOUNCE_TIME_MS)) {
        // 触发 Pause/Play
        if (g_music_state == STATE_PLAYING) {
            g_music_state = STATE_PAUSED;
        } else if (g_music_state == STATE_PAUSED) {
            g_music_state = STATE_PLAYING;
        }
        // (如果已停止，按键2无效)
        
        g_oled_update_flag = 1; // 立即更新OLED
        key2_press_time = g_system_millis;
        key2_state = 0; // 标记为已按下
    } else if (!key2_pressed) {
        key2_state = 1; // 标记为已释放
    }
}

/* --- 5. OLED 显示 (已适配您的OLED驱动) --- */

/**
 * @brief 使用字符型OLED驱动更新屏幕
 * @note 您的驱动OLED_ShowString使用 行(1-4) 和 列(1-16)
 */
void OLED_Update_Screen(void) {
    char buffer[17]; // 16个字符 + '\0'
    
    OLED_Clear(); 

    /* 第1行: 标题 (16 chars) */
    /* "Hero-mili(8 bit)" 17 chars, 太长了 */
    OLED_ShowString(1, 1, "Hero-mili 8bit ");

    /* 第2行: 作者 (12 chars, 居中) */
    OLED_ShowString(2, 3, "somesterdude");

    /* 第3行: 时间 (13 chars, 居中) " 00:00 / 00:00 " */
    uint32_t total_sec = MUSIC_TOTAL_TIME_MS / 1000;
    uint32_t current_sec = g_current_time_ms / 1000;
    sprintf(buffer, " %02lu:%02lu / %02lu:%02lu ",
            current_sec / 60, current_sec % 60,
            total_sec / 60, total_sec % 60);
    OLED_ShowString(3, 1, buffer); // 从第1列开始

    /* 第4行: 状态 + 字符进度条 (15 chars) */
    /* 格式: "> [....=.....]" (1 + 1 + 10 + 1 = 13 chars) */
    
    /* 1. 设置状态图标 */
    if (g_music_state == STATE_PLAYING) {
        strcpy(buffer, "||"); // 播放
    } else if (g_music_state == STATE_PAUSED) {
        strcpy(buffer, "> "); // 暂停
    } else {
        strcpy(buffer, "[]"); // 停止
    }
    
    /* 2. 计算进度条 (10格) */
    uint8_t bar_width = 10;
    uint8_t progress_percent = 0;
    if (MUSIC_TOTAL_TIME_MS > 0) {
        progress_percent = (g_current_time_ms * 100) / MUSIC_TOTAL_TIME_MS;
    }
    if (progress_percent > 100) progress_percent = 100;
    
    uint8_t fill_bars = (progress_percent * bar_width) / 100;
    
    /* 3. 绘制进度条字符串 */
    strcat(buffer, " ["); // "|| [" 或 ">  ["
    int i;
    for (i = 0; i < bar_width; i++) {
        if (i < fill_bars) {
            strcat(buffer, "=");
        } else {
            strcat(buffer, "."); // 用 '.' 填充空白
        }
    }
    strcat(buffer, "]"); // "|| [====....]"
    
    /* 4. 填充剩余空格 (确保覆盖整行) */
    while (strlen(buffer) < 16) {
        strcat(buffer, " ");
    }
    
    OLED_ShowString(4, 1, buffer);
}


/* --- 6. 主函数 --- */

int main(void) {
    /* 配置SysTick 1ms中断 */
    if (SysTick_Config(SystemCoreClock / 1000)) {
        while (1); /* 配置失败 */
    }
    
    /* 1. 初始化硬件 */
    Peripherals_GPIO_Config(); // 初始化 GPIO (蜂鸣器, 按键)
    Buzzer_TIM_Config();     // 初始化 TIM (蜂鸣器PWM)
    
    /* 2. 初始化 OLED (使用您驱动中的函数) */
    OLED_Init();
    
    /* 3. 初始化播放器状态 */
    g_music_state = STATE_STOPPED;
    g_current_time_ms = 0;
    music_index = 0;
    g_delay_counter = 0;
    g_oled_update_flag = 1; // 强制首次更新OLED
    last_oled_update_millis = 0;

    while (1) {
        /* a. 处理按键输入 */
        Poll_Keys();
        
        /* b. 处理音乐播放 */
        Play_Music_Loop();
        
        /* c. 处理OLED显示 (节流) */
        if (g_oled_update_flag || (g_system_millis - last_oled_update_millis > oled_update_interval))
        {
            if (g_music_state == STATE_PLAYING) {
                 oled_update_interval = 250; // 播放时刷新快，进度条流畅
            } else {
                 oled_update_interval = 1000; // 暂停或停止时刷新慢
            }
            
            OLED_Update_Screen();
            g_oled_update_flag = 0; // 清除标志
            last_oled_update_millis = g_system_millis;
        }
    }
}
