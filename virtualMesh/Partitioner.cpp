#include "Partitioner.h"

#include <metis.h>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <iostream>

namespace Core {
	struct MetisGraph {
		idx_t nvtxs;
		std::vector<idx_t> xadj;
		std::vector<idx_t> adjncy;		// —πÀıÕº±Ì æ
		std::vector<idx_t> adjwgt;		// edge weight

	};

	typedef uint32_t u32;
	typedef int32_t i32;

	MetisGraph* ToMetisData(const Graph& graph) {
		MetisGraph* mg = new MetisGraph;
		mg->nvtxs = graph.GetGraph().size();
		for (auto& mp : graph.GetGraph()) {
			mg->xadj.push_back(mg->adjncy.size());
			for (auto [to, cost] : mp) {
				mg->adjncy.push_back(to);
				mg->adjwgt.push_back(cost);
			}
		}
		mg->xadj.push_back(mg->adjncy.size());
		return mg;
	}

	uint32_t Partitioner::BisectGraph(MetisGraph* graphData, MetisGraph* childGraphs[2], uint32_t start, uint32_t end) {
		assert(end - start == graphData->nvtxs);

		if (graphData->nvtxs <= _maxPartSize) {
			_ranges.push_back({ start, end });
			return end;
		}

		const uint32_t expectPartSize = (_minPartSize + _maxPartSize) / 2;
		const uint32_t expectNumParts = std::max(2u, (graphData->nvtxs + expectPartSize - 1) / expectPartSize);		// ceiling

		std::vector<idx_t> swapTo(graphData->nvtxs);
		std::vector<idx_t> parts(graphData->nvtxs);

		idx_t nw = 1, npart = 2, ncut = 0;
		real_t partWeight[] = {
			float(expectNumParts >> 1) / expectNumParts,
			1.0 - float(expectNumParts >> 1) / expectNumParts
		};

		int result = METIS_PartGraphRecursive(
			&graphData->nvtxs,			// vertices num
			&nw,						// the link-num of each vertex
			graphData->xadj.data(),		// offset of vertex
			graphData->adjncy.data(),
			nullptr,					// vertices weights
			nullptr,					// vertices size
			graphData->adjwgt.data(),	// edges weight
			&npart,						// target part's num
			partWeight,					// partition weight
			nullptr,
			nullptr,					// options
			&ncut,						// edge cut
			parts.data()
		);
		assert(result == METIS_OK);

		int32_t left = 0, right = graphData->nvtxs - 1;
		while (left <= right) {
			while (left <= right && parts[left] == 0) swapTo[left] = left, left++;
			while (left <= right && parts[right] == 1) swapTo[right] = right, right--;
			if (left < right) {
				std::swap(_nodeId[start + left], _nodeId[start + right]);
				swapTo[left] = right, swapTo[right] = left;
				left++, right--;
			}
		}

		int32_t split = left;

		int32_t splitGraphSize[2] = { split, graphData->nvtxs - split };
		assert(splitGraphSize[0] >= 1 && splitGraphSize[1] >= 1);

		if (splitGraphSize[0] <= _maxPartSize && splitGraphSize[1] <= _maxPartSize) {
			_ranges.push_back({ start, start + split });
			_ranges.push_back({ start + split, end });
		}
		else {
			for (uint32_t i = 0; i < 2; i++) {
				childGraphs[i] = new MetisGraph;
				childGraphs[i]->adjncy.reserve(graphData->adjncy.size() >> 1);
				childGraphs[i]->adjwgt.reserve(graphData->adjwgt.size() >> 1);
				childGraphs[i]->xadj.reserve(splitGraphSize[i] + 1);
				childGraphs[i]->nvtxs = splitGraphSize[i];
			}

			for (uint32_t i = 0; i < graphData->nvtxs; i++) {
				uint32_t isRightChild = (i >= childGraphs[0]->nvtxs);
				uint32_t id = swapTo[i];
				MetisGraph* child = childGraphs[isRightChild];
				child->xadj.push_back(child->adjncy.size());
				for (uint32_t j = graphData->xadj[id]; j < graphData->xadj[id + 1]; j++) {
					idx_t vertId = graphData->adjncy[j];
					idx_t weight = graphData->adjwgt[j];

					vertId = swapTo[vertId] - (isRightChild ? splitGraphSize[0] : 0);

					if (0 <= vertId && vertId < splitGraphSize[isRightChild]) {
						child->adjncy.push_back(vertId);
						child->adjwgt.push_back(weight);
					}
				}
			}

			childGraphs[0]->xadj.push_back(childGraphs[0]->adjncy.size());
			childGraphs[1]->xadj.push_back(childGraphs[1]->adjncy.size());
		}

		return start + split;
	}

	void Partitioner::RecursiveBisectGraph(MetisGraph* graphData, uint32_t start, uint32_t end) {
		MetisGraph* childGraph[2] = { 0 };
		uint32_t split = BisectGraph(graphData, childGraph, start, end);
		delete graphData;

		if (childGraph[0] && childGraph[1]) {
			RecursiveBisectGraph(childGraph[0], start, split);
			RecursiveBisectGraph(childGraph[1], split, end);
		}
		else {
			assert(!childGraph[0] && !childGraph[1]);
		}
	}

	void Partitioner::Init(uint32_t nodeNum) {
		_nodeId.resize(nodeNum);
		_sortTo.resize(nodeNum);
		uint32_t i = 0;
		for (auto& id : _nodeId) {
			id = i++;
		}
		i = 0;
		for (auto& id : _sortTo) {
			id = i++;
		}
	}

	void Partitioner::Partition(const Graph& graph, uint32_t minPartSize, uint32_t maxPartSize) {
		Init(graph.GetGraph().size());
		_minPartSize = minPartSize;
		_maxPartSize = maxPartSize;

		MetisGraph* graphData = ToMetisData(graph);
		RecursiveBisectGraph(graphData, 0, graphData->nvtxs);

		std::sort(_ranges.begin(), _ranges.end());
		for (auto i = 0; i < _nodeId.size(); i++) {
			_sortTo[_nodeId[i]] = i;
		}
	}
}