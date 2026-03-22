float linearizePerspectiveDepth(float depth01, float nearPlane, float farPlane) {
	float safeNear = max(nearPlane, 1e-5);
	float safeFar = max(farPlane, safeNear + 1e-5);
	return (safeNear * safeFar) / (safeNear + depth01 * (safeFar - safeNear));
}

vec2 spotShadowSampleOffsets[20] = vec2[](
	vec2( 0.000,  0.000), vec2( 0.527,  0.085), vec2(-0.040,  0.536), vec2(-0.671, -0.044),
	vec2( 0.043, -0.674), vec2( 0.311,  0.615), vec2(-0.620,  0.331), vec2(-0.352, -0.620),
	vec2( 0.626, -0.327), vec2( 0.914,  0.258), vec2( 0.287,  0.926), vec2(-0.352,  0.884),
	vec2(-0.889,  0.266), vec2(-0.930, -0.240), vec2(-0.296, -0.922), vec2( 0.314, -0.914),
	vec2( 0.878, -0.330), vec2( 0.683,  0.681), vec2(-0.704,  0.662), vec2(-0.684, -0.681)
);

float getRandomAngle(vec2 fragCoord) {
    float noise = fract(52.9829189 * fract(dot(fragCoord, vec2(0.06711056, 0.00583715))));
    return noise * 6.28318530718;
}

float computeSpotLightShadow(SpotLight spotLight, vec3 fragPosition, sampler2D shadowMapTexture){
    vec4 lightSpace = spotLight.perspective * vec4(fragPosition, 1.0);
    vec3 projected = lightSpace.xyz / lightSpace.w;
    vec2 uv = projected.xy * 0.5 + vec2(0.5);

    if (projected.z <= 0.0 || projected.z >= 1.0 || uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0) {
        return 1.0;
    }

    float angle = getRandomAngle(gl_FragCoord.xy); 
    float s = sin(angle);
    float c = cos(angle);
    mat2 rotationMat = mat2(c, -s, s, c);

    const int blockerSamples = 20;
    const int pcfSamples = 20;

    float receiverDepth = projected.z;
    float receiverLinearDepth = linearizePerspectiveDepth(receiverDepth, spotLight.near_plane, spotLight.far_plane);
    float texelSize = 1.0 / max(float(spotLight.shadow), 1.0);
    
    // 【修复1】：定义一个真实的线性世界空间Bias（需根据你的场景单位微调，比如0.05米）
    float linearBias = 0.005; 
    // 将线性Bias动态转换为Reverse-Z非线性Bias：(near / z^2) * linearBias
    float depthDerivative = spotLight.near_plane / (receiverLinearDepth * receiverLinearDepth);
    float dynamicBaseBias = linearBias * depthDerivative;

    float lightRadiusUv = clamp(spotLight.radius / max(spotLight.far_plane, 1e-5), 0.0005, 0.08);
    float searchRadius = lightRadiusUv * clamp((receiverLinearDepth - spotLight.near_plane) / max(receiverLinearDepth, 1e-5), 0.0, 1.0);

    float blockerLinearDepthSum = 0.0;  
    int blockerCount = 0;
    for (int i = 0; i < blockerSamples; ++i) {
        vec2 rotatedOffset = rotationMat * spotShadowSampleOffsets[i];
        vec2 sampleUv = uv + rotatedOffset * searchRadius;
        float sampleDepth = texture(shadowMapTexture, sampleUv).r;
        
        // 使用动态转换的Bias
        if (receiverDepth + dynamicBaseBias < sampleDepth) {
            blockerLinearDepthSum += linearizePerspectiveDepth(sampleDepth, spotLight.near_plane, spotLight.far_plane);
            blockerCount += 1;
        }
    }

    if (blockerCount == 0) {
        return 1.0;
    }

    float avgBlockerLinearDepth = blockerLinearDepthSum / float(blockerCount);
    
    // 【修复2】：补齐透视投影丢失的 (near / receiver) 因子，防止 filterRadius 爆炸
    float penumbraRatio = max((receiverLinearDepth - avgBlockerLinearDepth) * spotLight.near_plane / max(avgBlockerLinearDepth * receiverLinearDepth, 1e-5), 0.0);
    float filterRadius = max(lightRadiusUv * penumbraRatio, texelSize);

    float shadowed = 0.0;
    
    // PCF的Bias也使用基于导数的动态Bias，略微根据滤波半径放大
    float pcfLinearBias = linearBias + filterRadius * 2.0; // 线性空间的放大
    float dynamicPcfBias = pcfLinearBias * depthDerivative;

    for (int i = 0; i < pcfSamples; ++i) {
        vec2 rotatedOffset = rotationMat * spotShadowSampleOffsets[i];
        vec2 sampleUv = uv + rotatedOffset * filterRadius;
        float sampleDepth = texture(shadowMapTexture, sampleUv).r;
        
        shadowed += (receiverDepth + dynamicPcfBias < sampleDepth) ? 1.0 : 0.0;
    }

    return 1.0 - shadowed / float(pcfSamples);
}

/*
 * PCSS kernel directions for omnidirectional point-light shadow maps.
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
    float receiverDepth = length(lightToFrag);
    vec3 sampleDir = normalize(lightToFrag);
    const int blockerSamples = 20;
    const int pcfSamples = 20;
    float bias = 0.05;

    float angularSearchRadius = clamp((sphereLight.radius / max(receiverDepth, 1e-5)) * 0.5, 0.0005, 0.2);
    float blockerDepthSum = 0.0;
    int blockerCount = 0;

    for (int i = 0; i < blockerSamples; ++i) {
        vec3 offsetDir = normalize(sphereShadowPcfDirections[i]);
        vec3 sampleVector = normalize(sampleDir + offsetDir * angularSearchRadius);
        
        float sampleDepth = texture(shadowMapTexture, sampleVector).r;
        float planarDepth = linearizePerspectiveDepth(sampleDepth, sphereLight.near_plane, sphereLight.far_plane);
        
        vec3 absVec = abs(sampleVector);
        float maxComp = max(absVec.x, max(absVec.y, absVec.z));
        float blockerDepth = planarDepth / max(maxComp, 1e-5);

        if (receiverDepth > blockerDepth + bias) {
            blockerDepthSum += blockerDepth;
            blockerCount += 1;
        }
    }

    if (blockerCount == 0) {
        return 1.0;
    }

    float avgBlockerDepth = blockerDepthSum / float(blockerCount);
    float penumbra = max((receiverDepth - avgBlockerDepth) / max(avgBlockerDepth, 1e-5), 0.0) * sphereLight.radius;
    float angularFilterRadius = clamp(penumbra / max(receiverDepth, 1e-5), 0.0005, 0.3);

    float shadowed = 0.0;
    
    for (int i = 0; i < pcfSamples; ++i) {
        vec3 offsetDir = normalize(sphereShadowPcfDirections[i]);
        vec3 sampleVector = normalize(sampleDir + offsetDir * angularFilterRadius);
        
        float sampleDepth = texture(shadowMapTexture, sampleVector).r;
        float planarDepth = linearizePerspectiveDepth(sampleDepth, sphereLight.near_plane, sphereLight.far_plane);
        
        vec3 absVec = abs(sampleVector);
        float maxComp = max(absVec.x, max(absVec.y, absVec.z));
        float closestDepth = planarDepth / max(maxComp, 1e-5);

        shadowed += (receiverDepth > closestDepth + bias) ? 1.0 : 0.0;
    }

    return 1.0 - shadowed / float(pcfSamples);
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
			pcfSum += (currentDepth + bias < pcfDepth) ? 1.0 : 0.0;
        }
    }
	return 1.0 - pcfSum / 9.0;
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

	vec3 sampleDir = normalize(lightToFrag);
	int litSamples = 0;
	
	float sampledDepth = texture(shadowMapTexture, sampleDir).r;
	float closestDepth = (sphereLight.near_plane * sphereLight.far_plane) / (sphereLight.near_plane + sampledDepth * (sphereLight.far_plane - sphereLight.near_plane));
	float bias = 0.001;

	// Return visibility: 1.0 means lit, 0.0 means shadowed.
	return vec3(closestDepth < distanceToLight ? 1.0 : 0.0);
}