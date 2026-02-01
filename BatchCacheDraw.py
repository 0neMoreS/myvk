import subprocess
import time
import re
import matplotlib.pyplot as plt
import pyautogui
import os
import sys

# ================= 配置区域 =================
# C++ 程序路径
EXE_PATH = r".\bin\main.exe"

# 测试的 GPU 名称
# DEVICE_NAME = "NVIDIA GeForce RTX 5080 Laptop GPU"
DEVICE_NAME = "Intel(R) Graphics"

# 测试范围：从 index=3 开始，到 index=3000，步长为 30
# 步长越小，阶梯图越精细，但测试时间越长
START_INDEX = 3
END_INDEX = 3000
STEP = 12  # 建议步长稍微错开一点，或者设为 1 以获得最精确结果

# ===========================================

def parse_output(line):
    """
    从输出行中提取数字。
    预期格式: "VS invocations: 123"
    """
    match = re.search(r"VS invocations:\s*(\d+)", line)
    if match:
        return int(match.group(1))
    return None

def run_benchmark():
    indices_drawn = []
    shader_invocations = []

    print(f"Starting benchmark on {DEVICE_NAME}...")
    print(f"Range: {START_INDEX} to {END_INDEX} (Step: {STEP})")

    # 循环测试不同的 Index Buffer 大小
    for index_count in range(START_INDEX, END_INDEX, STEP):
        
        # 构造命令
        # 注意：这里假设你的程序接受 --index 参数
        cmd = [
            EXE_PATH,
            "--physical-device", DEVICE_NAME,
            "--index", str(index_count)
        ]

        try:
            # 启动进程
            # stdout=subprocess.PIPE 让我们可以读取输出
            print(f"Running command: {' '.join(cmd)}")

            process = subprocess.Popen(
                cmd, 
                stdout=subprocess.PIPE, 
                stderr=subprocess.STDOUT, # 建议把错误输出也合并进来，防止报错你看不到
                text=True, 
                cwd=os.getcwd(),
                bufsize=1  # <--- 关键修改：设置为行缓冲 (Line Buffered)
            )

            print(f"Process started with PID: {process.pid}")

            # 等待程序初始化 (根据你的程序启动速度调整)
            time.sleep(2) 

            # 模拟按下 Tab 键触发查询
            # 注意：这需要你的程序窗口处于前台焦点
            # pyautogui.press('tab')

            invocation_count = None

            print("Waiting for output...")

            # 读取输出
            while True:
                # 设定超时防止死循环 (这里简单处理，实际可加 timeout)
                if process.poll() is not None:
                    break
                
                line = process.stdout.readline()
                if line:
                    print(f"Output: {line.strip()}") # 调试用
                    val = parse_output(line)
                    if val is not None:
                        invocation_count = val
                        break # 拿到数据就可以退出了
            
            print(f"Process completed for index {index_count}.")
            
            # 记录数据
            if invocation_count is not None:
                indices_drawn.append(index_count)
                shader_invocations.append(invocation_count)
                print(f"Index: {index_count} -> VS Invocations: {invocation_count}")
            
            # 杀死进程，准备下一轮
            process.terminate()
            try:
                process.wait(timeout=1)
            except subprocess.TimeoutExpired:
                process.kill()

            time.sleep(2)

        except Exception as e:
            print(f"Error at index {index_count}: {e}")
            # 确保进程被关闭
            if 'process' in locals():
                process.kill()

    return indices_drawn, shader_invocations

def plot_results(x, y, device_name):
    """
    绘制阶梯图
    """
    plt.figure(figsize=(6, 4), dpi=120) # 设置画布大小和清晰度
    
    # 关键：使用 step 函数绘制阶梯状线条
    # where='post' 表示阶梯在点之后变化，符合这种累计/批次效应的视觉逻辑
    plt.step(x, y, where='post', color='blue', linewidth=1.5)
    
    # 设置样式
    plt.title(f"Vertex Shader Invocations\n({device_name})", fontsize=10)
    plt.xlabel("Indices drawn", fontsize=9)
    plt.ylabel("Shader invocations", fontsize=9)
    
    # 设置网格
    plt.grid(True, which='both', linestyle='--', linewidth=0.5, alpha=0.7)
    
    # 设置刻度字体大小
    plt.tick_params(axis='both', which='major', labelsize=8)

    # 强制让 X 轴和 Y 轴从 0 开始（如果需要的话）
    plt.ylim(bottom=0)
    plt.xlim(left=0)
    
    # 自动调整布局
    plt.tight_layout()
    
    # 保存图片
    filename = f"benchmark_result_{device_name.replace(' ', '_')}.png"
    plt.savefig(filename)
    print(f"Graph saved to {filename}")
    
    # 显示图片
    plt.show()

if __name__ == "__main__":
    # 为了防止 pyautogui 报错（防止鼠标移动到角落触发安全机制）
    pyautogui.FAILSAFE = False
    
    x_data, y_data = run_benchmark()
    
    if x_data:
        plot_results(x_data, y_data, DEVICE_NAME)
    else:
        print("No data collected.")