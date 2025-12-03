# STM32 4-Channel 8-Bit Music Player (Hero-mili) 🎵

> 基于 STM32F103 的多通道“8-bit 风格”音乐播放器。包含完整的硬件驱动与自研 MIDI 转换工具链。

![Status](https://img.shields.io/badge/Status-Active-brightgreen)
![Platform](https://img.shields.io/badge/Platform-STM32F103-blue)
![Language](https://img.shields.io/badge/Language-C%20%2F%20Python-orange)

## 📖 项目简介

本项目利用 STM32 的 4 个定时器通道，驱动 4 个无源蜂鸣器同时发声，实现了**复音（Polyphonic）**播放效果，完美还原红白机（NES）时代的 8-bit 音乐风格。

**核心亮点：**
* **下位机 (C)**: 基于 SysTick 的事件驱动引擎，支持 OLED 实时显示进度条与状态。
* **上位机 (Python)**: 自研脚本，可将 `.mid` 文件自动转换为 C 语言 `.h` 头文件。

## 🔌 硬件接线说明 (Pinout)

根据 `main.c` 配置，请严格按照下表连接：

| STM32 引脚 | 外设名称 | 功能描述 | 备注 |
| :--- | :--- | :--- | :--- |
| **PA8** | 蜂鸣器 1 | 通道 1 (TIM1_CH1)
| **PA0** | 蜂鸣器 2 | 通道 2 (TIM2_CH1)
| **PA6** | 蜂鸣器 3 | 通道 3 (TIM3_CH1)
| **PB6** | 蜂鸣器 4 | 通道 4 (TIM4_CH1)
| **PA1** | 按键 1 | 开始 / 停止 | 内部上拉 (IPU) |
| **PA2** | 按键 2 | 暂停 / 继续 | 内部上拉 (IPU) |
| **PB8/PB9** | OLED | I2C SCL/SDA | 视具体 OLED 驱动而定 |
| **GND** | 所有外设 | 共地 | 必须连接 |

## 📂 项目目录结构

```text
STM32-Music-Player/
├── User/
│   ├── main.c           # 主逻辑：状态机、多线程音频混音、OLED刷新
│   ├── music_data.h     # (自动生成) 存放音符频率数据的头文件
│   └── stm32f10x_it.c   # 中断服务函数
├── oled/                # OLED 屏幕驱动库
├── System/              # 系统延迟与时钟配置
├── Library/             # STM32 标准外设库
├── Tools/               # (建议新建) 存放 Python 转换脚本
│   └── midi2stm32.py    # MIDI 转 C 数组工具
├── project led.uvprojx  # Keil MDK 工程文件
└── README.md            # 项目说明文档


最后，本项目大部分由ai辅助完成，还有这个readme也是
