float computeSpotLightShadow(SpotLight spotLight, vec3 fragPosition, sampler2D shadowMapTexture){
	vec4 light_space = spotLight.perspective * vec4(fragPosition, 1.0);
	vec3 projected = light_space.xyz / light_space.w;
	vec2 uv = projected.xy * 0.5 + vec2(0.5);

	float bias = 0.001;
	float texel_size = 1.05 / float(spotLight.shadow);
	float sum = 0.0;

	for (int x = -1; x <= 1; ++x) {
		for (int y = -1; y <= 1; ++y) {
			float closest_depth = texture(shadowMapTexture, uv + vec2(x, y) * texel_size).r;
			sum += (projected.z + bias > closest_depth) ? 1.0 : 0.0;
		}
	}

	return sum / 9.0;
}

/*
 * PCF kernel directions for omnidirectional point-light shadow maps.
 * We jitter the lookup direction and sample from the cubemap depth.
 */
vec3 sphereShadowPcfDirections[20] = vec3[](
	vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
	vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
	vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
	vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
	vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float computeSphereLightShadow(SphereLight sphereLight, vec3 fragPosition, samplerCube shadowMapTexture) {
	vec3 lightToFrag = fragPosition - sphereLight.position;
	float distanceToLight = length(lightToFrag);

	if (distanceToLight <= sphereLight.radius) {
		return 1.0;
	}
	if (distanceToLight >= sphereLight.limit) {
		return 0.0;
	}

	vec3 sampleDir = normalize(lightToFrag);
	int litSamples = 0;
	float bias = 0.001;

	for (int i = 0; i < 20; ++i) {
		float closestDepth = texture(shadowMapTexture, sampleDir + sphereShadowPcfDirections[i] * 0.0005).r;
		closestDepth = 25.0 / (1.0 + 24.0 * closestDepth);
		if (distanceToLight < closestDepth) {
			litSamples += 1;
		}
	}

	return litSamples / 20.0;
}


// ================================================================
// This is a simplified version without PCF, for debugging purposes
// ================================================================

// float computeSunLightShadow(SunLight sunLight, vec3 fragPosition, vec3 viewSpaceFragPosition, sampler2DArray shadowMapTexture) {
// 	int cascadeIndex = 3;
// 	for (int i = 0; i < 4; ++i) {
// 		// viewSpaceFragPosition.z is negative in front of the camera
// 		if (-viewSpaceFragPosition.z < sunLight.cascadeSplits[i]) {
// 			cascadeIndex = i;
// 			break;
// 		}
// 	}

// 	vec4 lightSpace = sunLight.orthographic[cascadeIndex] * vec4(fragPosition, 1.0);

// 	vec3 projected = lightSpace.xyz / lightSpace.w;
// 	vec2 uv = projected.xy * 0.5 + vec2(0.5);

// 	float bias = 0.001;

//     float closestDepth = texture(shadowMapTexture, vec3(uv, cascadeIndex)).r;
//     return (projected.z - bias <= closestDepth) ? 1.0 : 0.0;
// }

float sampleShadowPCF(sampler2DArray shadowMap, int cascadeIndex, vec3 projectedPos, vec2 texelSize, float bias) {
    float pcfSum = 0.0;
    vec2 uv = projectedPos.xy * 0.5 + vec2(0.5);
    float currentDepth = projectedPos.z;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 pcfUV = uv + vec2(x, y) * texelSize;
            float pcfDepth = texture(shadowMap, vec3(pcfUV, cascadeIndex)).r;
            pcfSum += (currentDepth + bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return pcfSum / 9.0;
}


float computeSunLightShadow(SunLight sunLight, vec3 fragPosition, vec3 viewSpaceFragPosition, sampler2DArray shadowMapTexture) {
    // Choose cascade level based on view-space depth of the fragment
	int cascadeIndex = 3;
    float viewZ = -viewSpaceFragPosition.z;
    for (int i = 0; i < 4; ++i) {
        if (viewZ < sunLight.cascadeSplits[i]) {
            cascadeIndex = i;
            break;
        }
    }

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapTexture, 0).xy);

    // calculate shadow using PCF sampling
    vec4 lightSpace = sunLight.orthographic[cascadeIndex] * vec4(fragPosition, 1.0);
    vec3 projected = lightSpace.xyz / lightSpace.w;
    
    // dynamic bias based on cascade level to reduce shadow acne
    float baseBias = 0.00005 * (float(cascadeIndex) + 1.0);
    
    float shadow = sampleShadowPCF(shadowMapTexture, cascadeIndex, projected, texelSize, baseBias);

    // cascade blending for smoother transitions between shadow map levels
    if (cascadeIndex < 3) {
        float currentSplit = sunLight.cascadeSplits[cascadeIndex];
        
        float blendDistance = 2.0; 
        float distanceToSplit = currentSplit - viewZ;

        // if the fragment is within the blend distance to the next cascade split, blend the shadow results
        if (distanceToSplit < blendDistance) {
            float blendFactor = distanceToSplit / blendDistance;

            int nextCascadeIndex = cascadeIndex + 1;
            vec4 nextLightSpace = sunLight.orthographic[nextCascadeIndex] * vec4(fragPosition, 1.0);
            vec3 nextProjected = nextLightSpace.xyz / nextLightSpace.w;
            float nextBias = 0.00005 * (float(nextCascadeIndex) + 1.0);

            float nextShadow = sampleShadowPCF(shadowMapTexture, nextCascadeIndex, nextProjected, texelSize, nextBias);

            // intropolate between current and next cascade shadow results
            shadow = mix(nextShadow, shadow, blendFactor);
        }
    }

    return shadow;
}

vec3 debugSunLightShadow(SunLight sunLight, vec3 fragPosition, vec3 viewSpaceFragPosition, sampler2DArray shadowMapTexture) {
	// int cascadeIndex = 3;
	// for (int i = 0; i < 4; ++i) {
	// 	// viewSpaceFragPosition.z is negative in front of the camera
	// 	if (-viewSpaceFragPosition.z < sunLight.cascadeSplits[i]) {
	// 		cascadeIndex = i;
	// 		break;
	// 	}
	// }

	// vec3 debugColor;
	// switch (cascadeIndex) {
	// 	case 0:
	// 		return vec3(1.0, 0.0, 0.0); // red
	// 	case 1:
	// 		return vec3(0.0, 1.0, 0.0); // green
	// 	case 2:
	// 		return vec3(0.0, 0.0, 1.0); // blue
	// 	case 3:
	// 		return vec3(1.0, 1.0, 0.0); // yellow
	// 	default:
	// 		return vec3(0.0);
	// }

	int cascadeIndex = 3;
	for (int i = 0; i < 4; ++i) {
		// viewSpaceFragPosition.z is negative in front of the camera
		if (-viewSpaceFragPosition.z < sunLight.cascadeSplits[i]) {
			cascadeIndex = i;
			break;
		}
	}

	vec4 lightSpace = sunLight.orthographic[cascadeIndex] * vec4(fragPosition, 1.0);

	vec3 projected = lightSpace.xyz / lightSpace.w;

	return vec3(projected.z);
	// vec2 uv = projected.xy * 0.5 + vec2(0.5);

	// float bias = 0.001;

    // float closestDepth = texture(shadowMapTexture, vec3(uv, cascadeIndex)).r;
    // return (projected.z - bias <= closestDepth) ? vec3(1.0) : vec3(0.0);
}

vec3 debugSphereLightShadow(SphereLight sphereLight, vec3 fragPosition, samplerCube shadowMapTexture) {
	vec3 lightToFrag = fragPosition - sphereLight.position;
	float distanceToLight = length(lightToFrag);

	if (distanceToLight <= sphereLight.radius) {
		return vec3(1.0);
	}
	if (distanceToLight >= sphereLight.limit) {
		return vec3(0.0);
	}

	vec3 sampleDir = normalize(lightToFrag);
	int litSamples = 0;

	// for (int i = 0; i < 20; ++i) {
	// 	float closestDepth = texture(shadowMapTexture, sampleDir + sphereShadowPcfDirections[i] * 0.0005).r;
	// 	closestDepth = 25.0 * closestDepth;
	// 	if (distanceToLight > closestDepth) {
	// 		litSamples += 1;
	// 	}
	// }
	
	float sampledDepth = texture(shadowMapTexture, sampleDir).r;
	float closestDepth = 25.0 * sampledDepth;
	float bias = 0.001;

	// Return visibility: 1.0 means lit, 0.0 means shadowed.
	return vec3(closestDepth < distanceToLight ? 1.0 : 0.0);
}