#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Core {
	struct Bounds
	{
		glm::vec3 pMin;
		glm::vec3 pMax;

		Bounds() : pMin(glm::vec3(FLT_MAX)), pMax(glm::vec3(-FLT_MAX)){}
		Bounds(const glm::vec3& p) : pMin(p), pMax(p){}
		Bounds operator+(const glm::vec3& other);
		Bounds operator+(const Bounds& other);
	};

	struct Sphere {
		glm::vec3 center;
		float radius;

		Sphere operator+(const Sphere& other);
		static Sphere FromPoints(const std::vector<glm::vec3>& pos, uint32_t size);
		static Sphere FromSpheres(const std::vector<Sphere>& spheres, uint32_t size);
	};
}