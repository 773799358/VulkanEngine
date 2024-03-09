#pragma once

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "vulkanScene.hpp"
#include <string>
#include <vector>

namespace VulkanEngine
{
	class Model
	{
	public:
		Model(std::string& path, VulkanRenderSceneData* sceneData);
	private:
		std::string directory;
		std::string fileName;
		VulkanRenderSceneData* sceneData = nullptr;

		void loadModel(const std::string& path);
		void processNode(aiNode* aiNode, const aiScene* scene);
		void processMesh(aiMesh* aiMesh, const aiScene* scene, Node* node);
		void processMaterial(aiMaterial* aiMat, const aiScene* scene, aiMesh* aiMesh);
	};
}