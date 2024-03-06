#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE		// 默认是OPENGL的 -1到1
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL2/SDL_events.h>

namespace VulkanEngine
{
	// 右手坐标系，Y上，Z前，X左
	class Camera
	{
	public:
		Camera(glm::vec3 position, float pitch, float yaw, glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f));	// 不允许绕前方向旋转，也就是roll，翻滚角

		glm::mat4 getViewMatrix();
		glm::mat4 getProjectMatrix(float aspect, bool reverse = false);

		void updateCameraTransform();

		glm::vec3 position;
		float pitch = 0.0f;			// 俯仰角
		float yaw = 180.0f;			// 偏航角

		float zoom = 70.0f;			// 视角

		glm::vec3 forward;
		glm::vec3 right;
		glm::vec3 up;
		glm::vec3 worldUp;
	};

	class CameraController
	{
	public:
		CameraController();

		void processInputEvent(SDL_Event* event, float speed); 
		
		Camera camera = { glm::vec3(0.0f, 2.0f, 2.0f), -45, 180 };
		 
	private:

		void cameraLookAtCenter();
		float radius;
		float moveSpeed = 0.1f;
		bool sprint = false;	// 加速
	};
}