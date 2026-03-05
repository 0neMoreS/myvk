float computeSpotLightShadow(SpotLight spotLight, vec3 fragPosition, sampler2D shadowMapTexture){
	if (spotLight.shadow <= 0) {
		return 1.0;
	}

	vec4 light_space = spotLight.perspective * vec4(fragPosition, 1.0);
	if (light_space.w <= 0.0) {
		return 1.0;
	}

	vec3 projected = light_space.xyz / light_space.w;
	vec2 uv = projected.xy * 0.5 + vec2(0.5);
	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || projected.z < 0.0 || projected.z > 1.0) {
		return 1.0;
	}

	float bias = 0.001;
	float texel_size = 1.05 / float(spotLight.shadow);
	float sum = 0.0;

	for (int x = -1; x <= 1; ++x) {
		for (int y = -1; y <= 1; ++y) {
			float closest_depth = texture(shadowMapTexture, uv + vec2(x, y) * texel_size).r;
			sum += (projected.z - bias <= closest_depth) ? 1.0 : 0.0;
		}
	}

	return sum / 9.0;
}

float computeSunLightShadow(SunLight sunLight, vec3 fragPosition, vec3 viewSpaceFragPosition, sampler2DArray shadowMapTexture) {
	if (sunLight.shadow <= 0) {
		return 1.0;
	}

	int cascadeIndex = 3;
	for (int i = 0; i < 4; ++i) {
		// viewSpaceFragPosition.z is negative in front of the camera
		if (-viewSpaceFragPosition.z < sunLight.cascadeSplits[i]) {
			cascadeIndex = i;
			break;
		}
	}

	vec4 lightSpace = sunLight.orthographic[cascadeIndex] * vec4(fragPosition, 1.0);
	if (lightSpace.w <= 0.0) {
		return 1.0;
	}

	vec3 projected = lightSpace.xyz / lightSpace.w;
	vec2 uv = projected.xy * 0.5 + vec2(0.5);
	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || projected.z < 0.0 || projected.z > 1.0) {
		return 1.0;
	}

	float bias = 0.001;

    float closestDepth = texture(shadowMapTexture, vec3(uv, cascadeIndex)).r;
    return (projected.z - bias <= closestDepth) ? 1.0 : 0.0;
}

vec3 debugSunLightShadow(SunLight sunLight, vec3 fragPosition, vec3 viewSpaceFragPosition, sampler2DArray shadowMapTexture) {
	if (sunLight.shadow <= 0) {
		return vec3(1.0);
	}

	int cascadeIndex = 3;
	for (int i = 0; i < 4; ++i) {
		// viewSpaceFragPosition.z is negative in front of the camera
		if (-viewSpaceFragPosition.z < sunLight.cascadeSplits[i]) {
			cascadeIndex = i;
			break;
		}
	}

	vec3 debugColor;
	switch (cascadeIndex) {
		case 0:
			return vec3(1.0, 0.0, 0.0); // red
		case 1:
			return vec3(0.0, 1.0, 0.0); // green
		case 2:
			return vec3(0.0, 0.0, 1.0); // blue
		case 3:
			return vec3(1.0, 1.0, 0.0); // yellow
		default:
			return vec3(0.0);
	}
}