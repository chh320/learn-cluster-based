#include "Application.h"
#include <bit>
#include <sstream>
#include <vector>

namespace Vk {
Application::Application(const RenderConfig& config)
    : _useInstance(config.useInstance)
{
    InitWindow(config.width, config.height);
    CreateDevice(enableValidationLayers);
    CreateSwapChain();
    CreateDepthBuffer(config.width, config.height);
    CreateDescriptorSetManager();
    CreateGraphicsPipeline(4, config.isWireFrame);
    CreateComputePipeline(4);
    CreateFrameContextBuffers();
}

Application::~Application()
{
    CleanUp();
}

// void Application::Run(const Core::Mesh& mesh, const Core::VirtualMesh& vmesh)
void Application::Run(std::vector<uint32_t>& packedData)
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

    CreateSyncObjects();

    RecordCommand();
    SetEvents();
    DrawFrame();

    vkDeviceWaitIdle(_device->GetDevice());
}

void Application::DrawFrame()
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

void Application::SetEvents()
{
    _window->OnKey = [this](const int key, const int scancode, const int action, const int mods) { OnKey(key, scancode, action, mods); };
    _window->OnCursorPosition = [this](const double xpos, const double ypos) { OnCursorPosition(xpos, ypos); };
    _window->OnMouseButton = [this](const int button, const int action, const int mods) { OnMouseButton(button, action, mods); };
    _window->OnScroll = [this](const double xoffset, const double yoffset) { OnScroll(xoffset, yoffset); };
}

void Application::CreateCamera()
{
    glm::vec3 pos = glm::vec3(0.f, 0.f, 5.f);
    glm::vec3 target = glm::vec3(0.f);
    glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
    float fov = 45.f;
    float aspect = (float)_swapchain->GetExtent().width / _swapchain->GetExtent().height;
    float zNear = 0.1f;
    float zFar = 100.f;
    _camera = new Core::Camera(pos, target, worldUp, fov, aspect, zNear, zFar);
}

void Application::CreateInstanceBuffers(std::vector<uint32_t>& packedData)
{
    //for (auto i = 0; i < _clustersNum; i++) {
    //    std::vector<uint32_t> bufferData;
    //    auto offset = 4 + i * 16;

    //    auto vertNum = packedData[offset + 0];
    //    auto vertOffset = packedData[offset + 1];
    //    auto triangleNum = packedData[offset + 2];
    //    auto idOffset = packedData[offset + 3];

    //    bufferData.push_back(triangleNum); // triangle num;
    //    bufferData.push_back(vertNum); // vertex num;
    //    bufferData.push_back(packedData[offset + 14]); // groupId;
    //    bufferData.push_back(packedData[offset + 15]); // mipLevel;

    //    bufferData.push_back(packedData[offset + 4]); // lodBounds.center.x
    //    bufferData.push_back(packedData[offset + 5]); // lodBounds.center.y
    //    bufferData.push_back(packedData[offset + 6]); // lodBounds.center.z
    //    bufferData.push_back(packedData[offset + 7]); // lodBounds.radius

    //    for (size_t j = 0; j < triangleNum; j++) {
    //        bufferData.push_back(packedData[idOffset + j]); // triangle corners id
    //    }

    //    for (size_t j = 0; j < vertNum; j++) {
    //        bufferData.push_back(packedData[vertOffset + j * 3 + 0]); // vert.x
    //        bufferData.push_back(packedData[vertOffset + j * 3 + 1]); // vert.y
    //        bufferData.push_back(packedData[vertOffset + j * 3 + 2]); // vert.z
    //    }

    //    for (auto j = 0; j < triangleNum * 3; j++)
    //        bufferData.push_back(0);

    //    _packedClusters.push_back(new Buffer(_device->GetAllocator(), bufferData.size() * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU));
    //    _packedClusters[_packedClusters.size() - 1]->Update(bufferData.data(), bufferData.size() * sizeof(uint32_t));
    //}

    _clustersNum = packedData[0];
    int imageCnt = _swapchain->GetImageCount();

    // buffer array [0] : const context
    _constContextBuffer = new Buffer(_device->GetAllocator(), sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    Buffer::UpdateDescriptorSets(std::vector<Buffer*>{_constContextBuffer}, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 0);
    _constContextBuffer->Update(&imageCnt, sizeof(uint32_t));

    // buffer array [1, 1 + swapchain image num) : indirect buffer for indirect draw
    _indirectBuffers.resize(imageCnt);
    for (auto& buffer : _indirectBuffers)
        buffer = new Buffer(_device->GetAllocator(), 4 * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    Buffer::UpdateDescriptorSets(_indirectBuffers, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 1);

    // buffer array [1 + swapchain image num, 1 + 2 * swapchain image num) : visibility cluster buffer
    _visibilityClusterBuffers.resize(imageCnt);
    for (auto& buffer : _visibilityClusterBuffers)
        buffer = new Buffer(_device->GetAllocator(), (1 << 20), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
    Buffer::UpdateDescriptorSets(_visibilityClusterBuffers, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 1 + imageCnt);

    // buffer array [1 + 3 * swapchain image num, ...) : packed cluster-based mesh data buffer
    _packedBuffer = new Buffer(_device->GetAllocator(), packedData.size() * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    Buffer::UpdateDescriptorSets(std::vector<Buffer*>{_packedBuffer}, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 1 + 3 * imageCnt);
    _packedBuffer->Update(packedData.data(), packedData.size() * sizeof(uint32_t));
}

void Application::CreateFrameContextBuffers()
{
    _uniformBuffers.resize(_swapchain->GetImageCount());
    for (auto& ubo : _uniformBuffers)
        ubo = new Buffer(_device->GetAllocator(), sizeof(UniformBuffers), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // buffer array [1 + 2 * swapchain image num, 1 + 3 * swapchain image num) : uniform buffer for mvp
    Buffer::UpdateDescriptorSets(_uniformBuffers, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 1 + 2 * _swapchain->GetImageCount());
}

void Application::CreateCommandBuffer()
{
    _commandPool = new CommandPool(*_device);
    _commandBuffers = new CommandBuffers(*_device, *_commandPool, _swapchain->GetImageCount());
}

void Application::RecordCommand()
{
    std::vector<std::pair<uint32_t, uint32_t>> id(_swapchain->GetImageCount());
    for (uint32_t i = 0; i < _swapchain->GetImageCount(); i++) {
        id[i] = { i, _swapchain->GetImageCount() };
    }
    for (auto i = 0; i < _commandBuffers->GetSize(); i++) {
        const auto cmd = _commandBuffers->Begin(i);
        BindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline->GetPipelineLayout());
        PushConstant(cmd, _computePipeline->GetPipelineLayout(), 4, &id[i]);
        {
            BufferBarrier bufferBarrier(_indirectBuffers[i]->GetBuffer(), _indirectBuffers[i]->GetSize(), 0, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier>(), std::vector<BufferBarrier>{ bufferBarrier });
        }
        BindComputePipeline(cmd);
        Dispatch(cmd, _clustersNum / 32 + 1, 1, 1);
        {
            ImageBarrier imageBarrier(_swapchain->GetImage(i),
                0, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT);
            BufferBarrier bufferBarrier(_indirectBuffers[i]->GetBuffer(), _indirectBuffers[i]->GetSize(), 0, 0, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier }, std::vector<BufferBarrier>{ bufferBarrier });
        }

        BeginRender(cmd, i);

        BindGraphicsPipeline(cmd);
        //if (!_useInstance)
        //    BindVertexAndIndicesBuffer(cmd);
        SetViewportAndScissor(cmd);
        BindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline->GetPipelineLayout());
        //Draw(cmd);
        DrawIndirect(cmd, i);
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

void Application::InitWindow(uint32_t width, uint32_t height)
{
    _window = new Window(width, height);
}

void Application::CreateDevice(bool enableValidationLayers)
{
    _device = new Device(enableValidationLayers, *_window);
}

void Application::CreateSwapChain()
{
    while (_window->IsMinimized()) {
        _window->WaitForEvents();
    }
    _swapchain = new SwapChain(*_device, *_window);
}

void Application::CreateDepthBuffer(uint32_t width, uint32_t height)
{
    _depthBuffer = new Image(*_device, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Application::CreateDescriptorSetManager()
{
    _descriptorSetManager = new DescriptorSetManager(_device->GetDevice());
}

void Application::CreateGraphicsPipeline(uint32_t pushConstantSize, bool isWireFrame)
{
    _graphicsPipeline = new GraphicsPipeline(*_device, *_swapchain, *_descriptorSetManager, pushConstantSize, _useInstance, isWireFrame);
}

void Application::CreateComputePipeline(uint32_t pushConstantSize) {
    _computePipeline = new ComputePipeline(*_device, *_descriptorSetManager, pushConstantSize);
}

void Application::BeginRender(VkCommandBuffer cmd, uint32_t imageId)
{
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    std::vector<VkRenderingAttachmentInfo> depthAttachments;

    VkRenderingAttachmentInfo info {};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    info.imageView = _swapchain->GetImageView(imageId);
    info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    info.clearValue.color = { 0.1, 0.1, 0.1, 1.0 };
    colorAttachments.push_back(info);

    if (_depthBuffer != nullptr) {
        VkRenderingAttachmentInfo info {};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        info.imageView = _depthBuffer->GetImageView();
        info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        info.clearValue.depthStencil.depth = 1.0;
        depthAttachments.push_back(info);
    }

    VkRenderingInfo renderingInfo {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = { _swapchain->GetExtent().width, _swapchain->GetExtent().height };

    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = depthAttachments.size() ? depthAttachments.data() : nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderingInfo);
}

void Application::EndRender(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
}

void Application::PushConstant(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, uint32_t size, void* p)
{
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0, size, p);
}

void Application::BindGraphicsPipeline(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline->GetPipeline());
}

void Application::BindComputePipeline(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline->GetPipeline());
}

void Application::BindVertexAndIndicesBuffer(VkCommandBuffer cmd)
{
    VkBuffer vertexBuffers[] = { _vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(cmd, _indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Application::BindDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint usage, VkPipelineLayout pipelineLayout)
{
    auto& bufferSet = _descriptorSetManager->GetBindlessBufferSet();
    vkCmdBindDescriptorSets(cmd, usage, pipelineLayout, 0, 1, &bufferSet, 0, nullptr);
}

void Application::SetViewportAndScissor(VkCommandBuffer cmd)
{
    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<uint32_t>(_swapchain->GetExtent().width);
    viewport.height = static_cast<uint32_t>(_swapchain->GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset = { 0, 0 };
    scissor.extent = _swapchain->GetExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Application::Dispatch(VkCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z) {
    vkCmdDispatch(cmd, x, y, z);
}

void Application::Draw(VkCommandBuffer cmd)
{
    if (_useInstance) {
        vkCmdDraw(cmd, 3 * 128, _packedClusters.size(), 0, 0);
    } else {
        vkCmdDrawIndexed(cmd, _indicesSize, 1, 0, 0, 0);
    }
}

void Application::DrawIndirect(VkCommandBuffer cmd, uint32_t id) {
    vkCmdDrawIndirect(cmd, _indirectBuffers[id]->GetBuffer(), 0, 1, 4 * sizeof(uint32_t));
}

void Application::CreateSyncObjects()
{
    _syncObjects = new SyncObjects(_device->GetDevice(), _swapchain->GetImageCount());
}

void Application::UpdateUniformBuffers(uint32_t imageId)
{
    glm::mat4 model = glm::mat4(1.f);
    glm::mat4 view = _camera->getViewMatrix();
    glm::mat4 proj = _camera->getProjMatrix();

    _ubo.mvp = proj * view * model;
    _ubo.view = view * model;

    if (GetMouseLeftDown()) {
        _camera->rotateByScreenX(_camera->getTarget(), GetMouseHorizontalMove() * 0.015);
        _camera->rotateByScreenY(_camera->getTarget(), GetMouseVerticalMove() * 0.01);
    } else if (GetMouseRightDown()) {
        _camera->moveCamera(GetMouseHorizontalMove(), GetMouseVerticalMove());
    }

    _uniformBuffers[imageId]->Update(&_ubo, sizeof(UniformBuffers));
}

void Application::AcquireNextImage(uint32_t frameId, uint32_t& imageId)
{
    vkAcquireNextImageKHR(_device->GetDevice(), _swapchain->GetSwapChain(), UINT64_MAX, _syncObjects->GetImageAvailableSemaphore(frameId), VK_NULL_HANDLE, &imageId);
}

void Application::WaitForFence(uint32_t frameId)
{
    auto fence = _syncObjects->GetFence(frameId);
    vkWaitForFences(_device->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
}

void Application::ResetFence(uint32_t frameId)
{
    auto fence = _syncObjects->GetFence(frameId);
    vkResetFences(_device->GetDevice(), 1, &fence);
}

void Application::QueueSubmit(uint32_t currentFrame, uint32_t imageId)
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

void Application::CreateVertexBuffer(VkDevice device, const std::vector<glm::vec3>& vertices)
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

void Application::CreateIndexBuffer(VkDevice device, const std::vector<uint32_t>& indices)
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

void Application::CreateBuffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
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

void Application::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    SingleTimeCommands::Submit(*_device, *_commandPool, [&](VkCommandBuffer cmd) {
        VkBufferCopy copyRegion {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion); // copy memory
    });
}

uint32_t Application::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
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

void Application::CleanUp()
{
    CleanUp(_swapchain);
    CleanUp(_depthBuffer);
    for (auto& ubo : _uniformBuffers)
        CleanUp(ubo);
    for (auto& buffer : _indirectBuffers)
        CleanUp(buffer);
    for (auto& buffer : _visibilityClusterBuffers)
        CleanUp(buffer);
    CleanUp(_packedBuffer);
    CleanUp(_constContextBuffer);
   /* for (auto& buffer : _packedClusters)
        CleanUp(buffer);*/
    CleanUp(_descriptorSetManager);
    if (!_useInstance)
        CleanBuffer(_device->GetDevice());
    CleanUp(_graphicsPipeline);
    CleanUp(_computePipeline);
    CleanUp(_syncObjects);
    CleanUp(_commandPool);
    CleanUp(_commandBuffers);
    CleanUp(_window);
    CleanUp(_device);
    CleanUp(_camera);
}

void Application::CleanBuffer(VkDevice device)
{
    vkDestroyBuffer(device, _indexBuffer, nullptr);
    vkFreeMemory(device, _indexBufferMemory, nullptr);

    vkDestroyBuffer(device, _vertexBuffer, nullptr);
    vkFreeMemory(device, _vertexBufferMemory, nullptr);
}

void Application::ShowFps()
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

void Application::OnKey(int key, int scancode, int action, int mods)
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
        default:
            break;
        }
    }
}
void Application::OnScroll(double xoffset, double yoffset)
{
    _camera->zoom(yoffset > 0.0 ? 1.0 : -1.0);
}

void Application::OnCursorPosition(double xpos, double ypos)
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

void Application::OnMouseButton(int button, int action, int mods)
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