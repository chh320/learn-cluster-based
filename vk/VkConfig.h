#pragma once

#define NOMINMAX
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vma/vk_mem_alloc.h>


namespace Vk {
void Check(VkResult result, const char* operation);
const char* ToString(const VkResult result);
}