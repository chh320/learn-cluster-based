#include "DebugLodApplication.h"
#include <bit>
#include <sstream>
#include <vector>

namespace Vk {
DebugLodApplication::DebugLodApplication(const RenderConfig& config)
    : _useInstance(config.useInstance)
    , _modelScale(1.0)
{
    InitWindow(config.width, config.height);
    CreateDevice(enableValidationLayers);
    CreateSwapChain();
    CreateDescriptorSetManager();
    CreateGraphicsPipeline(8, config.isWireFrame);
    CreateFrameContextBuffers();
}

DebugLodApplication::~DebugLodApplication()
{
    CleanUp();
}

void DebugLodApplication::Run(std::vector<uint32_t>& packedData)
{
    CreateCamera();
    CreateCommandBuffer();

    if (_useInstance) {
        CreateInstanceBuffers(packedData);
    }
    // else {
    //     CreateVertexBuffer(_device->GetDevice(), mesh.vertices);
    //     CreateIndexBuffer(_device->GetDevice(), mesh.indices);
    // }
    CreateDepthBuffer(_window->GetWidth(), _window->GetHeight());

    CreateSyncObjects();

    RecordCommand();
    SetEvents();
    DrawFrame();

    vkDeviceWaitIdle(_device->GetDevice());
}

void DebugLodApplication::DrawFrame()
{
    uint32_t frameId = 0;
    while (!_window->ShouleClose()) {
        _window->PollEvents();

        uint32_t imageId;
        WaitForFence(frameId);
        AcquireNextImage(frameId, imageId);
        ResetFence(frameId);
        UpdateUniformBuffers(imageId);
        QueueSubmit(frameId, imageId);

        frameId = (frameId + 1) % 3;
        ShowFps();
        CleanUpMouseStatus();
    }
}

void DebugLodApplication::SetEvents()
{
    _window->OnKey = [this](const int key, const int scancode, const int action, const int mods) { OnKey(key, scancode, action, mods); };
    _window->OnCursorPosition = [this](const double xpos, const double ypos) { OnCursorPosition(xpos, ypos); };
    _window->OnMouseButton = [this](const int button, const int action, const int mods) { OnMouseButton(button, action, mods); };
    _window->OnScroll = [this](const double xoffset, const double yoffset) { OnScroll(xoffset, yoffset); };
}

void DebugLodApplication::CreateCamera()
{
    glm::vec3 pos = glm::vec3(0.f, 0.f, 3.f);
    glm::vec3 target = glm::vec3(0.f);
    glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
    float fov = 60.f;
    float aspect = (float)_swapchain->GetExtent().width / _swapchain->GetExtent().height;
    float zNear = 0.1f;
    float zFar = FLT_MAX;
    _camera = new Core::Camera(pos, target, worldUp, fov, aspect, zNear, zFar);
}

void DebugLodApplication::CreateInstanceBuffers(std::vector<uint32_t>& packedData)
{

    _clustersNum = packedData[0];
    _groupsNum = packedData[1];
    _MipLevelNum = packedData[4 + 20 * _clustersNum - 1] + 1;
    float radius = std::abs(Util::Uint2Float(packedData[packedData[2] + 8 * (_groupsNum - 1) + 7]));
    _modelScale = pow(10, -std::floor(std::log10(radius)));
    // std::cout << radius << " " << _modelScale << "\n";

    int imageCnt = _swapchain->GetImageCount();

    // buffer array [1 + 3 * swapchain image num] : packed cluster-based mesh data buffer
    _packedBuffer = new Buffer(_device->GetAllocator(), packedData.size() * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    Buffer::UpdateDescriptorSets(std::vector<Buffer*> { _packedBuffer }, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 1 + 3 * imageCnt);
    _packedBuffer->Update(packedData.data(), packedData.size() * sizeof(uint32_t));
}

void DebugLodApplication::CreateFrameContextBuffers()
{
    _uniformBuffers.resize(_swapchain->GetImageCount());
    for (auto& ubo : _uniformBuffers)
        ubo = new Buffer(_device->GetAllocator(), sizeof(UniformBuffers), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // buffer array [1 + 2 * swapchain image num, 1 + 3 * swapchain image num) : uniform buffer for mvp
    Buffer::UpdateDescriptorSets(_uniformBuffers, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 1 + 2 * _swapchain->GetImageCount());
}

void DebugLodApplication::CreateCommandBuffer()
{
    _commandPool = new CommandPool(*_device);
    _commandBuffers = new CommandBuffers(*_device, *_commandPool, _swapchain->GetImageCount());
}

void DebugLodApplication::RecordCommand()
{
    std::vector<uint32_t> graphicsPushConstants(2);
    graphicsPushConstants[1] = _swapchain->GetImageCount();

    for (auto i = 0; i < _commandBuffers->GetSize(); i++) {
        const auto cmd = _commandBuffers->Begin(i);
        {
            ImageBarrier imageBarrier(_swapchain->GetImage(i),
                0, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT);

            ImageBarrier depthImageBarrier(_depthBuffer->GetImage(),
                0, 0,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_DEPTH_BIT);
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier, depthImageBarrier }, std::vector<BufferBarrier> {});
        }

        graphicsPushConstants[0] = i;
        BeginRender(cmd, { _swapchain->GetImageView(i), _depthBuffer->GetImageView(), { _swapchain->GetExtent().width, _swapchain->GetExtent().height } });
        BindGraphicsPipeline(cmd, _graphicsPipeline->GetPipeline());
        // if (!_useInstance)
        //     BindVertexAndIndicesBuffer(cmd);
        SetViewportAndScissor(cmd, _swapchain->GetExtent());
        PushConstant(cmd, _graphicsPipeline->GetPipelineLayout(), 8, graphicsPushConstants.data());
        BindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline->GetPipelineLayout(), 0, _descriptorSetManager->GetBindlessBufferSet());
        Draw(cmd);
        EndRender(cmd);

        {
            ImageBarrier imageBarrier(_swapchain->GetImage(i),
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                0, 0,
                VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_IMAGE_ASPECT_COLOR_BIT);
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier }, std::vector<BufferBarrier>());
        }

        _commandBuffers->End(i);
    }
}

void DebugLodApplication::InitWindow(uint32_t width, uint32_t height)
{
    _window = new Window(width, height);
}

void DebugLodApplication::CreateDevice(bool enableValidationLayers)
{
    _device = new Device(enableValidationLayers, *_window);
}

void DebugLodApplication::CreateSwapChain()
{
    while (_window->IsMinimized()) {
        _window->WaitForEvents();
    }
    _swapchain = new SwapChain(*_device, *_window);
}

void DebugLodApplication::CreateDepthBuffer(uint32_t width, uint32_t height)
{
    _depthBuffer = new Image(*_device, width, height, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void DebugLodApplication::CreateDescriptorSetManager()
{
    _descriptorSetManager = new DescriptorSetManager(_device->GetDevice());
}

void DebugLodApplication::CreateGraphicsPipeline(uint32_t pushConstantSize, bool isWireFrame)
{
    {
        RenderInfo info {};
        info.viewWidth = _swapchain->GetExtent().width;
        info.viewHeight = _swapchain->GetExtent().height;
        info.useInstance = true;
        info.shaderName = { "shaders/debugLod.vert", "shaders/debugLod.frag" };
        info.compareOp = VK_COMPARE_OP_GREATER;
        info.pushConstantSize = pushConstantSize;
        info.colorAttachmentFormats = std::vector<VkFormat> { _swapchain->GetImageFormat() };
        info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        _graphicsPipeline = new GraphicsPipeline(*_device, *_descriptorSetManager, info, isWireFrame);
    }
}

void DebugLodApplication::BeginRender(VkCommandBuffer cmd, const RenderPassInfo& renderPassInfo)
{
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    std::vector<VkRenderingAttachmentInfo> depthAttachments;

    VkRenderingAttachmentInfo info {};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    info.imageView = renderPassInfo.colorImageView;
    info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    info.clearValue.color = { 0.1, 0.1, 0.1, 1.0 };
    colorAttachments.push_back(info);

    VkRenderingAttachmentInfo depthInfo {};
    depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthInfo.imageView = renderPassInfo.depthImageView;
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    depthInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // depthInfo.clearValue.depthStencil.depth = 1.0;
    depthAttachments.push_back(depthInfo);

    VkRenderingInfo renderingInfo {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = renderPassInfo.extent2D;

    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = depthAttachments.size() ? depthAttachments.data() : nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderingInfo);
}

void DebugLodApplication::EndRender(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
}

void DebugLodApplication::PushConstant(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, uint32_t size, void* p)
{
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0, size, p);
}

void DebugLodApplication::BindGraphicsPipeline(VkCommandBuffer cmd, VkPipeline pipeline)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void DebugLodApplication::BindVertexAndIndicesBuffer(VkCommandBuffer cmd)
{
    VkBuffer vertexBuffers[] = { _vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(cmd, _indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void DebugLodApplication::BindDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint usage, VkPipelineLayout pipelineLayout, uint32_t id, VkDescriptorSet descriptorSet)
{
    vkCmdBindDescriptorSets(cmd, usage, pipelineLayout, id, 1, &descriptorSet, 0, nullptr);
}

void DebugLodApplication::SetViewportAndScissor(VkCommandBuffer cmd, VkExtent2D extent2D)
{
    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<uint32_t>(extent2D.width);
    viewport.height = static_cast<uint32_t>(extent2D.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset = { 0, 0 };
    scissor.extent = extent2D;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void DebugLodApplication::Draw(VkCommandBuffer cmd)
{
    if (_useInstance) {
        vkCmdDraw(cmd, 3 * 128, _clustersNum, 0, 0);
    } else {
        vkCmdDrawIndexed(cmd, _indicesSize, 1, 0, 0, 0);
    }
}

void DebugLodApplication::CreateSyncObjects()
{
    _syncObjects = new SyncObjects(_device->GetDevice(), _swapchain->GetImageCount());
}

void DebugLodApplication::UpdateUniformBuffers(uint32_t imageId)
{
    glm::mat4 model = glm::mat4(1.f);
    model = glm::scale(model, glm::vec3(_modelScale));
    glm::mat4 view = _camera->getViewMatrix();
    glm::mat4 proj = _camera->getProjMatrix();

    _ubo.mvp = proj * view * model;
    _ubo.viewDir = glm::vec4(_camera->getViewDir(), 1.0f);

    if (GetMouseLeftDown()) {
        _camera->rotateByScreenX(_camera->getTarget(), GetMouseHorizontalMove() * 0.015);
        _camera->rotateByScreenY(_camera->getTarget(), GetMouseVerticalMove() * 0.01);
    } else if (GetMouseRightDown()) {
        _camera->moveCamera(GetMouseHorizontalMove(), GetMouseVerticalMove());
    }

    _uniformBuffers[imageId]->Update(&_ubo, sizeof(UniformBuffers));
}

void DebugLodApplication::AcquireNextImage(uint32_t frameId, uint32_t& imageId)
{
    vkAcquireNextImageKHR(_device->GetDevice(), _swapchain->GetSwapChain(), UINT64_MAX, _syncObjects->GetImageAvailableSemaphore(frameId), VK_NULL_HANDLE, &imageId);
}

void DebugLodApplication::WaitForFence(uint32_t frameId)
{
    auto fence = _syncObjects->GetFence(frameId);
    vkWaitForFences(_device->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
}

void DebugLodApplication::ResetFence(uint32_t frameId)
{
    auto fence = _syncObjects->GetFence(frameId);
    vkResetFences(_device->GetDevice(), 1, &fence);
}

void DebugLodApplication::QueueSubmit(uint32_t currentFrame, uint32_t imageId)
{
    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { _syncObjects->GetImageAvailableSemaphore(currentFrame) };
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
    };

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &(_commandBuffers->GetCommandBuffers()[currentFrame]);

    VkSemaphore signalSemaphores[] = { _syncObjects->GetRenderFinishedSemaphore(currentFrame) };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    Check(vkQueueSubmit(_device->GetQueue(), 1, &submitInfo, _syncObjects->GetFence(currentFrame)), "queue submit.");

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { _swapchain->GetSwapChain() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageId;
    presentInfo.pResults = nullptr;

    vkQueuePresentKHR(_device->GetQueue(), &presentInfo);
}

void DebugLodApplication::CreateVertexBuffer(VkDevice device, const std::vector<glm::vec3>& vertices)
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    CreateBuffer(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);

    CopyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void DebugLodApplication::CreateIndexBuffer(VkDevice device, const std::vector<uint32_t>& indices)
{
    _indicesSize = indices.size();

    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    CreateBuffer(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    CreateBuffer(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);

    CopyBuffer(stagingBuffer, _indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void DebugLodApplication::CreateBuffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void DebugLodApplication::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    SingleTimeCommands::Submit(*_device, *_commandPool, [&](VkCommandBuffer cmd) {
        VkBufferCopy copyRegion {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion); // copy memory
    });
}

uint32_t DebugLodApplication::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(_device->GetPhysicalDevice(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

void DebugLodApplication::CleanUp()
{
    CleanUp(_swapchain);
    CleanUp(_depthBuffer);
    for (auto& ubo : _uniformBuffers)
        CleanUp(ubo);
    CleanUp(_packedBuffer);
    /* for (auto& buffer : _packedClusters)
         CleanUp(buffer);*/
    CleanUp(_descriptorSetManager);
    if (!_useInstance)
        CleanBuffer(_device->GetDevice());
    CleanUp(_graphicsPipeline);
    CleanUp(_syncObjects);
    CleanUp(_commandPool);
    CleanUp(_commandBuffers);
    CleanUp(_window);
    CleanUp(_device);
    CleanUp(_camera);
}

void DebugLodApplication::CleanBuffer(VkDevice device)
{
    vkDestroyBuffer(device, _indexBuffer, nullptr);
    vkFreeMemory(device, _indexBufferMemory, nullptr);

    vkDestroyBuffer(device, _vertexBuffer, nullptr);
    vkFreeMemory(device, _vertexBufferMemory, nullptr);
}

void DebugLodApplication::ShowFps()
{
    static int nbFrames = 0;
    static float lastTime = 0.0f;
    // Measure speed
    double currentTime = glfwGetTime();
    double delta = currentTime - lastTime;
    nbFrames++;
    if (delta >= 1.0) { // If last cout was more than 1 sec ago
        // cout << 1000.0 / double(nbFrames) << endl;

        double fps = double(nbFrames) / delta;

        std::stringstream ss;
        ss << "Vulkan - Cluster-Based DAG"
           << " [" << fps << " FPS]";

        glfwSetWindowTitle(_window->GetWindow(), ss.str().c_str());

        nbFrames = 0;
        lastTime = currentTime;
    }
}

void DebugLodApplication::OnKey(int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            _window->Close();
            break;
        case GLFW_KEY_J:
            _ubo.viewMode = (_ubo.viewMode + 5 - 1) % 5;
            break;
        case GLFW_KEY_K:
            _ubo.viewMode = (_ubo.viewMode + 1) % 5;
            break;
        case GLFW_KEY_U:
            _ubo.mipLevel = (_ubo.mipLevel + _MipLevelNum - 1) % _MipLevelNum;
            break;
        case GLFW_KEY_I:
            _ubo.mipLevel = (_ubo.mipLevel + 1) % _MipLevelNum;
            break;
        default:
            break;
        }
    }
}
void DebugLodApplication::OnScroll(double xoffset, double yoffset)
{
    _camera->zoom(yoffset > 0.0 ? 1.0 : -1.0);
}

void DebugLodApplication::OnCursorPosition(double xpos, double ypos)
{
    if (_mouseStatus.lDown || _mouseStatus.rDown) {
        if (_mouseStatus.lastX != xpos)
            _mouseStatus.horizontalMove = xpos - _mouseStatus.lastX;
        if (_mouseStatus.lastY != ypos)
            _mouseStatus.verticalMove = ypos - _mouseStatus.lastY;
    }
    _mouseStatus.lastX = xpos;
    _mouseStatus.lastY = ypos;
}

void DebugLodApplication::OnMouseButton(int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        switch (action) {
        case GLFW_PRESS:
            _mouseStatus.lDown = true;
            break;
        case GLFW_RELEASE:
            _mouseStatus.lDown = false;
            break;
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        switch (action) {
        case GLFW_PRESS:
            _mouseStatus.rDown = true;
            break;
        case GLFW_RELEASE:
            _mouseStatus.rDown = false;
            break;
        }
    }
}
}