#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Lighting uniforms
uniform vec3 ambientColor;
uniform float ambientIntensity;

uniform vec3 sunDirection;
uniform vec3 sunColor;

#define MAX_LIGHTS 8
uniform int lightsCount;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightRadii[MAX_LIGHTS];

// Output fragment color
out vec4 finalColor;

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);
    
    vec3 baseColor = (texelColor * colDiffuse * fragColor).rgb;
    
    // Base Ambient light
    vec3 lighting = ambientColor * ambientIntensity;
    
    vec3 normal = normalize(fragNormal);
    
    // Directional light (Sun)
    // Normalize sun direction just in case
    vec3 sDir = normalize(sunDirection);
    float sunDiff = max(0.0, dot(normal, sDir));
    lighting += sunColor * sunDiff;

    // Point lights
    for(int i = 0; i < lightsCount && i < MAX_LIGHTS; i++)
    {
        vec3 lightVector = lightPositions[i] - fragPosition;
        float distance = length(lightVector);
        
        if(distance < lightRadii[i])
        {
            vec3 lightDir = normalize(lightVector);
            float nDotL = max(0.0, dot(normal, lightDir));
            
            // Attenuation (simple linear falloff for Minecraft style)
            float attenuation = 1.0 - (distance / lightRadii[i]);
            
            lighting += lightColors[i] * nDotL * attenuation;
        }
    }
    
    // Clamp lighting to avoid blown out colors
    lighting = clamp(lighting, 0.0, 1.0);
    
    if (texelColor.a < 0.1) discard;
    
    finalColor = vec4(baseColor * lighting, texelColor.a);
}
