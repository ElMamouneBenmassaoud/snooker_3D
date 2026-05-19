#version 400 core

in vec3 fragNormal;
in vec3 fragPos;
in vec2 fragUV;

uniform sampler2D   textureSampler;
uniform samplerCube envMap;
uniform int         useTexture;
uniform float       reflectStrength;  // 0 = no reflection, subtle > 0
uniform vec3        objectColor;
uniform vec3        lightPos[3];
uniform vec3        viewPos;

out vec4 fragColor;

void main() {
    vec3 baseColor = (useTexture == 1) ? texture(textureSampler, fragUV).rgb * objectColor : objectColor;

    vec3 norm    = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPos);

    vec3 ambient = 0.35 * baseColor;
    vec3 light   = vec3(0.0);

    for (int i = 0; i < 3; i++) {
        vec3 lightDir   = normalize(lightPos[i] - fragPos);
        float diff      = max(dot(norm, lightDir), 0.0);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        light += (0.45 * diff * baseColor + 0.08 * spec * vec3(1.0));
    }

    vec3 color = ambient + light;

    // Subtle environment reflection (cubemap) — only on balls (reflectStrength > 0)
    if (reflectStrength > 0.0) {
        vec3 envDir   = reflect(-viewDir, norm);
        vec3 envColor = texture(envMap, envDir).rgb;
        color = mix(color, color + envColor, reflectStrength);
    }

    fragColor = vec4(color, 1.0);
}
