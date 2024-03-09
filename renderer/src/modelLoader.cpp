#include "modelLoader.hpp"
#include "macro.hpp"
#include "vulkanUtil.hpp"

namespace VulkanEngine
{
	Model::Model(std::string& path, VulkanRenderSceneData* sceneData) : sceneData(sceneData)
	{
		loadModel(path);
	}

	glm::mat4 toGLMMat4(aiMatrix4x4& aiMat)
	{
		glm::mat4 mat;

		for (int i = 0; i < 4; ++i) 
		{
			for (int j = 0; j < 4; ++j) 
			{
				mat[i][j] = static_cast<float>(aiMat[j][i]);
			}
		}

		return mat;
	}

	std::unordered_map<aiNode*, Node*> nodes;
	std::unordered_map<aiMesh*, Mesh*> meshes;
	std::unordered_map<aiMaterial*, PBRMaterial*> materials;
	std::unordered_map<std::string, Texture*> textures;

	Node* getNode(aiNode* aiNode)
	{
		auto iter = nodes.find(aiNode);
		if (iter != nodes.end())
		{
			return iter->second;
		}
		return nullptr;
	}

	Node* createOrGetNode(aiNode* aiNode)
	{
		if (aiNode == nullptr)
		{
			return nullptr;
		}
		Node* findNode = getNode(aiNode);
		if (findNode == nullptr)
		{
			Node* node = new Node();
			node->name = aiNode->mName.C_Str();
			node->localTransform = toGLMMat4(aiNode->mTransformation);

			nodes[aiNode] = node;
			return node;
		}
		return findNode;
	}

	Texture* createOrGetTexture(const std::string& path)
	{
		if (path.empty())
		{
			return nullptr;
		}
		auto& iter = textures.find(path);
		if (iter != textures.end())
		{
			return iter->second;
		}
		Texture* texture = new Texture();
		texture->path = path;

		textures[path] = texture;
		return texture;
	}

	PBRMaterial* createOrGetMaterial(aiMaterial* aiMaterial)
	{
		if (aiMaterial == nullptr)
		{
			return nullptr;
		}
		auto& iter = materials.find(aiMaterial);
		if (iter != materials.end())
		{
			return iter->second;
		}
		PBRMaterial* material = new PBRMaterial();

		materials[aiMaterial] = material;
		return material;
	}

	Mesh* createOrGetMesh(aiMesh* aiMesh)
	{
		if (aiMesh == nullptr)
		{
			return nullptr;
		}
		auto& iter = meshes.find(aiMesh);
		if (iter != meshes.end())
		{
			return iter->second;
		}
		Mesh* mesh = new Mesh();

		meshes[aiMesh] = mesh;
		return mesh;
	}

	void Model::loadModel(const std::string& path)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			LOG_ERROR("ASSIMP:: {}", importer.GetErrorString());
			return;
		}
		directory = path.substr(0, path.find_last_of('/'));
		fileName = path.substr(path.find_last_of('/') + 1);
		fileName = fileName.substr(0, fileName.find_last_of('.'));

		Node* root = createOrGetNode(scene->mRootNode);
		root->parent = nullptr;
		processNode(scene->mRootNode, scene);

		for (auto& iter = nodes.begin(); iter != nodes.end(); iter++)
		{
			Node* node = iter->second;

			if (node != nullptr) 
			{
				glm::mat4 worldTransform = glm::mat4(1.0f);
				Node* parent = node;
				while (parent->parent != nullptr)
				{
					worldTransform = parent->localTransform * worldTransform;
					parent = parent->parent;
				}
				node->worldTransform = worldTransform;
				sceneData->nodes.push_back(iter->second);
			}
		}

		for (auto& iter = meshes.begin(); iter != meshes.end(); iter++)
		{
			sceneData->meshes.push_back(iter->second);
		}

		for (auto& iter = materials.begin(); iter != materials.end(); iter++)
		{
			sceneData->materials.push_back(iter->second);
		}

		for (auto& iter = textures.begin(); iter != textures.end(); iter++)
		{
			iter->second->fullPath = directory + '/' + iter->second->path;
			sceneData->textures.push_back(iter->second);
		}
	}

	void Model::processNode(aiNode* aiNode, const aiScene* scene)
	{
		Node* node = createOrGetNode(aiNode);
		node->parent = createOrGetNode(aiNode->mParent);
		for (size_t i = 0; i < aiNode->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[aiNode->mMeshes[i]];
			if (mesh != nullptr)
			{
				processMesh(mesh, scene, node);
			}
		}
		node->children.resize(aiNode->mNumChildren);
		for (size_t i = 0; i < aiNode->mNumChildren; i++)
		{
			node->children[i] = createOrGetNode(aiNode->mChildren[i]);
			processNode(aiNode->mChildren[i], scene);
		}
	}

	void Model::processMesh(aiMesh* aiMesh, const aiScene* scene, Node* node)
	{
		Mesh* mesh = createOrGetMesh(aiMesh);
		
		if (mesh->node == nullptr)
		{
			mesh->node = node;

			mesh->vertices.resize(aiMesh->mNumVertices);
			for (size_t i = 0; i < aiMesh->mNumVertices; i++)
			{
				Vertex& vertex = mesh->vertices[i];
				vertex.position.x = aiMesh->mVertices[i].x;
				vertex.position.y = aiMesh->mVertices[i].y;
				vertex.position.z = aiMesh->mVertices[i].z;

				vertex.normal.x = aiMesh->mNormals[i].x;
				vertex.normal.y = aiMesh->mNormals[i].y;
				vertex.normal.z = aiMesh->mNormals[i].z;

				if (aiMesh->mTextureCoords[0])
				{
					vertex.texcoord.x = aiMesh->mTextureCoords[0][i].x;
					vertex.texcoord.y = aiMesh->mTextureCoords[0][i].y;

					vertex.tangent.x = aiMesh->mTangents[i].x;
					vertex.tangent.y = aiMesh->mTangents[i].y;
					vertex.tangent.z = aiMesh->mTangents[i].z;
				}
				else
				{
					vertex.texcoord = { 0, 0 };
					vertex.tangent = { 0, 0, 0 };
				}

				// TODO: 顶点色基本自己的测试模型才会用到
				vertex.color.x = 1.0f;
				vertex.color.y = 1.0f;
				vertex.color.z = 1.0f;
			}

			for (size_t i = 0; i < aiMesh->mNumFaces; i++)
			{
				aiFace& face = aiMesh->mFaces[i];
				for (size_t j = 0; j < face.mNumIndices; j++)
				{
					mesh->indices.push_back(face.mIndices[j]);
				}
			}

			if (aiMesh->mMaterialIndex >= 0)
			{
				aiMaterial* aiMaterial = scene->mMaterials[aiMesh->mMaterialIndex];
				processMaterial(aiMaterial, scene, aiMesh);
			}

			if (mesh->material == nullptr)
			{
				mesh->material = sceneData->materials[0];
			}
		}
	}

	void Model::processMaterial(aiMaterial* aiMat, const aiScene* scene, aiMesh* aiMesh)
	{
		auto getTexture = [&](aiMaterial* aiMat, aiTextureType type)->std::vector<std::string>
		{
			std::vector<std::string> textures;
			for (size_t i = 0; i < aiMat->GetTextureCount(type); i++)
			{
				aiString str;
				aiMat->GetTexture(type, i, &str);
				if (str.length != 0)
				{
					const aiTexture* texture = scene->GetEmbeddedTexture(str.C_Str());
					if (texture == nullptr)
					{
						textures.push_back(str.C_Str());
					}
					else
					{
						std::string texturePath = directory + '/' + fileName + '/';
						unsigned int width, height;
						width = texture->mWidth;
						std::vector<unsigned char> buffer;
						buffer.resize(texture->mWidth);
						memcpy(buffer.data(), texture->pcData, texture->mWidth);
						std::string ext = texture->achFormatHint;
						if (type == aiTextureType_DIFFUSE || type == aiTextureType_BASE_COLOR)
						{
							std::string texFileName = "diffuse." + ext;
							std::string outPath = texturePath + texFileName;
							VulkanUtil::saveFile(outPath, buffer);
							textures.push_back(fileName + '/' + texFileName);
						}
						if (type == aiTextureType_NORMALS)
						{
							std::string texFileName = "normal." + ext;
							std::string outPath = texturePath + texFileName;
							VulkanUtil::saveFile(outPath, buffer);
							textures.push_back(fileName + '/' + texFileName);
						}
						if (type == aiTextureType_DIFFUSE_ROUGHNESS)
						{
							std::string texFileName = "mr." + ext;
							std::string outPath = texturePath + texFileName;
							VulkanUtil::saveFile(outPath, buffer);
							textures.push_back(fileName + '/' + texFileName);
						}
					}
				}
			}
			return textures;
		};

		std::vector<std::string> diffuse = getTexture(aiMat, aiTextureType_DIFFUSE);
		std::vector<std::string> glossiness = getTexture(aiMat, aiTextureType_SPECULAR);
		std::vector<std::string> emissive = getTexture(aiMat, aiTextureType_EMISSIVE);
		std::vector<std::string> normal = getTexture(aiMat, aiTextureType_NORMALS);
		std::vector<std::string> shininess = getTexture(aiMat, aiTextureType_SHININESS);
		std::vector<std::string> opacity = getTexture(aiMat, aiTextureType_OPACITY);
		std::vector<std::string> baseColor = getTexture(aiMat, aiTextureType_BASE_COLOR);
		std::vector<std::string> emissiveColor = getTexture(aiMat, aiTextureType_EMISSION_COLOR);
		std::vector<std::string> roughness = getTexture(aiMat, aiTextureType_DIFFUSE_ROUGHNESS);
		std::vector<std::string> ao = getTexture(aiMat, aiTextureType_AMBIENT_OCCLUSION);

		if (!diffuse.empty() || !normal.empty() || !baseColor.empty() || !roughness.empty())
		{
			PBRMaterial* material = createOrGetMaterial(aiMat);

			meshes[aiMesh]->material = material;

			if (!diffuse.empty())
			{
				Texture* tex = createOrGetTexture(diffuse[0]);
				material->baseColor = tex;
			}
			if (!normal.empty())
			{
				Texture* tex = createOrGetTexture(normal[0]);
				material->normal = tex;
			}
			else
			{
				material->normal = sceneData->textures[1];
			}
			if (!baseColor.empty() && material->baseColor == nullptr)
			{
				Texture* tex = createOrGetTexture(baseColor[0]);
				material->baseColor = tex;
			}
			if (!roughness.empty())
			{
				Texture* tex = createOrGetTexture(roughness[0]);
				material->metallicRoughness = tex;
			}
			else
			{
				material->metallicRoughness = sceneData->textures[2];
			}
			if (!ao.empty())
			{
				Texture* tex = createOrGetTexture(ao[0]);
				material->occlusion = tex;
			}

			if (material->baseColor == nullptr)
			{
				material->baseColor = sceneData->textures[0];
			}
		}
	}

}