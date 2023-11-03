#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Core {
	bool Mesh::LoadMesh(std::string filePath) {
		Assimp::Importer importer;
		auto scene = importer.ReadFile(filePath.c_str(), 0);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::cerr << "Error loading model: " << importer.GetErrorString() << std::endl;
			return false;
		}

		for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
			aiMesh* mesh = scene->mMeshes[i];
			auto vertexCnt = mesh->mNumVertices;
			auto indiceCnt = mesh->mNumFaces * 3;

			vertices.resize(vertexCnt);
			//std::vector<glm::vec3>().swap(vertices);
			for (unsigned int i = 0; i < vertexCnt; i++) {
				//vertices.emplace_back(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
				vertices[i].x = mesh->mVertices[i].x;
				vertices[i].y = mesh->mVertices[i].y;
				vertices[i].z = mesh->mVertices[i].z;
			}

			indices.resize(indiceCnt);
			//std::vector<uint32_t>().swap(indices);
			for (unsigned int i = 0; i < indiceCnt; i += 3) {
				for (int j = 0; j < 3; j++) {
					indices[j] = mesh->mFaces[i / 3].mIndices[j];
					//indices.emplace_back(mesh->mFaces[i / 3].mIndices[j]);
				}
			}
		}
		return true;
	}

	bool Mesh::SimplifyMesh() {
		Util::HashTable verticeHT(vertices.size());
		std::vector<glm::vec3> remainVertNum;
		std::vector<uint32_t> oldIdToNewId(vertices.size());

		for (uint32_t i = 0; i < vertices.size(); i++) {
			auto firstId = verticeHT.First(Util::HashTable::HashValue(vertices[i]));
			bool hasSameVertex = false;
			for (auto id = firstId; verticeHT.IsValid(id); id = verticeHT.Next(id)) {
				if (vertices[id] == vertices[i]) {
					hasSameVertex = true;
					indices[i] = oldIdToNewId[id];
					break;
				}
			}
			if (!hasSameVertex) {
				verticeHT.Add(Util::HashTable::HashValue(vertices[i]), i);
				remainVertNum.emplace_back(vertices[i]);
				oldIdToNewId[i] = remainVertNum.size() - 1;
				indices[i] = oldIdToNewId[i];
			}
		}

		remainVertNum.swap(vertices);
		//int maxN = 0;
		//for (int i = 0; i < indices.size(); i++) maxN = maxN > indices[i] ? maxN : indices[i];
		//std::cout << maxN  << " " << remainVertNum.size() << "\n";
		return true;
	}
}