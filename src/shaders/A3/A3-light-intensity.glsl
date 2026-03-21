const float LIGHT_PI = 3.14159265359;

float horizon_approx(float NoL, float sa){
	float fac;
	if(NoL > sa){
		fac = NoL;
	} else if (NoL > -sa){
		fac = mix(sa, 0.0, (NoL - sa) / (-sa - sa));
	} else {
		fac = 0.0;
	}
	return fac;
}

vec3 sampleSunLightIntensity(SunLight sunLight) {
	return sunLight.tint;
}

float sunLightNoLFactor(SunLight sunLight, vec3 N) {
	vec3 L = normalize(-sunLight.direction);
	float theta = sunLight.angle;
	float NoL = dot(N, L);
	float e0 = max(NoL, 0.0);
	float ePI = 0.5 * NoL + 0.5;
	return mix(e0, ePI, theta / LIGHT_PI);
}

vec3 sampleSphereLightIntensity(SphereLight sphereLight, vec3 fragPosition, vec3 N) {
	vec3 toLight = sphereLight.position - fragPosition;
	float distance = length(toLight);
	if (distance <= 0.0 || distance >= sphereLight.far_plane) {
		return vec3(0.0);
	}

	float effectiveDistance = max(distance, sphereLight.radius);
	float attenuation = (1.0 - pow(effectiveDistance / sphereLight.far_plane, 4.0));
	attenuation = max(attenuation, 0.0);
	return sphereLight.tint * attenuation / (4.0 * LIGHT_PI * effectiveDistance * effectiveDistance);
}

vec3 sampleSpotLightIntensity(SpotLight spotLight, vec3 fragPosition, vec3 N) {
	vec3 toLight = spotLight.position - fragPosition;
	float distance = length(toLight);
	if (distance <= 0.0 || distance >= spotLight.limit) {
		return vec3(0.0);
	}

	vec3 L = toLight / distance;
	float sa = (distance <= spotLight.radius ? 1.0 : spotLight.radius / distance);
	float NoLFactor = horizon_approx(dot(N, L), sa);

	float phi = acos(dot(L, normalize(-spotLight.direction)));
	if (phi >= spotLight.fov * 0.5) {
		return vec3(0.0);
	}

	float blend = 1.0;
	float blendDenominator = spotLight.fov * spotLight.blend * 0.5;
	if (blendDenominator > 0.0) {
		blend = min(1.0, (spotLight.fov * 0.5 - phi) / blendDenominator);
	}

	float effectiveDistance = max(distance, spotLight.radius);
	float attenuation = (1.0 - pow(effectiveDistance / spotLight.limit, 4.0));
	attenuation = max(attenuation, 0.0);
	return blend * spotLight.tint * attenuation * NoLFactor / (4.0 * LIGHT_PI * effectiveDistance * effectiveDistance);
}

float areaLightNoLFactor(float radius, vec3 toLight, vec3 N) {
	float distance = length(toLight);
	vec3 L = toLight / length(toLight);
	float sa = (distance <= radius ? 1.0 : radius / distance);
	float NoLFactor = horizon_approx(dot(N, L), sa);
	return NoLFactor;
}