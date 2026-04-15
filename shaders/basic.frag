#version 400 core

in vec3 fragNormal;
in vec3 fragPos;

uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

out vec4 fragColor;

void main() {
    vec3 ambient = 0.3 * objectColor;

    vec3 norm     = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = diff * objectColor;

    vec3 viewDir    = normalize(viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular   = 0.3 * spec * vec3(1.0);

    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
