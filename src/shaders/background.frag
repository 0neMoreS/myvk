#version 450

layout(location = 0) in vec2 position;
layout(location = 0) out vec4 outColor;
layout(push_constant) uniform Push {
	float time;
};

// --- Perlin helpers ---
float hash12(vec2 p) {
    // Fast-ish hash: returns [0,1)
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 grad2(vec2 ip) {
    // Random unit-ish gradient
    float a = 6.2831853 * hash12(ip);
    return vec2(cos(a), sin(a));
}

float fade(float t) {
    // Perlin's quintic fade
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float perlin2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    vec2 g00 = grad2(i + vec2(0.0, 0.0));
    vec2 g10 = grad2(i + vec2(1.0, 0.0));
    vec2 g01 = grad2(i + vec2(0.0, 1.0));
    vec2 g11 = grad2(i + vec2(1.0, 1.0));

    float v00 = dot(g00, f - vec2(0.0, 0.0));
    float v10 = dot(g10, f - vec2(1.0, 0.0));
    float v01 = dot(g01, f - vec2(0.0, 1.0));
    float v11 = dot(g11, f - vec2(1.0, 1.0));

    vec2 u = vec2(fade(f.x), fade(f.y));

    float vx0 = mix(v00, v10, u.x);
    float vx1 = mix(v01, v11, u.x);
    return mix(vx0, vx1, u.y); // roughly in [-~0.7, ~0.7]
}

float fbm(vec2 p) {
    float sum = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 5; ++i) {
        sum += amp * perlin2(p * freq);
        freq *= 2.0;
        amp *= 0.5;
    }
    return sum;
}

void main() {
    vec2 uv = position * 0.5 + 0.5;
    float scale = 6.0;
    vec2 p = uv * scale + vec2(time * 0.2, time * 0.15);
    float n = fbm(p);
    float c = 0.5 + 0.5 * n;

    outColor = vec4(vec3(c), 1.0);
}