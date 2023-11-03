#pragma once

#include "Bound.h"
#include "Partitioner.h"
#include "HashTable.h"
#include "Util.h"

#include "MeshSimplifier.h"

#include <vector>
#include <glm/glm.hpp>
#include <span>

namespace Core {
	class Cluster final {
	public:
		static const uint32_t clusterSize = 128;

		std::vector<glm::vec3> verts;
		std::vector<uint32_t> indices;
		std::vector<uint32_t> externalEdges;

		Bounds boxBounds;
		Sphere sphereBounds;
		Sphere lodBounds;
		float lodError;
		uint32_t mipLevel;
		uint32_t groupId;

		static void BuildClusters(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, std::vector<Cluster>& clusters);
		static void BuildAdjacentEdgeLink(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, Graph& edgeLink);
		static void BuildAdjacentGraph(const Graph& edgeLink, Graph& graph);
	};

	class ClusterGroup final
	{
	public:
		static const uint32_t clusterGroupSize = 32;

		uint32_t mipLevel;
		std::vector<uint32_t> clusters;
		std::vector<std::pair<uint32_t, uint32_t>> externalEdges;
		Sphere bounds;
		Sphere lodBounds;
		float maxParentLodError;

		static void BuildClusterGroups(std::vector<Cluster>& clusters, uint32_t offset, uint32_t clusterNum, uint32_t mipLevel, std::vector<ClusterGroup>& clusterGroups);
		static void BuildParentClusters(ClusterGroup& clusterGroup, std::vector<Cluster>& clusters);
		static void BuildClustersEdgeLink(std::span<const Cluster> clusters, const std::vector<std::pair<uint32_t, uint32_t>>& externalEdges, Graph& edgeLink);
		static void BuildClustersGraph(const Graph& edgeLink, const std::vector<uint32_t>& edge2Cluster, uint32_t clusterNum, Graph& graph);
	};
}