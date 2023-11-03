#version 450
#extension  GL_ARB_separate_shader_objects:enable
#extension GL_EXT_nonuniform_qualifier : enable

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

struct Cluster{
	uint triangleNum;
	uint verticesNum;
	uint groupId;
	uint mipLevel;

	vec4 bounds;

	uint indexOffset;
	uint posOffset;
	uint isExternalEdgeOffset;

	uint clusterId;
	uint bufferId;

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

Cluster GetCluster(uint clusterId){
	Cluster cluster;
	uint idx = clusterId + pushConstants.swapchainImageNum;

	cluster.triangleNum = inputData[nonuniformEXT(idx)].data[0];
	cluster.verticesNum = inputData[nonuniformEXT(idx)].data[1];
	cluster.groupId = inputData[nonuniformEXT(idx)].data[2];
	cluster.mipLevel = inputData[nonuniformEXT(idx)].data[3];

	cluster.bounds.x = uintBitsToFloat(inputData[nonuniformEXT(idx)].data[4]);
	cluster.bounds.y = uintBitsToFloat(inputData[nonuniformEXT(idx)].data[5]);
	cluster.bounds.z = uintBitsToFloat(inputData[nonuniformEXT(idx)].data[6]);
	cluster.bounds.w = uintBitsToFloat(inputData[nonuniformEXT(idx)].data[7]);

	cluster.indexOffset = 8;						// indice data is at offset 8 of each triangle's data in cluster.
	cluster.posOffset = 8 + cluster.triangleNum;	// There are 8 data before this, and the size of the index offset is the number of triangles

	cluster.clusterId = clusterId;
	cluster.bufferId = idx;
	cluster.isExternalEdgeOffset = 8 + cluster.triangleNum + cluster.verticesNum * 3;

	return cluster;
}

vec3 GetPosition(Cluster cluster, uint id){
	uint triangleId = id / 3;
	uint triangleData = inputData[nonuniformEXT(cluster.bufferId)].data[cluster.indexOffset + triangleId];
	uint vertId = ((triangleData >> (id % 3 * 8)) & 255);

	vec3 p;
	p.x = uintBitsToFloat(inputData[nonuniformEXT(cluster.bufferId)].data[cluster.posOffset + vertId * 3 + 0]);
	p.y = uintBitsToFloat(inputData[nonuniformEXT(cluster.bufferId)].data[cluster.posOffset + vertId * 3 + 1]);
	p.z = uintBitsToFloat(inputData[nonuniformEXT(cluster.bufferId)].data[cluster.posOffset + vertId * 3 + 2]);
	return p;
}

// --------------------------------------------

uint Cycle3(uint i){
    uint imod3 = i % 3;
    return i - imod3 + ((1 << imod3) & 3);
}

uint Cycle3(uint i, uint offset){
    return i - i % 3 + (i + offset) % 3;
}

uint MurmurMix(uint Hash){
	Hash ^= Hash >> 16;
	Hash *= 0x85ebca6b;
	Hash ^= Hash >> 13;
	Hash *= 0xc2b2ae35;
	Hash ^= Hash >> 16;
	return Hash;
}

vec3 Id2Color(uint id){
	uint Hash = MurmurMix(id + 1);

	vec3 color = vec3(
		(Hash >> 0) & 255,
		(Hash >> 8) & 255,
		(Hash >> 16) & 255
	);

	return color * (1.0f / 255.0f);
}

// --------------------------------------------

void main(){
	uint clusterId = gl_InstanceIndex;
	uint indexId = gl_VertexIndex;
	uint triangleId = indexId / 3;

	FrameContext frameContext = GetFrameContext();
	Cluster cluster = GetCluster(clusterId);

	if(triangleId >= cluster.triangleNum || cluster.mipLevel != frameContext.level){
		gl_Position.z = 0 / 0; //discard
		return;
	}

	vec3 p;
	p = GetPosition(cluster, indexId);

	if(frameContext.viewMode == 0) color = Id2Color(triangleId);
	else if(frameContext.viewMode == 1) color = Id2Color(clusterId);
	else if(frameContext.viewMode == 2) color = Id2Color(cluster.groupId);

	isExt = 0;
	coord = vec3(0);
	if(frameContext.displayExtEdge != 0){
		uint isExtEdge = inputData[nonuniformEXT(cluster.bufferId)].data[cluster.isExternalEdgeOffset + indexId];
		isExtEdge |= inputData[nonuniformEXT(cluster.bufferId)].data[cluster.isExternalEdgeOffset + Cycle3(indexId, 2)];

		isExt = isExtEdge;
		coord[gl_VertexIndex % 3] = 1;
	}

	vec4 worldPos = frameContext.mvp * vec4(p, 1.0f);
	worldPos = worldPos / worldPos.w;
	if(worldPos.z < 0 || worldPos.z > 1) worldPos.z = 0 / 0;
	gl_Position = worldPos;
}