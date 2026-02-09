#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vTexCoord;

uniform vec3 uLightPos;
uniform vec3 uAmbient;
uniform vec3 uDiffuse;
uniform vec3 uViewPos;
uniform float uHighlight;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vWorldPos);

    float diff = max(dot(normal, lightDir), 0.0);

    vec3 ambient = uAmbient * vColor;
    vec3 diffuse = uDiffuse * diff * vColor;

    vec3 color = ambient + diffuse;

    // Highlight effect
    color = mix(color, vec3(1.0), uHighlight * 0.3);

    FragColor = vec4(color, 1.0);
}
