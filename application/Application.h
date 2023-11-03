#pragma once

#include "Barrier.h"
#include "Buffer.h"
#include "CommandBuffer.h"
#include "DescriptorSetManager.h"
#include "Device.h"
#include "GraphicsPipeline.h"
#include "Image.h"
#include "SwapChain.h"
#include "SyncObjects.h"
#include "VkConfig.h"
#include "VkWindow.h"

#include "Camera.h"

#include <iostream>
#include <string>
#include <vector>

namespace Vk {
struct RenderConfig {
    int32_t width;
    int32_t height;
    bool isWireFrame;
    bool useInstance;
};

struct UniformBuffers {
    UniformBuffers()
        : mvp(glm::mat4(1.f))
        , viewMode(0)
        , level(0)
        , displayExtEdge(0)
    {
    }
    glm::mat4 mvp;
    uint32_t viewMode;
    uint32_t level;
    uint32_t displayExtEdge;
};

class Application {
public:
    Application() {};
    Application(const RenderConfig& config);
    ~Application();

    void InitWindow(uint32_t width, uint32_t height);
    void CreateDevice(bool enableValidationLayers);
    void CreateSwapChain();
    void CreateDepthBuffer(uint32_t width, uint32_t height);
    void CreatePipeline(uint32_t pushConstantSize, bool isWireFrame);
    void CreateDescriptorSetManager();

    void CreateCamera();
    void CreateInstanceBuffers(const std::vector<uint32_t>& packedData);
    void CreateFrameContextBuffers();
    void CreateCommandBuffer();
    void RecordCommand();
    void CreateSyncObjects();
    void Run(const std::vector<uint32_t>& packedData);
    void BeginRender(VkCommandBuffer cmd, uint32_t imageId);
    void EndRender(VkCommandBuffer cmd);
    void PushConstant(VkCommandBuffer cmd, uint32_t size, void* p);
    void BindGraphicsPipeline(VkCommandBuffer cmd);
    void BindVertexAndIndicesBuffer(VkCommandBuffer cmd);
    void BindDescriptorSets(VkCommandBuffer cmd);
    void SetViewportAndScissor(VkCommandBuffer cmd);
    void Draw(VkCommandBuffer cmd);

    void UpdateUniformBuffers(uint32_t imageId);
    void AcquireNextImage(uint32_t frameId, uint32_t& imageId);
    void WaitForFence(uint32_t frameId);
    void ResetFence(uint32_t frameId);
    void QueueSubmit(uint32_t currentFrame, uint32_t imageId);

    void CreateBuffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void CreateIndexBuffer(VkDevice device, const std::vector<uint32_t>& indices);
    void CreateVertexBuffer(VkDevice device, const std::vector<glm::vec3>& vertices);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void OnKey(int key, int scancode, int action, int mods);
    void OnScroll(double xoffset, double yoffset);
    void OnCursorPosition(double xpos, double ypos);
    void OnMouseButton(int button, int action, int mods);

    const bool& GetMouseLeftDown() const { return _mouseStatus.lDown; }
    const bool& GetMouseRightDown() const { return _mouseStatus.rDown; }
    const int32_t& GetMouseHorizontalMove() const { return _mouseStatus.horizontalMove; }
    const int32_t& GetMouseVerticalMove() const { return _mouseStatus.verticalMove; }
    void CleanUpMouseStatus()
    {
        _mouseStatus.verticalMove = 0;
        _mouseStatus.horizontalMove = 0;
    }

    void SetEvents();
    void DrawFrame();
    void CleanBuffer(VkDevice device);
    void CleanUp();
    template <class T>
    void CleanUp(T* p)
    {
        if (p != nullptr) {
            delete p;
        }
        p = nullptr;
    }

    void ShowFps();

private:
    Window* _window;
    Device* _device;
    SwapChain* _swapchain;
    GraphicsPipeline* _graphicsPipeline;
    Image* _depthBuffer;
    DescriptorSetManager* _descriptorSetManager;
    SyncObjects* _syncObjects;

    CommandPool* _commandPool;
    CommandBuffers* _commandBuffers;

    VkBuffer _vertexBuffer;
    VkDeviceMemory _vertexBufferMemory;
    VkBuffer _indexBuffer;
    VkDeviceMemory _indexBufferMemory;

    std::vector<Buffer*> _uniformBuffers;
    std::vector<Buffer*> _packedClusters;

    uint32_t _indicesSize;

    Core::Camera* _camera;
    UniformBuffers _ubo;

    struct {
        bool lDown = false, rDown = false;
        int32_t horizontalMove = 0, verticalMove = 0;
        int32_t lastX, lastY;
    } _mouseStatus;

    bool _useInstance;

#ifdef _DEBUG
    bool enableValidationLayers = true;
#else
    bool enableValidationLayers = false;
#endif
};
}