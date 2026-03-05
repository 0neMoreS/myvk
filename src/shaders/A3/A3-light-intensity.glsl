const float LIGHT_PI = 3.14159265359;

struct LightSample {
	vec3 intensity; // radiance/intensity (without NoL and BRDF term)
	float NoL;      // cosine term, already clamped to [0, 1]
};

LightSample makeNoLightSample() {
	LightSample ls;
	ls.intensity = vec3(0.0);
	ls.NoL = 0.0;
	return ls;
}

LightSample sampleSunLightIntensity(SunLight sunLight, vec3 N) {
	LightSample ls = makeNoLightSample();
	vec3 L = normalize(-sunLight.direction);
	ls.intensity = sunLight.tint;
	float theta = acos(clamp(dot(L, N), -1.0, 1.0)) - sunLight.angle * 0.5;
	if (theta >= LIGHT_PI * 0.5) {
		ls.NoL = 0.0;
		return ls;
	}
	theta = max(theta, 0.0);
	ls.NoL = cos(theta);
	return ls;
}

LightSample sampleSphereLightIntensity(SphereLight sphereLight, vec3 fragPosition, vec3 N) {
	LightSample ls = makeNoLightSample();

	vec3 toLight = sphereLight.position - fragPosition;
	float distance = length(toLight);
	if (distance <= 0.0 || distance >= sphereLight.limit) {
		return ls;
	}

	vec3 L = toLight / distance;
	ls.NoL = max(dot(N, L), 0.0);
	if (ls.NoL <= 0.0) {
		return ls;
	}
	float effectiveDistance = max(distance, sphereLight.radius);
	float attenuation = (1.0 - pow(effectiveDistance / sphereLight.limit, 4.0));
	attenuation = max(attenuation, 0.0);
	ls.intensity = sphereLight.tint * attenuation / (4.0 * LIGHT_PI * effectiveDistance * effectiveDistance);
	return ls;
}

LightSample sampleSpotLightIntensity(SpotLight spotLight, vec3 fragPosition, vec3 N) {
	LightSample ls = makeNoLightSample();

	vec3 toLight = spotLight.position - fragPosition;
	float distance = length(toLight);
	if (distance <= 0.0 || distance >= spotLight.limit) {
		return ls;
	}

	vec3 L = toLight / distance;
	ls.NoL = max(dot(N, L), 0.0);
	if (ls.NoL <= 0.0) {
		return ls;
	}
	float phi = acos(clamp(dot(L, normalize(-spotLight.direction)), -1.0, 1.0));
	if (phi >= spotLight.fov * 0.5) {
		return ls;
	}

	float blend = 1.0;
	float blendDenominator = spotLight.fov * spotLight.blend * 0.5;
	if (blendDenominator > 0.0) {
		blend = min(1.0, (spotLight.fov * 0.5 - phi) / blendDenominator);
	}

	float effectiveDistance = max(distance, spotLight.radius);
	float attenuation = (1.0 - pow(effectiveDistance / spotLight.limit, 4.0));
	attenuation = max(attenuation, 0.0);

	ls.intensity = blend * spotLight.tint * attenuation / (4.0 * LIGHT_PI * effectiveDistance * effectiveDistance);
	return ls;
}
