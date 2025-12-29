#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 resolution;   // render texture size
uniform float pixelSize;   // size in screen pixels

void main()
{
    vec2 uv = fragTexCoord;

    vec2 pixel = pixelSize / resolution;
    uv = floor(uv / pixel) * pixel;

    finalColor = texture(texture0, uv) * fragColor;
}
