#version 400 core

//https://www.shadertoy.com/view/Xltfzj

uniform sampler2D u_InputTexture;

out float FragColor;
in vec2 v_TexCoords;

const float Pi = 6.28318530718; // Pi*2

// GAUSSIAN BLUR SETTINGS {{{
uniform float Directions = 15.0; // BLUR DIRECTIONS (Default 16.0 - More is better but slower)
uniform float Quality = 2.0; // BLUR QUALITY (Default 4.0 - More is better but slower)
uniform float Size = 5.0; // BLUR SIZE (Radius)
// GAUSSIAN BLUR SETTINGS }}}

void main(void) {

	vec2 iResolution = vec2(1920.0, 1080.0);

    vec2 Radius = Size/iResolution.xy;

    // Pixel colour
    vec4 Color = texture(u_InputTexture, v_TexCoords);
    
    // Blur calculations
    for( float d=0.0; d<Pi; d+=Pi/Directions)
    {
		for(float i=1.0/Quality; i<=1.0; i+=1.0/Quality)
        {
			Color += texture( u_InputTexture, v_TexCoords + vec2(cos(d),sin(d))*Radius*i);		
        }
    }
    
    // Output to screen
    Color /= Quality * Directions - 15.0;
    FragColor =  Color.r;
}
