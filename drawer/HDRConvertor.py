import numpy as np
import imageio.v3 as imageio
import py360convert

def float_to_rgbe(img_float):
    """将 32位 浮点型 HDR 图像转换为 RGBE 编码的 8位 RGBA 图像"""
    # 获取 RGB 三个通道中的最大值 V
    v = np.max(img_float, axis=-1)
    
    # 使用 frexp 函数分离尾数(mantissa)和指数(exponent)
    mantissa, exponent = np.frexp(v)
    
    # 忽略极小值，防止对数计算出现负无穷
    valid_mask = v > 1e-32
    
    # 初始化输出数组，形状为 (Height, Width, 4)，类型为 8位无符号整数
    rgbe = np.zeros(img_float.shape[:2] + (4,), dtype=np.uint8)
    
    # 计算缩放因子：公式为 256.0 / (2^E)
    scale = np.zeros_like(v)
    scale[valid_mask] = 256.0 / (2.0 ** exponent[valid_mask])
    
    # 写入 R, G, B 通道 (并使用 clip 防止溢出)
    for i in range(3):
        rgbe[..., i] = np.clip(img_float[..., i] * scale, 0, 255).astype(np.uint8)
    
    # 写入 Alpha 通道作为指数 E，并按 RGBE 标准偏移 +128
    rgbe[..., 3][valid_mask] = np.clip(exponent[valid_mask] + 128, 0, 255).astype(np.uint8)
    
    return rgbe

if __name__ == "__main__":
    # 1. 读取从 Poly Haven 下载的 HDR 图片 (请修改为你的文件名)
    input_file = "D:\A1-Profession\ComputerGraphic\myvk\\report\A2\images\ox_bridge_morning_4k.hdr"
    output_file = "output_rgbe_vertical.png"
    
    print("正在加载 HDR 图像...")
    hdr_image = imageio.imread(input_file)

    # 2. 将全景图转换为天空盒的 6 个面
    # face_w 代表天空盒每个面的分辨率，1024 生成的长图就是 1024 x 6144
    print("正在转换为天空盒贴图...")
    faces_dict = py360convert.e2c(hdr_image, face_w=1024, cube_format='dict')

    # 3. 定义纵向排列顺序
    # 常见的 WebGL 引擎（如 Three.js）顺序通常为: +X, -X, +Y, -Y, +Z, -Z
    # 对应 py360convert 缩写: R(右), L(左), U(上), D(下), F(前), B(后)
    # 如果你的引擎面有颠倒或错位，只需调整下面列表的顺序即可
    order = ['R', 'L', 'U', 'D', 'F', 'B']
    
    print("正在纵向拼接图像...")
    vertical_strip = np.vstack([faces_dict[face] for face in order])

    # 4. 将拼接好的浮点数数据转换为 RGBE 编码
    print("正在进行 RGBE 编码...")
    rgbe_image = float_to_rgbe(vertical_strip)

    # 5. 导出为包含 Alpha 通道的 PNG
    print("正在导出 PNG 图片...")
    imageio.imwrite(output_file, rgbe_image)
    print(f"转换成功！已生成: {output_file}")