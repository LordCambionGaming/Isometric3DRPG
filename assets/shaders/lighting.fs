#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragCamPos;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform sampler2D shadowMap;
uniform mat4  lightVP;
uniform int   shadowsOn;
uniform float shadowTexel;
uniform float shadowStrength;

uniform vec3  ambientColor;
uniform float ambientIntensity;
uniform vec3  sunDirection;
uniform vec3  sunColor;

#define MAX_LIGHTS 8
uniform int   lightsCount;
uniform vec3  lightPositions[MAX_LIGHTS];
uniform vec3  lightColors[MAX_LIGHTS];
uniform float lightRadii[MAX_LIGHTS];

uniform vec3  fogColor;
uniform float fogDensity;
uniform float exposure;

out vec4 finalColor;

const float PI = 3.14159265;

vec3 toLinear(vec3 c) { return pow(c, vec3(2.2)); }

vec3 aces(vec3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 shade(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float rough, float metal, float wrap)
{
    float ndl = dot(N, L);
    float diff = clamp((ndl + wrap) / (1.0 + wrap), 0.0, 1.0);
    vec3 H = normalize(L + V);
    float nh = max(dot(N, H), 0.0);
    float vh = max(dot(V, H), 0.0);
    float gloss = 1.0 - rough;
    float shin = mix(6.0, 220.0, gloss * gloss);
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - vh, 5.0);
    float spec = pow(nh, shin) * (shin + 8.0) / (8.0 * PI);
    vec3 kd = (1.0 - F) * (1.0 - metal);
    return (kd * albedo * diff + F * spec * max(ndl, 0.0)) * radiance;
}

float shadowFactor(vec3 N, vec3 L)
{
    if (shadowsOn == 0) return 1.0;
    vec4 lp = lightVP * vec4(fragPosition + N * 0.05, 1.0);
    vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    float bias = max(0.0030 * (1.0 - dot(N, L)), 0.0008);
    float lit = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) {
            float d = texture(shadowMap, p.xy + vec2(x, y) * shadowTexel).r;
            lit += (p.z - bias <= d) ? 1.0 : 0.0;
        }
    return lit / 9.0;
}

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    if (texel.a < 0.1) discard;

    vec3 albedo = toLinear((texel * colDiffuse * fragColor).rgb);

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(fragCamPos - fragPosition);
    if (!gl_FrontFacing) N = -N;

    float rough = 0.75, metal = 0.0;

    vec3 skyAmb    = toLinear(ambientColor) * ambientIntensity * 1.25;
    vec3 groundAmb = skyAmb * vec3(0.55, 0.48, 0.40);
    vec3 ambient   = mix(groundAmb, skyAmb, N.y * 0.5 + 0.5) * albedo;

    vec3 L = normalize(sunDirection);
    vec3 sun = toLinear(sunColor) * 2.6;
    float sh = mix(1.0, shadowFactor(N, L), shadowStrength);
    vec3 color = ambient + shade(N, V, L, sun, albedo, rough, metal, 0.18) * sh;

    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    float sunUp = clamp(sunDirection.y * 2.0 + 0.3, 0.0, 1.0);
    color += rim * 0.22 * (skyAmb * 1.6 + toLinear(sunColor) * 0.35 * sunUp) * mix(vec3(1.0), albedo, 0.5);

    for (int i = 0; i < lightsCount && i < MAX_LIGHTS; i++) {
        vec3 lv = lightPositions[i] - fragPosition;
        float dist = length(lv);
        float r = lightRadii[i];
        if (dist < r) {
            float x = dist / r;
            float att = clamp(1.0 - x * x, 0.0, 1.0);
            att *= att;
            color += shade(N, V, lv / dist, toLinear(lightColors[i]) * 3.2 * att, albedo, rough, metal, 0.0);
        }
    }

    float ex = exposure > 0.0 ? exposure : 1.0;
    color = aces(color * ex);
    color = pow(color, vec3(1.0 / 2.2));

    if (fogDensity > 0.0) {
        float d = length(fragPosition - fragCamPos) * fogDensity;
        float f = 1.0 - exp(-d * d);
        color = mix(color, fogColor, clamp(f, 0.0, 1.0));
    }

    finalColor = vec4(color, texel.a);
}