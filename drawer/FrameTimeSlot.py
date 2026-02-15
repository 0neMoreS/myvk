import re
import matplotlib.pyplot as plt


def parse_frame_times(txt_path):
    gpu_times_ms = []
    cpu_times_ms = []

    pattern = re.compile(r"^(GPU|CPU) Headless frame time:\s*([0-9.]+)\s*ms\s*$")

    with open(txt_path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line:
                continue
            match = pattern.match(line)
            if not match:
                continue
            kind, value = match.group(1), float(match.group(2))
            if kind == "GPU":
                gpu_times_ms.append(value)
            else:
                cpu_times_ms.append(value)

    return gpu_times_ms, cpu_times_ms


def plot_frame_times(gpu_times_ms, cpu_times_ms):
    if len(gpu_times_ms) != len(cpu_times_ms):
        raise ValueError("GPU and CPU data length mismatch")

    frame_indices = list(range(1, len(gpu_times_ms) + 1))

    plt.figure(figsize=(7, 4), dpi=120)
    plt.plot(frame_indices, gpu_times_ms, marker="o", markersize=3, linewidth=1.2, label="GPU")
    plt.plot(frame_indices, cpu_times_ms, marker="o", markersize=3, linewidth=1.2, label="CPU")

    values = gpu_times_ms + cpu_times_ms
    sorted_values = sorted(values)
    p95_index = int(0.95 * (len(sorted_values) - 1)) if sorted_values else 0
    p95_value = sorted_values[p95_index] if sorted_values else 1.0
    y_max = max(0.5, p95_value * 1.2)

    plt.title("Headless Frame Time", fontsize=10)
    plt.xlabel("Frame index", fontsize=9)
    plt.ylabel("Frame time (ms)", fontsize=9)
    plt.ylim(bottom=0, top=y_max)
    plt.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.7)
    plt.tick_params(axis="both", which="major", labelsize=8)
    plt.legend(fontsize=8)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    txt_path = "./drawer/headless_frame_time.txt"
    gpu_ms, cpu_ms = parse_frame_times(txt_path)
    if not gpu_ms or not cpu_ms:
        raise SystemExit("No valid data found in txt file")
    plot_frame_times(gpu_ms, cpu_ms)
