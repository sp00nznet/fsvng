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

// Glow/rim uniforms (default 0 = Classic theme, no visual change)
uniform vec3 uGlowColor;
uniform float uGlowIntensity;
uniform float uRimIntensity;
uniform float uRimPower;

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

    // Rim glow (Fresnel edge lighting)
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    rim = pow(rim, max(uRimPower, 0.01));
    vec3 rimGlow = uGlowColor * rim * uRimIntensity;

    // Emissive glow (base + pulse)
    vec3 emissive = uGlowColor * uGlowIntensity;

    color += rimGlow + emissive;

    FragColor = vec4(color, 1.0);
}
