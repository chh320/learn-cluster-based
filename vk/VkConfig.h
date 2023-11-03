#pragma once

#define NOMINMAX
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vma/vk_mem_alloc.h>
#include <stdexcept>

namespace Vk {
    void Check(VkResult result, const char* operation);
    const char* ToString(const VkResult result);
}