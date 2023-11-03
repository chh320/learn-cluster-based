#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 localPos;
layout(location = 0) out vec3 color;
layout(location = 1) out vec3 coord;
layout(location = 2) out float isExt;

layout(set=0,binding=0) buffer bindlessData{
    uint data[];
}inputData[];

layout(push_constant) uniform constant{
	uint swapchainId;
	uint swapchainImageNum;
}pushConstants;

struct FrameContext{
	mat4 mvp;
    uint viewMode;	// 0:tri 1:cluster 2:group
    uint level;
	uint displayExtEdge;
};

FrameContext GetFrameContext(){
	uint idx = pushConstants.swapchainId;
	FrameContext context;
	for(int i = 0; i < 4; i++){
		vec4 p;
		p.x = uintBitsToFloat(inputData[idx].data[i * 4]);
		p.y = uintBitsToFloat(inputData[idx].data[i * 4 + 1]);
		p.z = uintBitsToFloat(inputData[idx].data[i * 4 + 2]);
		p.w = uintBitsToFloat(inputData[idx].data[i * 4 + 3]);
		context.mvp[i] = p;
	}
	
	context.viewMode		= inputData[idx].data[16];
	context.level			= inputData[idx].data[17];
	context.displayExtEdge	= inputData[idx].data[18];
	return context;
}

// --------------------------------------------

void main(){
	isExt = 0;
	coord = vec3(0);
	FrameContext frameContext = GetFrameContext();
	color = localPos;
	gl_Position = frameContext.mvp * vec4(localPos, 1.0f);
}