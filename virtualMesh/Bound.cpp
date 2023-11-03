#include "Bound.h"

namespace Core {
	Bounds Bounds::operator+(const glm::vec3& other) {
		Bounds bounds;
		for (uint32_t i = 0; i < 3; i++) {
			bounds.pMin[i] = std::min(other[i], bounds.pMin[i]);
			bounds.pMax[i] = std::max(other[i], bounds.pMax[i]);
		}
		return bounds;
	}

	Bounds Bounds::operator+(const Bounds& other) {
		Bounds bounds;
		for (uint32_t i = 0; i < 3; i++) {
			bounds.pMin[i] = std::min(other.pMin[i], bounds.pMin[i]);
			bounds.pMax[i] = std::max(other.pMax[i], bounds.pMax[i]);
		}
		return bounds;
	}

	Sphere Sphere::operator+(const Sphere& other) {
		glm::vec3 v = other.center - center;
		float len = glm::length(v);
		if (std::abs(radius - other.radius) >= len) {	// one contain the other.
			return radius < other.radius ? other : *this;
		}
		Sphere sphere;
		sphere.radius = (len + radius + other.radius) * 0.5;
		sphere.center = center + v * ((sphere.radius - radius) / len);
		return sphere;
	}

	Sphere Sphere::FromPoints(const std::vector<glm::vec3>& pos, uint32_t size) {
		std::vector<uint32_t> minIdx(3);
		std::vector<uint32_t> maxIdx(3);
		for (auto i = 0; i < size; i++) {
			for (auto k = 0; k < 3; k++) {
				if (pos[i][k] < pos[minIdx[k]][k]) minIdx[k] = i;
				if (pos[i][k] > pos[maxIdx[k]][k]) maxIdx[k] = i;
			}
		}

		float maxLen = 0;
		uint32_t maxAxis = 0;
		for (auto k = 0; k < 3; k++) {
			glm::vec3 pMin = pos[minIdx[k]];
			glm::vec3 pMax = pos[maxIdx[k]];
			float len = glm::length(pMax - pMin);
			if (len > maxLen) {
				maxLen = len;
				maxAxis = k;
			}
		}
		glm::vec3 pMin = pos[minIdx[maxAxis]];
		glm::vec3 pMax = pos[maxIdx[maxAxis]];

		Sphere sphere;
		sphere.center = (pMax + pMin) * 0.5f;
		sphere.radius = maxLen * 0.5f;

		maxLen = sphere.radius;

		for (auto i = 0; i < size; i++) {
			float len = glm::length(pos[i] - sphere.center);
			if (len > maxLen) {
				float t = 0.5 * (len - sphere.radius) / len;
				sphere.center = sphere.center + (pos[i] - sphere.center) * t;
				sphere.radius = (sphere.radius + len) * 0.5f;
				maxLen = len;
			}
		}

		return sphere;
	}

	Sphere Sphere::FromSpheres(const std::vector<Sphere>& spheres, uint32_t size) {
		std::vector<uint32_t> minIdx(3);
		std::vector<uint32_t> maxIdx(3);
		for (auto i = 0; i < size; i++) {
			for (auto k = 0; k < 3; k++) {
				if (spheres[i].center[k] - spheres[i].radius < spheres[minIdx[k]].center[k] - spheres[minIdx[k]].radius)
					minIdx[k] = i;
				if (spheres[i].center[k] + spheres[i].radius < spheres[maxIdx[k]].center[k] + spheres[maxIdx[k]].radius)
					maxIdx[k] = i;
			}
		}

		float maxLen = 0;
		uint32_t maxAxis = 0;
		for (auto k = 0; k < 3; k++) {
			Sphere sMin = spheres[minIdx[k]];
			Sphere sMax = spheres[maxIdx[k]];
			float len = glm::length(sMax.center - sMin.center) + sMax.radius + sMin.radius;
			if (len > maxLen) {
				maxLen = len;
				maxAxis = k;
			}
		}

		Sphere sphere = spheres[minIdx[maxAxis]];
		sphere = sphere + spheres[maxIdx[maxAxis]];

		for (auto i = 0; i < size; i++) {
			sphere = sphere + spheres[i];
		}
		return sphere;
	}
}