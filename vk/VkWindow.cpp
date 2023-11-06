#include "VkWindow.h"
#include <iostream>

namespace Vk {
	namespace {
		void GlfwErrorCallback(const int error, const char* const description)
		{
			std::cerr << "ERROR: GLFW: " << description << " (code: " << error << ")" << std::endl;
		}

		void GlfwKeyCallback(GLFWwindow* window, const int key, const int scancode, const int action, const int mods)
		{
			auto* const this_ = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (this_->OnKey)
			{
				this_->OnKey(key, scancode, action, mods);
			}
		}

		void GlfwCursorPositionCallback(GLFWwindow* window, const double xpos, const double ypos)
		{
			auto* const this_ = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (this_->OnCursorPosition)
			{
				this_->OnCursorPosition(xpos, ypos);
			}
		}

		void GlfwMouseButtonCallback(GLFWwindow* window, const int button, const int action, const int mods)
		{
			auto* const this_ = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (this_->OnMouseButton)
			{
				this_->OnMouseButton(button, action, mods);
			}
		}

		void GlfwScrollCallback(GLFWwindow* window, const double xoffset, const double yoffset)
		{
			auto* const this_ = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (this_->OnScroll)
			{
				this_->OnScroll(xoffset, yoffset);
			}
		}
	}

	Window::Window(uint32_t width, uint32_t height) : _width(width), _height(height){
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		_window = glfwCreateWindow(width, height, "Lite Nanite", nullptr, nullptr);
		glfwSetWindowUserPointer(_window, this);
		glfwSetKeyCallback(_window, GlfwKeyCallback);
		glfwSetCursorPosCallback(_window, GlfwCursorPositionCallback);
		glfwSetMouseButtonCallback(_window, GlfwMouseButtonCallback);
		glfwSetScrollCallback(_window, GlfwScrollCallback);

	}

	Window::~Window() {
		glfwDestroyWindow(_window);
		glfwTerminate();
	}

	VkExtent2D Window::FramebufferSize() const {
		int width, height;
		glfwGetFramebufferSize(_window, &width, &height);
		return VkExtent2D{ static_cast<uint32_t>(width),static_cast<uint32_t>(height) };
	}

	bool Window::IsMinimized() const {
		const auto size = FramebufferSize();
		return size.height == 0 && size.width == 0;
	}

	void Window::WaitForEvents() const {
		glfwWaitEvents();
	}

	bool Window::ShouleClose() {
		return glfwWindowShouldClose(_window);
	}

	void Window::PollEvents() {
		glfwPollEvents();
	}

	glm::dvec2 Window::GetCursorPos() {
		glm::dvec2 pos;
		glfwGetCursorPos(_window, &pos.x, &pos.y);
		return pos;
	}

	void Window::Close()
	{
		glfwSetWindowShouldClose(_window, 1);
	}
}