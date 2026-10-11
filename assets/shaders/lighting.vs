#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragCamPos;

// Mappe del materiale (raylib: slot = indice di MATERIAL_MAP_*)
uniform sampler2D texture0;   // albedo / base color
uniform sampler2D texture2;   // normal map        (se assente -> nero -> ignorata)
uniform sampler2D texture3;   // glTF metal/rough  (G = roughness, B = metallic; se assente -> ignorata)
uniform vec4 colDiffuse;

// --- uniform gia' usati da LightingSystem (invariati) ---
uniform vec3  ambientColor;
uniform float ambientIntensity;
uniform vec3  sunDirection;   // verso il sole
uniform vec3  sunColor;

#define MAX_LIGHTS 8
uniform int   lightsCount;
uniform vec3  lightPositions[MAX_LIGHTS];
uniform vec3  lightColors[MAX_LIGHTS];
uniform float lightRadii[MAX_LIGHTS];

// --- uniform NUOVI e opzionali: se non li imposti dal C++ il comportamento resta sicuro ---
uniform vec3  fogColor;       // colore della nebbia (di solito il colore del cielo, 0..1)
uniform float fogDensity;     // 0 = nessuna nebbia (default). Valori utili: 0.006 .. 0.02
uniform float exposure;       // 0 = usa il default (1.0)

out vec4 finalColor;

const float PI = 3.14159265;

vec3 toLinear(vec3 c) { return pow(c, vec3(2.2)); }

// Tonemapping ACES (Narkowicz): comprime le luci forti senza "bruciare" il colore
vec3 aces(vec3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Frame tangente ricavato dalle derivate: la normal map funziona senza tangenti nel mesh
mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv)
{
    vec3 dp1 = dFdx(p), dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv), duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-12));
    return mat3(T * invmax, B * invmax, N);
}

// BRDF semplificata: diffuso morbido + speculare Blinn-Phong normalizzato con Fresnel
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

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    if (texel.a < 0.1) discard;

    vec3 albedo = toLinear((texel * colDiffuse * fragColor).rgb);

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(fragCamPos - fragPosition);
    if (!gl_FrontFacing) N = -N;                         // facce doppie (foglie, mantelli)

    // Normal map (opzionale)
    vec3 nm = texture(texture2, fragTexCoord).rgb;
    if (dot(nm, nm) > 0.001) {
        vec3 tn = normalize(nm * 2.0 - 1.0);
        N = normalize(cotangentFrame(N, fragPosition, fragTexCoord) * tn);
    }

    // Roughness / metallic (opzionali)
    float rough = 0.72, metal = 0.0;
    vec3 mr = texture(texture3, fragTexCoord).rgb;
    if (dot(mr, mr) > 0.0001) { rough = clamp(mr.g, 0.05, 1.0); metal = clamp(mr.b, 0.0, 1.0); }

    // Ambiente emisferico: cielo dall'alto, terreno caldo e piu' scuro dal basso
    vec3 skyAmb    = toLinear(ambientColor) * ambientIntensity * 1.25;
    vec3 groundAmb = skyAmb * vec3(0.55, 0.48, 0.40);
    vec3 ambient   = mix(groundAmb, skyAmb, N.y * 0.5 + 0.5) * albedo * (1.0 - metal * 0.8);

    // Sole (diffuso con "wrap" leggero = pelle e stoffe meno dure)
    vec3 L = normalize(sunDirection);
    vec3 sun = toLinear(sunColor) * 2.6;
    vec3 color = ambient + shade(N, V, L, sun, albedo, rough, metal, 0.18);

    // Rim light: contorno luminoso che stacca i personaggi dallo sfondo
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    float sunUp = clamp(sunDirection.y * 2.0 + 0.3, 0.0, 1.0);
    color += rim * 0.22 * (skyAmb * 1.6 + toLinear(sunColor) * 0.35 * sunUp) * mix(vec3(1.0), albedo, 0.5);

    // Luci puntiformi (torce, finestre...) con falloff morbido
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

    // Nebbia atmosferica (solo se fogDensity > 0)
    if (fogDensity > 0.0) {
        float d = length(fragPosition - fragCamPos) * fogDensity;
        float f = 1.0 - exp(-d * d);
        color = mix(color, toLinear(fogColor), clamp(f, 0.0, 1.0));
    }

    float ex = exposure > 0.0 ? exposure : 1.0;
    color = aces(color * ex);
    color = pow(color, vec3(1.0 / 2.2));

    finalColor = vec4(color, texel.a);
}