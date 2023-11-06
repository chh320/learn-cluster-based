#version 450
layout(location = 0) in vec3 color;
layout(location = 1) in vec3 normal;
layout(location = 2) in float isPhoneModel;

//layout(location = 1) in vec3 coord;
//layout(location = 2) in float isExt;

layout(location = 0) out vec4 fragColor;


void main(){
    //float t = 1 - min(min(coord.x, coord.y), coord.z);
    //t = min(t, isExt);
    //t = clamp(pow(t + 0.1, 50) + 0.1, 0, 1);

    
    //vec3 c = mix(color, vec3(1), t);
    //if(isPhoneModel == 1)
    vec3 c =  max(dot(normal, vec3(1.0)), 0.0) * vec3(0.5) + vec3(0.2);
    if(isPhoneModel != 0)
        fragColor = vec4(c, 1.0);
    else
	    fragColor = vec4(color, 1.0);
}