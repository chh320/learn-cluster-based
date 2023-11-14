#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D hizMipMap[];

layout(push_constant) uniform constant{
    uint wWidth;
    uint wHeight;
    uint maxMipSize;
    uint lastMipLevel;
}pushConstant;

ivec2 Mip1ToMip0(ivec2 p){
    return p * ivec2(pushConstant.wWidth, pushConstant.wHeight) / ivec2(pushConstant.maxMipSize, pushConstant.maxMipSize);
}

void main(){
    ivec2 p = ivec2(gl_FragCoord);
    if(pushConstant.lastMipLevel == 0) p = Mip1ToMip0(p);
    else p = 2 * p;

    float x = texture(hizMipMap[pushConstant.lastMipLevel], p + 0.5 + ivec2(0,0)).x;
    float y = texture(hizMipMap[pushConstant.lastMipLevel], p + 0.5 + ivec2(1,0)).x;
    float z = texture(hizMipMap[pushConstant.lastMipLevel], p + 0.5 + ivec2(1,1)).x;
    float w = texture(hizMipMap[pushConstant.lastMipLevel], p + 0.5 + ivec2(0,1)).x;

    float minZ = min(min(x, y), min(w, z));
    gl_FragDepth = minZ;
}