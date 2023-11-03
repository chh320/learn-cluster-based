#pragma once

#include <vector>
#include <map>

namespace Core {
	class Graph final{
	public:
		void Init(uint32_t n) { _graph.resize(n); }
		void AddNode() { _graph.push_back({}); }
		void AddEdge(uint32_t from, uint32_t to, int32_t cost) { _graph[from][to] = cost; }
		void IncreaseEdgeCost(uint32_t from, uint32_t to, int32_t cost) { _graph[from][to] += cost; }

		const std::vector<std::map<uint32_t, int32_t>>& GetGraph() const { return _graph; }

	private:
		std::vector<std::map<uint32_t, int32_t>> _graph;
	};

	struct MetisGraph;

	class Partitioner final{
	public:
		void Init(uint32_t nodeNum);
		void Partition(const Graph& graph, uint32_t minPartSize, uint32_t maxPartSize);

		const std::vector<std::pair<uint32_t, uint32_t>>& GetRanges() const { return _ranges; }
		const uint32_t& GetNodeId(uint32_t id) const { return _nodeId[id]; }
		const uint32_t& GetSortTo(uint32_t id) const { return _sortTo[id]; }

	private:
		std::vector<uint32_t> _nodeId;						// ordered by id
		std::vector<std::pair<uint32_t, uint32_t>> _ranges;	// the range of block
		std::vector<uint32_t> _sortTo;
		uint32_t _minPartSize;
		uint32_t _maxPartSize;

		uint32_t BisectGraph(MetisGraph* graphData, MetisGraph* childGraphs[2], uint32_t start, uint32_t end);
		void RecursiveBisectGraph(MetisGraph* graphData, uint32_t start, uint32_t end);
	};
}