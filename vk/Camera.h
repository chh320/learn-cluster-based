#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

namespace Core {
	class Camera final {
	public:
		Camera(glm::vec3 pos, glm::vec3 target, glm::vec3 worldup, float fov, float aspect, float near, float far)
			: pos_(pos)
			, target_(target)
			, worldup_(worldup)
			, fov_(fov)
			, front_(glm::normalize(target_ - pos_))
			, aspect_(aspect)
			, zNear_(near)
			, zFar_(far)
			, right_(glm::cross(front_, worldup_))
		{
			_updateCameraVectors();
			_updateViewMatrix();
			_updateProjMatrix();
		}

		glm::mat4 getViewMatrix() const { return view_; }
		glm::mat4 getProjMatrix() const { return proj_; }
		glm::vec3 getViewPos() const { return pos_; }
		glm::vec3 getViewDir() const { return front_; }
		glm::vec3 getTarget() const { return target_; }
		float getNear() const { return zNear_; }
		float getFar() const { return zFar_; }
		float getAspect() const { return aspect_; }
		float getFov() const { return fov_; }

		void setViewPos(const glm::vec3& pos) {
			pos_ = pos;
			_updateViewMatrix();
		}

		void setAspect(float aspect) {
			aspect_ = aspect;
			_updateProjMatrix();
		}

		void zoom(float dir) {
			dir *= 0.3;		// scale value
			pos_ += dir * front_;
			_updateViewMatrix();
		}

		void rotateByScreenX(const glm::vec3& center, float angleX) {
			glm::mat4 translate = glm::translate(glm::mat4(1.0f), -center);
			glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), -angleX, glm::vec3(0, 1, 0));
			glm::mat4 insTranslate = glm::inverse(translate);
			pos_ = insTranslate * rotate * translate * glm::vec4(pos_, 1.0f);
			right_ = rotate * glm::vec4(right_, 1.f);
			front_ = rotate * glm::vec4(front_, 1.f);
			_updateCameraVectors();
		}

		void rotateByScreenY(const glm::vec3& center, float angleY) {
			glm::mat4 translate = glm::translate(glm::mat4(1.0f), -center);
			glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), -angleY, right_);
			glm::mat4 insTranslate = glm::inverse(translate);
			pos_ = insTranslate * rotate * translate * glm::vec4(pos_, 1.0f);
			front_ = rotate * glm::vec4(front_, 1.f);
			_updateCameraVectors();
		}

		void moveCamera(float xpos, float ypos) {
			xpos *= -0.01;
			ypos *= 0.01;
			pos_ += xpos * right_ + ypos * up_;
			target_ += xpos * right_ + ypos * up_;
			_updateCameraVectors();
		}

	private:
		glm::vec3 pos_;
		glm::vec3 target_;
		glm::vec3 front_;
		glm::vec3 worldup_;
		glm::vec3 right_;
		glm::vec3 up_;


		float zNear_;
		float zFar_;
		float fov_;
		float aspect_;

		glm::mat4 view_;
		glm::mat4 proj_;

		void  _updateViewMatrix() {
			view_ = glm::lookAt(pos_, target_, up_);
		}

		void _updateProjMatrix() {
			proj_ = glm::perspectiveRH_ZO(glm::radians(fov_), aspect_, zFar_, zNear_ );	// reverse z
			//proj_ = glm::perspective(glm::radians(fov_), aspect_, zNear_, zFar_);
			proj_[1][1] *= -1;
		}

		void _updateCameraVectors() {
			up_ = glm::normalize(glm::cross(right_, front_));
			_updateViewMatrix();
		}
	};
}