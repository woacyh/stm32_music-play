import mido
import sys
import os
import math

# --- 内部类，用于存储事件 ---
class MusicEvent:
    def __init__(self, delay_ms, freqs):
        self.delay_ms = int(round(delay_ms))
        if self.delay_ms == 0:
             self.delay_ms = 1 # 确保最小延时为 1ms
        self.freqs = [int(round(f)) for f in freqs]

    def __repr__(self):
        return f"Delay: {self.delay_ms}ms, Freqs: {self.freqs}"

# MIDI音高编号转频率 (Hz)
def midi_to_freq(note):
    if note == 0:
        return 0
    # A4 = 69 = 440 Hz
    return 440 * (2 ** ((note - 69) / 12.0))

def process_midi(midi_file_path):
    print(f"正在加载 MIDI 文件: {midi_file_path}")
    try:
        mid = mido.MidiFile(midi_file_path)
    except Exception as e:
        print(f"错误: 无法加载MIDI文件: {e}")
        return

    # 1. 确定时间基准 (ms per tick)
    microseconds_per_beat = 500000 
    ticks_per_beat = mid.ticks_per_beat if mid.ticks_per_beat > 0 else 480
    
    # 查找第一个 tempo 设定 (V1 脚本的逻辑)
    for msg in mid.tracks[0]: 
        if msg.type == 'set_tempo':
            microseconds_per_beat = msg.tempo
            break
            
    microseconds_per_tick = microseconds_per_beat / ticks_per_beat
    ms_per_tick = microseconds_per_tick / 1000.0
    print(f"MIDI信息: Ticks per Beat = {ticks_per_beat}, us per Beat = {microseconds_per_beat}")
    print(f"Calculated: {ms_per_tick:.4f} ms per tick")

    # 2. 合并所有轨道，并按绝对时间排序 (V1 脚本的逻辑)
    merged_track = mido.merge_tracks(mid.tracks)
    
    events = []
    absolute_time_ticks = 0
    for msg in merged_track:
        absolute_time_ticks += msg.time
        # (修复：确保只处理tempo, note_on, note_off)
        if msg.type == 'set_tempo':
            # 处理运行中的 tempo 变化
            microseconds_per_beat = msg.tempo
            microseconds_per_tick = microseconds_per_beat / ticks_per_beat
            ms_per_tick = microseconds_per_tick / 1000.0
            print(f"注意: Tempo 变化于 tick {absolute_time_ticks}, new ms/tick = {ms_per_tick:.4f}")
        
        if msg.type == 'note_on' or msg.type == 'note_off':
            events.append((absolute_time_ticks, msg, ms_per_tick)) # 存下当前的 ms_per_tick

    # 3. 处理事件，生成4通道音符状态 (V1 脚本的逻辑)
    active_channels = [None] * 4 
    output_events = [] 
    last_event_time_ticks = 0
    
    for (time_ticks, msg, current_ms_per_tick) in events:
        
        delta_ticks = time_ticks - last_event_time_ticks
        
        if delta_ticks > 0:
            delay_ms = delta_ticks * current_ms_per_tick # 使用事件发生时的 tempo
            current_freqs = [midi_to_freq(n) if n is not None else 0 for n in active_channels]
            output_events.append(MusicEvent(delay_ms, current_freqs))

        # --- 更新当前事件的状态 ---
        is_note_on = (msg.type == 'note_on' and msg.velocity > 0)
        is_note_off = (msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0))
        
        if is_note_on:
            try:
                free_index = active_channels.index(None)
                active_channels[free_index] = msg.note
            except ValueError:
                print(f"警告: Time {time_ticks}: 通道全满, 音符 {msg.note} 被丢弃.")
                pass
                
        elif is_note_off:
            try:
                active_index = active_channels.index(msg.note)
                active_channels[active_index] = None
            except ValueError:
                pass
                
        last_event_time_ticks = time_ticks

    # 添加最后一个事件 (100ms 尾音 + 1ms 停止)
    current_freqs = [midi_to_freq(n) if n is not None else 0 for n in active_channels]
    output_events.append(MusicEvent(100, current_freqs))
    output_events.append(MusicEvent(1, [0,0,0,0]))
    
    # --- 4. (新!) 计算总时长 (适配 OLED C 代码) ---
    # 我们只加总到倒数第二个事件 (忽略最后的1ms停止事件)
    total_duration_ms = sum(event.delay_ms for event in output_events[:-1])
    
    
    # 5. 写入 .h 文件 (!!! 格式已修复 !!!)
    output_dir = os.path.dirname(midi_file_path)
    output_filename = os.path.join(output_dir, "music_data.h") # 直接覆盖
    
    with open(output_filename, 'w', encoding='utf-8') as f:
        f.write("/*\n")
        f.write(f" * 自动生成的音乐数据 (来自: {os.path.basename(midi_file_path)})\n")
        f.write(" * By Midi-to-STM32 Python Script (V3 - Event-Based)\n")
        f.write(" */\n\n")
        f.write("#ifndef __MUSIC_DATA_H\n")
        f.write("#define __MUSIC_DATA_H\n\n")
        
        f.write("#include \"stm32f10x.h\"\n\n") # <-- (已修复)
        
        # --- (新!) 写入总时长 ---
        f.write(f"/* 音乐总时长 (毫秒) */\n")
        f.write(f"#define MUSIC_TOTAL_TIME_MS (uint32_t){total_duration_ms}\n\n")

        # --- (已修复) 结构体定义 (适配 main.c) ---
        f.write("/* 音乐事件结构体 */\n")
        f.write("typedef struct {\n")
        f.write("    uint16_t freq[4];  /* 4个通道的频率 (Hz), 0=关闭 */\n")
        f.write("    uint16_t delay_ms; /* 此状态保持的毫秒数 */\n")
        f.write("} MusicEvent;\n\n")
        
        f.write(f"#define MUSIC_DATA_LENGTH {len(output_events)}\n\n")
        
        f.write("const MusicEvent music_data[MUSIC_DATA_LENGTH] = {\n")
        
        # --- (已修复) 数据格式 (适配 main.c) ---
        for event in output_events:
            freq_str = ", ".join(map(str, event.freqs))
            # 格式: { {freqs}, delay_ms }
            f.write(f"    {{ {{ {freq_str} }}, {event.delay_ms} }},\n")
            
        f.write("};\n\n")
        f.write("#endif /* __MUSIC_DATA_H */\n") # (修复了注释风格)

    print(f"\n处理完成!")
    print(f"总时长: {total_duration_ms / 1000.0:.2f} 秒")
    print(f"总共 {len(output_events)} 个音乐事件被生成.")
    print(f"输出文件: {output_filename}")


if __name__ == "__main__":
    print("--- STM32 4通道 8-Bit 音乐转换器 (V3 - 事件驱动高保真版) ---")
    
    try:
        midi_file_path = input("请输入 MIDI 文件的完整路径 (可拖拽文件到此处): ")
        
        # 移除路径中的引号 (如果用户拖放文件)
        midi_file_path = midi_file_path.strip().strip('\'" ')
        
        if not os.path.exists(midi_file_path):
            print(f"错误: 文件 '{midi_file_path}' 不存在。")
        else:
            process_midi(midi_file_path)
            
    except KeyboardInterrupt:
        print("\n操作已取消。")
    except Exception as e:
        print(f"\n发生意外错误: {e}")
    
    print("\n按 Enter 键退出...")
    input()