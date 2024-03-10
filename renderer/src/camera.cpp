#include "camera.hpp"
#include <iostream>

namespace VulkanEngine
{
	Camera::Camera(glm::vec3 position, float pitch, float yaw, glm::vec3 worldUp) : position(position), worldUp(worldUp), pitch(pitch), yaw(yaw)
	{
		updateCameraTransform();
	}

	glm::mat4 Camera::getViewMatrix()
	{
		return glm::lookAtRH(position, position + forward, worldUp);
	}

	glm::mat4 Camera::getProjectMatrix(float aspect, bool reverse)
	{
		if (reverse)
		{
			glm::mat4 pro = glm::perspective(glm::radians(zoom), aspect, far, near);
			pro[1][1] *= -1;		// vulkan跟opengl y相反
			return pro;
		}
		else 
		{
			glm::mat4 pro = glm::perspective(glm::radians(zoom), aspect, near, far);
			pro[1][1] *= -1;
			return pro;
		}
	}

	void Camera::updateCameraTransform()
	{
		pitch = glm::clamp(pitch, -89.0f, 89.0f);
		while (yaw > 360.0f)
		{
			yaw -= 360.0f;
		}

		while (yaw < -360.0f)
		{
			yaw += 360.0f;
		}

		forward.x = glm::cos(glm::radians(pitch)) * glm::sin(glm::radians(yaw));
		forward.y = glm::sin(glm::radians(pitch));
		forward.z = glm::cos(glm::radians(pitch)) * glm::cos(glm::radians(yaw));
		forward = glm::normalize(forward);

		right = glm::normalize(glm::cross(forward, worldUp));
		up = glm::normalize(glm::cross(right, forward));
	}

	CameraController::CameraController()
	{
		radius = camera.position.length();
	}

	void CameraController::processInputEvent(SDL_Event* event, float frameTime)
	{
		float velocity = moveSpeed + sprint * 0.2f * frameTime;

		if (event->type == SDL_KEYDOWN)
		{
			switch (event->key.keysym.sym)
			{
			//case SDLK_w:
			//	camera.position += camera.forward * velocity;
			//	break;
			//case SDLK_s:
			//	camera.position -= camera.forward * velocity;
			//	break;
			//case SDLK_a:
			//	camera.position -= camera.right * velocity;
			//	break;
			//case SDLK_d:
			//	camera.position += camera.right * velocity;
			//	break;
			//case SDLK_q:
			//	camera.position -= camera.up * velocity;
			//	break;
			//case SDLK_e:
			//	camera.position += camera.up * velocity;
			//	break;
			//case SDLK_UP:
			//	camera.pitch += 5.0f * velocity;
			//	break;
			//case SDLK_DOWN:
			//	camera.pitch -= 5.0f * velocity;
			//	break;
			//case SDLK_LEFT:
			//	camera.yaw += 5.0f * velocity;
			//	break;
			//case SDLK_RIGHT:
			//	camera.yaw -= 5.0f * velocity;
			//	break;
			case SDLK_LCTRL:
				sprint = true;
				break;
			}
		}
		if (event->type == SDL_KEYUP)
		{
			switch (event->key.keysym.sym)
			{
			case SDLK_LCTRL:
				sprint = false;
				break;
			} 
		}
		else if (event->type == SDL_MOUSEMOTION) 
		{
			if (event->button.button == SDL_BUTTON_LEFT)
			{
				camera.pitch -= event->motion.yrel * velocity;
				camera.yaw -= event->motion.xrel * velocity;
			}
		}
		else if (event->type == SDL_MOUSEWHEEL)
		{
			if (event->wheel.y > 0) 
			{
				//camera.zoom -= 45.0f * velocity;
				//camera.zoom = glm::clamp(camera.zoom, 1.0f, 180.0f);
			}
			if (event->wheel.y < 0) 
			{
				//camera.zoom += 45.0f * velocity;
				//camera.zoom = glm::clamp(camera.zoom, 1.0f, 180.0f);
			}
			radius -= event->wheel.y * minRadius;
			radius = glm::max(radius, minRadius / 2.0f);
		}

		if (event->type == SDL_KEYDOWN || (event->type == SDL_MOUSEMOTION && event->button.button == SDL_BUTTON_LEFT) || event->type == SDL_MOUSEWHEEL)
		{
			camera.updateCameraTransform();
			cameraLookAtCenter();
		}
	}

	glm::vec3 CameraController::getCenter()
	{
		return center;
	}

	void CameraController::setCenterAndRadius(const glm::vec3& center, float radius)
	{
		camera.position += (center - this->center);
		this->center = center;
		this->radius = radius;
		minRadius = radius / 10.0f;
		camera.near = minRadius;
		if (minRadius * 15 > camera.far)
		{
			camera.far = minRadius * 15;
		}
		cameraLookAtCenter();
	}

	void CameraController::cameraLookAtCenter()
	{
		camera.position = -camera.forward * radius + center;
	}

}