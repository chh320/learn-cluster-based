#version 450
layout(location = 0) in vec3 color;

//layout(location = 1) in vec3 coord;
//layout(location = 2) in float isExt;

layout(location = 0) out vec4 fragColor;


void main(){
	fragColor = vec4(color, 1.0);
}