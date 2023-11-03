#pragma once
#include "VkConfig.h"
#include <glm/glm.hpp>
#include <functional>

namespace Vk {
	class Window {
	public:
		Window() {};
		Window(uint32_t width, uint32_t height);
		~Window();

		VkExtent2D FramebufferSize() const;
		bool IsMinimized() const;
		void WaitForEvents() const;

		bool ShouleClose();
		void PollEvents();
		glm::dvec2 GetCursorPos();
		
		void Close();

		GLFWwindow* GetWindow() { return _window; }
		const GLFWwindow* GetWindow() const { return _window; }
		const uint32_t GetWidth() const { return _width; }
		const uint32_t GetHeight() const { return _height; }

		std::function<void(int key, int scancode, int action, int mods)> OnKey;
		std::function<void(double xpos, double ypos)> OnCursorPosition;
		std::function<void(int button, int action, int mods)> OnMouseButton;
		std::function<void(double xoffset, double yoffset)> OnScroll;
	private:
		GLFWwindow* _window;
		uint32_t _width;
		uint32_t _height;
	};
}