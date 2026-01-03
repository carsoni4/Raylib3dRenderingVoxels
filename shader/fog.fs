#version 330

// Input from vertex shader
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragWorldPosition;  // NEW: comes from vertex shader now

// Output
out vec4 finalColor;

// Uniforms
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform float fogDensity;

void main()
{
    // Get base color (texture * vertex color for your lighting)
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 color = texelColor.rgb * fragColor.rgb * colDiffuse.rgb;
    
    // Calculate fog using pre-calculated world position (NO matrix multiplication!)
    float dist = length(viewPos - fragWorldPosition);
    float fogFactor = 1.0 - exp(-pow(fogDensity * dist, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    // Fog color (should match your SKY_COLOR)
    vec3 fogColor = vec3(0.39, 0.39, 0.39); // ~100/255 = gray
    
    // Mix color with fog
    vec3 finalRGB = mix(color, fogColor, fogFactor);
    
    finalColor = vec4(finalRGB, texelColor.a * fragColor.a * colDiffuse.a);
}