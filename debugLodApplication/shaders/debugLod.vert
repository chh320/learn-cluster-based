#version 450
#extension  GL_ARB_separate_shader_objects:enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out vec3 color;

//layout(location = 1) out vec3 coord;
//layout(location = 2) out float isExt;

layout(set=0,binding=0) buffer bindlessData{
    uint data[];
}inputData[];

layout(push_constant) uniform constant{
	uint swapchainId;
    uint imageCnt;
}pushConstants;

struct FrameContext{
	mat4 mvp;
    vec4 viewDir;
    uint viewMode;	// 0:triangle 1:cluster 2:group 3:mipLevel 4: phone-model
    uint mipLevel;
};

struct Cluster{
    uint verticesNum;
    uint vertOffset;
	uint triangleNum;
    uint indexOffset;

	uint groupId;
	uint mipLevel;
};

uint GetImageNum(){
    return pushConstants.imageCnt;
}

FrameContext GetFrameContext(){
	uint idx = pushConstants.swapchainId + 1 + 2 * GetImageNum();
	FrameContext context;
	for(int i = 0; i < 4; i++){
		vec4 p;
		p.x = uintBitsToFloat(inputData[idx].data[i * 4]);
		p.y = uintBitsToFloat(inputData[idx].data[i * 4 + 1]);
		p.z = uintBitsToFloat(inputData[idx].data[i * 4 + 2]);
		p.w = uintBitsToFloat(inputData[idx].data[i * 4 + 3]);
		context.mvp[i] = p;
	}
    for(int i = 0; i < 4; i++){
		context.viewDir[i] = uintBitsToFloat(inputData[idx].data[16 + i]);
	}
	context.viewMode = inputData[idx].data[20];
    context.mipLevel = inputData[idx].data[21];
	return context;
}

Cluster GetCluster(uint clusterId){
	Cluster cluster;
	uint idx = 1 + 3 * GetImageNum();
    uint offset = 4 + 20 * clusterId;

	cluster.verticesNum         = inputData[idx].data[offset + 0];
    cluster.vertOffset          = inputData[idx].data[offset + 1];
	cluster.triangleNum         = inputData[idx].data[offset + 2];
    cluster.indexOffset         = inputData[idx].data[offset + 3];

	cluster.groupId             = inputData[idx].data[offset + 18];
	cluster.mipLevel            = inputData[idx].data[offset + 19];

	return cluster;
}

vec3 GetPosition(Cluster cluster, uint index){
	uint triangleId = index / 3;
    uint id = 1 + 3 * GetImageNum();
	uint triangleData = inputData[id].data[cluster.indexOffset + triangleId];
	uint vertId = ((triangleData >> (index % 3 * 8)) & 255);

	vec3 p;
	p.x = uintBitsToFloat(inputData[id].data[cluster.vertOffset + vertId * 3 + 0]);
	p.y = uintBitsToFloat(inputData[id].data[cluster.vertOffset + vertId * 3 + 1]);
	p.z = uintBitsToFloat(inputData[id].data[cluster.vertOffset + vertId * 3 + 2]);
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
	uint clusterId  = gl_InstanceIndex;
	uint indexId    = gl_VertexIndex;
	uint triangleId = indexId / 3;

	FrameContext frameContext = GetFrameContext();
	Cluster cluster = GetCluster(clusterId);

	if(triangleId >= cluster.triangleNum || cluster.mipLevel != frameContext.mipLevel){
		gl_Position.z = 0 / 0; //discard
		return;
	}

    vec3 p      = GetPosition(cluster, indexId);
    uint id2    = Cycle3(indexId, 1);
    uint id3    = Cycle3(indexId, 2);
    vec3 p1     = GetPosition(cluster, id2);
    vec3 p2     = GetPosition(cluster, id3);
    vec3 normal = normalize(cross(p1 - p, p2 - p));

	if      (frameContext.viewMode == 0) color = Id2Color(triangleId);
	else if (frameContext.viewMode == 2) color = Id2Color(clusterId);
	else if (frameContext.viewMode == 3) color = Id2Color(cluster.groupId);
    else if (frameContext.viewMode == 4) color = Id2Color(cluster.mipLevel);
    else if (frameContext.viewMode == 1) color = abs(dot(normal, -vec3(frameContext.viewDir))) * vec3(0.8) + vec3(0.2);

    vec4 pos = frameContext.mvp * vec4(p, 1.0f);
    pos = pos / pos.w;
    if(pos.z<0||pos.z>1) pos.z=0/0;
	gl_Position = frameContext.mvp * vec4(p, 1.0f);
}