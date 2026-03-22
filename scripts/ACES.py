import numpy as np

def aces_approx(v, E):
    v = np.array(v, dtype=float)
    
    v *= np.power(2.0, E)
    
    v *= 0.6
    
    a = 2.51
    b = 0.03
    c = 2.43
    d = 0.59
    e = 0.14
    
    numerator = v * (a * v + b)
    denominator = v * (c * v + d) + e
    
    output = numerator / denominator
    
    return np.clip(output, 0.0, 1.0)

for a in [0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0]:
    print(f"Input: {np.array([a, a, a])}, Output: {aces_approx(np.array([a, a, a]), E=0.0)}")

# hdr_color = np.array([0.8, 1.5, 3.0])
# ldr_color = aces_approx(hdr_color, E=0.0)

# print(f"HDR: {hdr_color}")
# print(f"LDR: {ldr_color}")