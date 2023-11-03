#pragma once

#include <memory>
#include <glm/glm.hpp>

namespace Core {
	class MeshSimplifier {
		MeshSimplifier* pimpl;

	public:
		MeshSimplifier() : pimpl(nullptr){}
		MeshSimplifier(glm::vec3* vertices, uint32_t vertNum, uint32_t* indices, uint32_t indexNum);
		~MeshSimplifier();

		void LockPosition(const glm::vec3& v);
		void Simplify(uint32_t targetTriangleNum);
		uint32_t RemainingVertNum();
		uint32_t RemainingTriangleNum();
		float MaxError();
	};
}