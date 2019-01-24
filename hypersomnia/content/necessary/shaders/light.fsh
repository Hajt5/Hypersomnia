#version 140
precision mediump int;
precision mediump float;

smooth in vec4 theColor;

out vec4 outputColor;

uniform vec2 light_pos;
uniform vec3 light_attenuation;
uniform vec3 multiply_color;
uniform float distance_mult;

void main() 
{	
	float light_distance = length(gl_FragCoord.xy - light_pos) * distance_mult;
	vec4 final_color = theColor;
	final_color.rgb *= multiply_color;

	final_color.a *= 1.0 / (
		light_attenuation.x
		+ light_attenuation.y * light_distance
		+ light_attenuation.z * light_distance * light_distance
	); 

	outputColor = final_color;
}