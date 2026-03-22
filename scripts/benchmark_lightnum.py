import copy
import json
import math
import re
import subprocess
import sys
from pathlib import Path


LIGHT_COUNTS = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000]
GPU_TIME_PATTERN = re.compile(r"GPU Headless frame time:\s*([0-9]*\.?[0-9]+)\s*ms")
CPU_TIME_PATTERN = re.compile(r"CPU Headless frame time:\s*([0-9]*\.?[0-9]+)\s*ms")


def load_scene(scene_path: Path):
    with scene_path.open("r", encoding="utf-8") as f:
        data = json.load(f)

    if not isinstance(data, list) or not data or data[0] != "s72-v2":
        raise ValueError("Scene file is not a valid s72-v2 array.")

    return data


def save_scene(scene_path: Path, scene_data):
    with scene_path.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(scene_data, f, ensure_ascii=False, indent="\t")
        f.write("\n")


def build_point_light_objects(count: int):
    # Use a centered square grid so all lights are easy to reason about.
    cols = math.ceil(math.sqrt(count))
    spacing = 0.6
    z = 1.0

    lights = []
    nodes = []

    for i in range(count):
        row = i // cols
        col = i % cols

        x = (col - (cols - 1) / 2.0) * spacing
        y = (row - (cols - 1) / 2.0) * spacing

        light_name = f"Point.{i + 1:04d}"
        node_name = f"PointNode.{i + 1:04d}"

        lights.append(
            {
                "type": "LIGHT",
                "name": light_name,
                "tint": [1, 1, 1],
                "sphere": {"radius": 0, "power": 10},
            }
        )
        nodes.append(
            {
                "type": "NODE",
                "name": node_name,
                "translation": [x, y, z],
                "rotation": [0, 0, 0, 1],
                "scale": [1, 1, 1],
                "light": light_name,
            }
        )

    return lights, nodes


def update_scene_with_light_count(base_scene, count: int):
    scene = copy.deepcopy(base_scene)

    objects = scene[1:]
    light_names = {
        obj["name"]
        for obj in objects
        if isinstance(obj, dict) and obj.get("type") == "LIGHT" and "name" in obj
    }
    light_node_names = {
        obj["name"]
        for obj in objects
        if isinstance(obj, dict) and obj.get("type") == "NODE" and obj.get("light") in light_names
    }

    filtered_objects = []
    scene_obj = None
    for obj in objects:
        if not isinstance(obj, dict):
            filtered_objects.append(obj)
            continue

        if obj.get("type") == "LIGHT":
            continue
        if obj.get("type") == "NODE" and obj.get("name") in light_node_names:
            continue
        if obj.get("type") == "SCENE":
            scene_obj = obj
            continue

        filtered_objects.append(obj)

    if scene_obj is None:
        raise ValueError("No SCENE object found in scene file.")

    roots = scene_obj.get("roots", [])
    roots = [r for r in roots if r not in light_node_names]

    new_lights, new_nodes = build_point_light_objects(count)
    scene_obj["roots"] = roots + [node["name"] for node in new_nodes]

    scene[:] = ["s72-v2", *filtered_objects, *new_lights, *new_nodes, scene_obj]
    return scene


def run_render(repo_root: Path):
    result = subprocess.run(
        ["cmd", "/c", "run.bat"],
        cwd=repo_root,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise RuntimeError(f"run.bat failed with exit code {result.returncode}")


def read_headless_averages(headless_file: Path):
    if not headless_file.exists():
        raise FileNotFoundError(f"Headless output file not found: {headless_file}")

    content = headless_file.read_text(encoding="utf-8", errors="ignore")
    gpu_values = [float(m.group(1)) for m in GPU_TIME_PATTERN.finditer(content)]
    cpu_values = [float(m.group(1)) for m in CPU_TIME_PATTERN.finditer(content)]

    if not gpu_values:
        raise ValueError("No GPU Headless frame time found in headless output file.")
    if not cpu_values:
        raise ValueError("No CPU Headless frame time found in headless output file.")

    return sum(gpu_values) / len(gpu_values), sum(cpu_values) / len(cpu_values)


def main():
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent

    scene_path = repo_root / "report" / "A3" / "A3-LightNumPBR.s72"
    headless_path = repo_root / "scripts" / "headless_frame_time.txt"

    original_scene = load_scene(scene_path)
    results = []

    try:
        for count in LIGHT_COUNTS:
            print(f"[INFO] Rendering with {count} point lights...")
            updated_scene = update_scene_with_light_count(original_scene, count)
            save_scene(scene_path, updated_scene)

            run_render(repo_root)
            avg_gpu_ms, avg_cpu_ms = read_headless_averages(headless_path)
            results.append((count, avg_gpu_ms, avg_cpu_ms))

            print(f"[INFO] lights={count}, avg GPU={avg_gpu_ms:.6f} ms, avg CPU={avg_cpu_ms:.6f} ms")
    finally:
        # Restore the original scene file to avoid leaving benchmark artifacts.
        save_scene(scene_path, original_scene)

    lights = [count for count, _, _ in results]
    frame_time_pgpu = [avg_gpu_ms for _, avg_gpu_ms, _ in results]
    frame_time_pcpu = [avg_cpu_ms for _, _, avg_cpu_ms in results]

    def format_js_array(values, is_float=False):
        if is_float:
            body = ", ".join(f"{v:.6f}" for v in values)
        else:
            body = ", ".join(str(v) for v in values)
        return f"[{body}]"

    print("\n=== Copy To report.html ===")
    print(f"const lights = {format_js_array(lights)};")
    print(f"const frameTimePCPU = {format_js_array(frame_time_pcpu, is_float=True)};")
    print(f"const frameTimePGPU = {format_js_array(frame_time_pgpu, is_float=True)};")


if __name__ == "__main__":
    main()
