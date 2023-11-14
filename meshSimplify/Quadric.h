#pragma once

#include <memory>
#include <glm/glm.hpp>

namespace Core {
	struct Quadric
	{
		double a2, b2, c2, d2;
		double ab, ac, ad, bc, bd, cd;

		Quadric() {
			std::memset(this, 0, sizeof(double) * 10);
		}

		Quadric(const glm::dvec3& v0, const glm::dvec3& v1, const glm::dvec3& v2) {
			const glm::dvec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
			auto a = normal.x, b = normal.y, c = normal.z;
			double d = -glm::dot(normal, v0);
			a2 = a * a, b2 = b * b, c2 = c * c, d2 = d * d;
			ab = a * b, ac = a * c, ad = a * d, bc = b * c, bd = b * d, cd = c * d;
		}

		void Add(Quadric b) {
			double* t1 = (double*)this;
			double* t2 = (double*)&b;
			for (auto i = 0; i < 10; i++) t1[i] += t2[i];
		}

		bool Get(glm::vec3& p) {
			glm::dmat4 m = {
				{a2, ab, ac, 0},
				{ab, b2, bc, 0},
				{ac, bc, c2, 0},
				{ad, bd, cd, 1}
			};


			glm::dmat4 inv = glm::inverse(m);
			glm::dvec4 v = inv[3];
			p = glm::vec3(v);
			return true;
		}

		float Evaluate(const glm::vec3& p) {
			float result = 
				a2 * p.x * p.x + 2 * ab * p.x * p.y + 2 * ac * p.x * p.z + 2 * ad * p.x
				+ b2 * p.y * p.y + 2 * bc * p.y * p.z + 2 * bd * p.y
				+ c2 * p.z * p.z + 2 * cd * p.z + d2;
			return result <= 0.f ? 0.f : result;
		}
	};
}