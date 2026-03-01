const float LIGHT_PI = 3.14159265359;

struct LightSample {
	vec3 L;         // object -> light direction (normalized)
	vec3 intensity; // radiance/intensity (without NoL and BRDF term)
};

LightSample makeNoLightSample() {
	LightSample ls;
	ls.L = vec3(0.0, 0.0, 1.0);
	ls.intensity = vec3(0.0);
	return ls;
}

LightSample sampleSunLightIntensity(SunLight sunLight) {
	LightSample ls;
	ls.L = normalize(sunLight.direction);
	ls.intensity = sunLight.tint;
	return ls;
}

LightSample sampleSphereLightIntensity(SphereLight sphereLight, vec3 fragPosition) {
	LightSample ls = makeNoLightSample();

	vec3 toLight = sphereLight.position - fragPosition;
	float distance = length(toLight);
	if (distance <= 0.0 || distance >= sphereLight.limit) {
		return ls;
	}

	ls.L = toLight / distance;
	float effectiveDistance = max(distance, sphereLight.radius);
	float attenuation = (1.0 - pow(effectiveDistance / sphereLight.limit, 4.0));
	attenuation = max(attenuation, 0.0);
	ls.intensity = sphereLight.tint * attenuation / (4.0 * LIGHT_PI * effectiveDistance * effectiveDistance);
	return ls;
}

LightSample sampleSpotLightIntensity(SpotLight spotLight, vec3 fragPosition) {
	LightSample ls = makeNoLightSample();

	vec3 toLight = spotLight.position - fragPosition;
	float distance = length(toLight);
	if (distance <= 0.0 || distance >= spotLight.limit) {
		return ls;
	}

	vec3 L = toLight / distance;
	float phi = acos(clamp(dot(L, normalize(spotLight.direction)), -1.0, 1.0));
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

	ls.L = L;
	ls.intensity = blend * spotLight.tint * attenuation / (4.0 * LIGHT_PI * effectiveDistance * effectiveDistance);
	return ls;
}
