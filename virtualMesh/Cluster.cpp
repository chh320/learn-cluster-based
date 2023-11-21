#include "Cluster.h"
#include <unordered_map>

namespace Core {
	void Cluster::BuildClusters(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, std::vector<Cluster>& clusters) {
		Graph edgeLink, graph;
		BuildAdjacentEdgeLink(vertices, indices, edgeLink);
		BuildAdjacentGraph(edgeLink, graph);

		Partitioner partitioner;
		partitioner.Partition(graph, Cluster::clusterSize - 4, Cluster::clusterSize);

		for (auto [left, right] : partitioner.GetRanges()) {
			Cluster cluster;

			std::unordered_map<uint32_t, uint32_t> mp;
			for (uint32_t i = left; i < right; i++) {
				uint32_t triangleId = partitioner.GetNodeId(i);
				for (uint32_t k = 0; k < 3; k++) {
					uint32_t edgeId = triangleId * 3 + k;
					uint32_t vertId = indices[edgeId];
					if (mp.find(vertId) == mp.end()) {
						mp[vertId] = cluster.verts.size();
						cluster.verts.push_back(vertices[vertId]);
					}
					bool isExternal = false;
					bool hasOpposedEdge = edgeLink.GetGraph()[edgeId].size() != 0;
					for (auto [adjEdge, _] : edgeLink.GetGraph()[edgeId]) {
						uint32_t adjTriangle = partitioner.GetSortTo(adjEdge / 3);
						if (adjTriangle < left || adjTriangle >= right) {
							isExternal = true;
							break;
						}
					}
					if (!hasOpposedEdge) isExternal = true;
					if (isExternal) {
						cluster.externalEdges.push_back(cluster.indices.size());
					}
					cluster.indices.push_back(mp[vertId]);
				}
			}

			cluster.mipLevel = 0;
			cluster.lodError = 0;
			cluster.sphereBounds = Sphere::FromPoints(cluster.verts, cluster.verts.size());
			cluster.lodBounds = cluster.sphereBounds;
			cluster.boxBounds = cluster.verts[0];
			cluster.groupId = 0;
			for (auto v : cluster.verts) cluster.boxBounds = cluster.boxBounds + v;

			clusters.push_back(cluster);
		}
	}

	void Cluster::BuildAdjacentEdgeLink(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, Graph& edgeLink) {
		Util::HashTable edgeHashTable(indices.size());
		edgeLink.Init(indices.size());

		for (size_t forwardId = 0; forwardId < indices.size(); forwardId++) {
			glm::vec3 v0 = vertices[indices[forwardId]];
			glm::vec3 v1 = vertices[indices[Util::Cycle3(forwardId)]];

			auto hash0 = Util::HashTable::HashValue(v0);
			auto hash1 = Util::HashTable::HashValue(v1);
			edgeHashTable.Add(Util::HashTable::Murmur32({ hash0, hash1 }), forwardId);

			for (auto backId = edgeHashTable.First(Util::HashTable::Murmur32({ hash1, hash0 })); edgeHashTable.IsValid(backId); backId = edgeHashTable.Next(backId)) {
				if (v1 == vertices[indices[backId]] && v0 == vertices[indices[Util::Cycle3(backId)]]) {
					edgeLink.IncreaseEdgeCost(forwardId, backId, 1);
					edgeLink.IncreaseEdgeCost(backId, forwardId, 1);
				}
			}
		}
	}

	void Cluster::BuildAdjacentGraph(const Graph& edgeLink, Graph& graph) {
		graph.Init(edgeLink.GetGraph().size() / 3);		// triangle's nums;
		uint32_t edge0 = 0;
		for (const auto& mp : edgeLink.GetGraph()) {
			for (auto [edge1, cost] : mp) {
				graph.IncreaseEdgeCost(edge0 / 3, edge1 / 3, 1);
			}
			edge0++;
		}
	}


	void ClusterGroup::BuildClusterGroups(std::vector<Cluster>& clusters, uint32_t offset, uint32_t clusterNum, uint32_t mipLevel, std::vector<ClusterGroup>& clusterGroups) {
		std::span<const Cluster> clustersView(clusters.begin() + offset, clusterNum);

		std::vector<uint32_t> edge2Cluster;
		std::vector<uint32_t> cluster2Edge;
		std::vector<std::pair<uint32_t, uint32_t>> externalEdges;

		uint32_t clusterId = 0;
		for (auto& cluster : clustersView) {
			assert(cluster.mipLevel == mipLevel);
			cluster2Edge.push_back(edge2Cluster.size());

			for (auto edgeId : cluster.externalEdges) {
				externalEdges.push_back({ clusterId, edgeId });
				edge2Cluster.push_back(clusterId);
			}
			clusterId++;
		}

		Graph edgeLink, graph;
		BuildClustersEdgeLink(clustersView, externalEdges, edgeLink);
		BuildClustersGraph(edgeLink, edge2Cluster, clusterNum, graph);

		Partitioner partitioner;
		partitioner.Partition(graph, ClusterGroup::maxClusterGroupSize - 4, ClusterGroup::maxClusterGroupSize);


		
		for (auto[left, right] : partitioner.GetRanges()) {
			ClusterGroup clusterGroup;
			clusterGroup.mipLevel = mipLevel;

			for (auto i = left; i < right; i++) {
				uint32_t clusterId = partitioner.GetNodeId(i);
				//clusters[clusterId + offset].groupId = clusterGroups.size();
				clusterGroup.clusters.push_back(clusterId + offset);

				for (uint32_t edgeId = cluster2Edge[clusterId]; edgeId < edge2Cluster.size() && edge2Cluster[edgeId] == clusterId; edgeId++) {
					bool isExternal = false;
					bool hasOpposedEdge = edgeLink.GetGraph()[edgeId].size() != 0;
					for (auto [adj, _] : edgeLink.GetGraph()[edgeId]) {
						auto adjCluster = partitioner.GetSortTo(edge2Cluster[adj]);
						if (adjCluster < left || adjCluster >= right) {
							isExternal = true;
							break;
						}
					}
					if (!hasOpposedEdge) isExternal = true;
					if (isExternal) {
						uint32_t realEdgeId = externalEdges[edgeId].second;
						clusterGroup.externalEdges.push_back(std::pair{ clusterId + offset, realEdgeId});
					}
				}
			}
			clusterGroups.push_back(clusterGroup);
		}
	}

	

	void ClusterGroup::BuildParentClusters(uint32_t groupId, ClusterGroup& clusterGroup, std::vector<Cluster>& clusters) {
		std::vector<glm::vec3> vertices;
		std::vector<uint32_t> indices;
		std::vector<Sphere> lodBounds;
		float maxParentLodError = 0;
		uint32_t idOffset = 0;

		for (uint32_t clusterId : clusterGroup.clusters) {
			auto& cluster = clusters[clusterId];
			cluster.groupId = groupId;
			for (const auto& v : cluster.verts) vertices.push_back(v);
			for (const auto& id : cluster.indices) indices.push_back(id + idOffset);
			idOffset += cluster.verts.size();
			lodBounds.push_back(cluster.lodBounds);
			maxParentLodError = std::max(maxParentLodError, cluster.lodError);
		}
		Sphere parentLodBound = Sphere::FromSpheres(lodBounds, lodBounds.size());

		MeshSimplifier meshSimplifier(vertices.data(), vertices.size(), indices.data(), indices.size());

		Util::HashTable edgeHashTable(clusterGroup.externalEdges.size());

		uint32_t i = 0;
		for (auto [clusterId, edgeId] : clusterGroup.externalEdges) {
			auto& vertices = clusters[clusterId].verts;
			auto& indices = clusters[clusterId].indices;

			const auto& v0 = vertices[indices[edgeId]];
			const auto& v1 = vertices[indices[Util::Cycle3(edgeId)]];

			auto hash0 = Util::HashTable::HashValue(v0);
			auto hash1 = Util::HashTable::HashValue(v1);

			edgeHashTable.Add(Util::HashTable::Murmur32({ hash0, hash1 }), i);
			meshSimplifier.LockPosition(v0);
			meshSimplifier.LockPosition(v1);

			i++;
		}
		meshSimplifier.Simplify((Cluster::clusterSize - 2) * (clusterGroup.clusters.size() / 2));
		vertices.resize(meshSimplifier.RemainingVertNum());
		indices.resize(meshSimplifier.RemainingTriangleNum() * 3);

		maxParentLodError = std::max(maxParentLodError, std::sqrt(meshSimplifier.MaxError()));

		Graph edgeLink, graph;
		Cluster::BuildAdjacentEdgeLink(vertices, indices, edgeLink);
		Cluster::BuildAdjacentGraph(edgeLink, graph);

		Partitioner partitioner;
		partitioner.Partition(graph, Cluster::clusterSize - 4, Cluster::clusterSize);

		for (auto [left, right] : partitioner.GetRanges()) {
			//std::cout << left << " " << right << " " << right - left + 1 << "\n";
			Cluster cluster;
			std::unordered_map<uint32_t, uint32_t> mp;
			for (uint32_t i = left; i < right; i++) {
				uint32_t triangleId = partitioner.GetNodeId(i);
				for (uint32_t k = 0; k < 3; k++) {
					uint32_t edgeId = triangleId * 3 + k;
					uint32_t vertId = indices[edgeId];
					if (mp.find(vertId) == mp.end()) {
						mp[vertId] = cluster.verts.size();
						cluster.verts.push_back(vertices[vertId]);
					}
					bool isExternal = false;
					for (auto [adjEdge, _] : edgeLink.GetGraph()[edgeId]) {
						uint32_t adjTriangle = partitioner.GetSortTo(adjEdge / 3);
						if (adjTriangle < left || adjTriangle >= right) {
							isExternal = true;
							break;
						}
					}

					glm::vec3 v0 = vertices[vertId];
					glm::vec3 v1 = vertices[indices[Util::Cycle3(edgeId)]];
					if (!isExternal) {
						auto hash0 = Util::HashTable::HashValue(v0);
						auto hash1 = Util::HashTable::HashValue(v1);
						auto hashValue = Util::HashTable::Murmur32({ hash0, hash1 });
						for (auto j = edgeHashTable.First(hashValue); edgeHashTable.IsValid(j); j = edgeHashTable.Next(j)) {
							auto [clusterId, edgeId] = clusterGroup.externalEdges[j];
							auto& vertices = clusters[clusterId].verts;
							auto& indices = clusters[clusterId].indices;
							if (v0 == vertices[indices[edgeId]] && v1 == vertices[indices[Util::Cycle3(edgeId)]]) {
								isExternal = true;
								break;
							}
						}
					}

					if (isExternal) {
						cluster.externalEdges.push_back(cluster.indices.size());
					}
					cluster.indices.push_back(mp[vertId]);
				}
			}

			cluster.mipLevel = clusterGroup.mipLevel + 1;
			cluster.lodError = maxParentLodError;
			cluster.sphereBounds = Sphere::FromPoints(cluster.verts, cluster.verts.size());
			cluster.lodBounds = parentLodBound;
			cluster.boxBounds = cluster.verts[0];
			cluster.groupId = groupId + 1;
			for (auto v : cluster.verts) cluster.boxBounds = cluster.boxBounds + v;

			clusters.push_back(cluster);
		}
		clusterGroup.lodBounds = parentLodBound;
		clusterGroup.maxParentLodError = maxParentLodError;
	}

	void ClusterGroup::BuildClustersEdgeLink(std::span<const Cluster> clusters, const std::vector<std::pair<uint32_t, uint32_t>>& externalEdges, Graph& edgeLink) {
		Util::HashTable edgeHashTable(externalEdges.size());
		edgeLink.Init(externalEdges.size());

		uint32_t forwardId = 0;
		for (auto [clusterId, edgeId] : externalEdges) {

			auto& vertices = clusters[clusterId].verts;
			auto& indices = clusters[clusterId].indices;

			const glm::vec3& v0 = vertices[indices[edgeId]];
			const glm::vec3& v1 = vertices[indices[Util::Cycle3(edgeId)]];

			auto hash0 = Util::HashTable::HashValue(v0);
			auto hash1 = Util::HashTable::HashValue(v1);
			edgeHashTable.Add(Util::HashTable::Murmur32({ hash0, hash1 }), forwardId);

			for (auto backId = edgeHashTable.First(Util::HashTable::Murmur32({ hash1, hash0 })); edgeHashTable.IsValid(backId); backId = edgeHashTable.Next(backId)) {
				auto [clusterId2, edgeId2] = externalEdges[backId];
				auto& vertices2 = clusters[clusterId2].verts;
				auto& indices2 = clusters[clusterId2].indices;

				if (vertices2[indices2[edgeId2]] == v1 && vertices2[indices2[Util::Cycle3(edgeId2)]] == v0) {
					edgeLink.IncreaseEdgeCost(forwardId, backId, 1);
					edgeLink.IncreaseEdgeCost(backId, forwardId, 1);
				}
			}
			forwardId++;
		}
	}

	void ClusterGroup::BuildClustersGraph(const Graph& edgeLink, const std::vector<uint32_t>& edge2Cluster, uint32_t clusterNum, Graph& graph) {
		graph.Init(clusterNum);		// triangle's nums;
		uint32_t edge0 = 0;
		for (const auto& mp : edgeLink.GetGraph()) {
			for (auto [edge1, cost] : mp) {
				graph.IncreaseEdgeCost(edge2Cluster[edge0], edge2Cluster[edge1], 1);
			}
			edge0++;
		}
	}
}