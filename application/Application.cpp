#include "Application.h"
#include <bit>
#include <sstream>
#include <vector>

namespace Vk {
Application::Application(const RenderConfig& config)
    : _instanceXYZ(config.instanceXYZ)
    , _maxMipSize(config.maxMipSize)
    , _useInstance(config.useInstance)
{
    InitWindow(config.width, config.height);
    CreateDevice(enableValidationLayers);
    CreateSwapChain();
    CreateDescriptorSetManager();
    CreateGraphicsPipeline(8, config.isWireFrame);
    CreateComputePipeline(28);
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
    CreateDepthBuffer(_window->GetWidth(), _window->GetHeight());
    CreateHizDepthImage();
    CreateImageSampler();
    BindImageDescriptorSets();

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
        InitIndirectBuffer(imageId);
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
    glm::vec3 pos       = glm::vec3(0.f, 0.f, 3.f);
    glm::vec3 target    = glm::vec3(0.f);
    glm::vec3 worldUp   = glm::vec3(0.f, 1.f, 0.f);
    float fov           = 60.f;
    float aspect        = (float)_swapchain->GetExtent().width / _swapchain->GetExtent().height;
    float zNear         = 0.1f;
    float zFar          = 100.f;
    _camera = new Core::Camera(pos, target, worldUp, fov, aspect, zNear, zFar);

    pos     = glm::vec3(_instanceXYZ.x * 0.5 * 5, _instanceXYZ.z * 2 * 5, _instanceXYZ.y * 0.5 * 5);
    target  = glm::vec3(_instanceXYZ.x * 0.5 * 5,                    0.f, _instanceXYZ.y * 0.5 * 5);
    worldUp = glm::vec3(0.f, 0.f, -1.f);
    fov     = 90.f;
    _camera2 = new Core::Camera(pos, target, worldUp, fov, aspect, zNear, zFar);
}

void Application::CreateInstanceBuffers(std::vector<uint32_t>& packedData)
{
    _clustersNum = packedData[0];
    _groupsNum = packedData[1];
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
        buffer = new Buffer(_device->GetAllocator(), (1 << 22), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
    Buffer::UpdateDescriptorSets(_visibilityClusterBuffers, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 1 + imageCnt);

    // buffer array [1 + 3 * swapchain image num] : packed cluster-based mesh data buffer
    _packedBuffer = new Buffer(_device->GetAllocator(), packedData.size() * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    Buffer::UpdateDescriptorSets(std::vector<Buffer*>{_packedBuffer}, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 1 + 3 * imageCnt);
    _packedBuffer->Update(packedData.data(), packedData.size() * sizeof(uint32_t));

    // buffer array [2 + 3 * swapchain image num] : instance offset buffer
    _instanceNum = _instanceXYZ.x * _instanceXYZ.y * _instanceXYZ.z;
    std::vector<glm::vec3> instanceOffset;
    for (int i = 0; i < _instanceXYZ.x; i++)
        for (int j = 0; j < _instanceXYZ.y; j++) 
            for (int k = 0; k < _instanceXYZ.z; k++) 
                instanceOffset.emplace_back(i * 5.f, k * 5.f, j * 5.f);
    _instanceOffsetBuffer = new Buffer(_device->GetAllocator(), instanceOffset.size() * sizeof(glm::vec3), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    Buffer::UpdateDescriptorSets(std::vector<Buffer*>{_instanceOffsetBuffer}, _device->GetDevice(), _descriptorSetManager->GetBindlessBufferSet(), 2 + 3 * imageCnt);
    _instanceOffsetBuffer->Update(instanceOffset.data(), instanceOffset.size() * sizeof(glm::vec3));
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
    std::vector<uint32_t> pushConstants;
    pushConstants.push_back(0);                                         // swapchain image id
    pushConstants.push_back(Util::Float2Uint(_camera->getNear()));      // near plane
    pushConstants.push_back(Util::Float2Uint(_camera->getFar()));       // far plane
    pushConstants.push_back(_instanceNum);                              // instance num
    pushConstants.push_back(_window->GetWidth());                       // width
    pushConstants.push_back(_window->GetHeight());                      // height
    pushConstants.push_back(_maxMipSize);                               // max mip size

    std::vector<uint32_t> graphicsPushConstants(2);

    for (auto i = 0; i < _commandBuffers->GetSize(); i++) {
        const auto cmd = _commandBuffers->Begin(i);
        pushConstants[0] = i;
        BindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline->GetPipelineLayout(), 0, _descriptorSetManager->GetBindlessBufferSet());
        BindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline->GetPipelineLayout(), 1, _descriptorSetManager->GetBindlessImageSet());
        PushConstant(cmd, _computePipeline->GetPipelineLayout(), 28, pushConstants.data());
        {
            BufferBarrier bufferBarrier(_indirectBuffers[i]->GetBuffer(), _indirectBuffers[i]->GetSize(), 0, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier>(), std::vector<BufferBarrier>{ bufferBarrier });
        }
        BindComputePipeline(cmd);
        Dispatch(cmd, _groupsNum * _instanceNum / 32 + 1, 1, 1);
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
            BufferBarrier bufferBarrier(_indirectBuffers[i]->GetBuffer(), _indirectBuffers[i]->GetSize(), 0, 0, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier, depthImageBarrier }, std::vector<BufferBarrier>{ bufferBarrier });
        }

        graphicsPushConstants[0] = i;
        graphicsPushConstants[1] = 0;
        BeginRender(cmd, { _swapchain->GetImageView(i), _depthBuffer->GetImageView(), {_swapchain->GetExtent().width, _swapchain->GetExtent().height } });
        BindGraphicsPipeline(cmd, _graphicsPipeline->GetPipeline());
        //if (!_useInstance)
        //    BindVertexAndIndicesBuffer(cmd);
        SetViewportAndScissor(cmd, _swapchain->GetExtent());
        PushConstant(cmd, _graphicsPipeline->GetPipelineLayout(), 8, graphicsPushConstants.data());
        BindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline->GetPipelineLayout(), 0, _descriptorSetManager->GetBindlessBufferSet());
        //Draw(cmd);
        DrawIndirect(cmd, i);
        EndRender(cmd);

        {
            ImageBarrier imageBarrier(_swapchain->GetImage(i),
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT);
            ImageBarrier depthImageBarrier(_depthBuffer->GetImage(),
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_DEPTH_BIT);
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier, depthImageBarrier}, std::vector<BufferBarrier>());
        }


        // hiz part ---------------------------------------------
        std::vector<uint32_t> hizPushConstants(4);
        hizPushConstants[0] = _window->GetWidth();
        hizPushConstants[1] = _window->GetHeight();
        hizPushConstants[2] = _maxMipSize;

        BindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _hizGraphicsPipeline->GetPipelineLayout(), 1, _descriptorSetManager->GetBindlessImageSet());
        for (uint32_t level = 0, mipSize = _maxMipSize; level < _hizMipLevels; level++, mipSize >>= 1) {
            {
                ImageBarrier imageBarrier(_hizImage->GetImage(),
                    0, 0,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_ASPECT_DEPTH_BIT,
                    level);
                Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier }, std::vector<BufferBarrier>());
            }

            hizPushConstants[3] = level;
            PushConstant(cmd, _computePipeline->GetPipelineLayout(), hizPushConstants.size() * sizeof(uint32_t), hizPushConstants.data());
            BeginRender(cmd, { _tmpImage->GetImageView(), _hizImage->GetImageView(level), {mipSize, mipSize } });
            BindGraphicsPipeline(cmd, _hizGraphicsPipeline->GetPipeline());
            SetViewportAndScissor(cmd, {mipSize, mipSize});
            vkCmdDraw(cmd, 6, 1, 0, 0);
            EndRender(cmd);

            {
                ImageBarrier imageBarrier(_hizImage->GetImage(),
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_IMAGE_ASPECT_DEPTH_BIT,
                    level);
                Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier }, std::vector<BufferBarrier>());
            }
        }
        /*{
            ImageBarrier imageBarrier(_hizImage->GetImage(),
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                0, 0,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_ASPECT_DEPTH_BIT
                );
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier }, std::vector<BufferBarrier>());
        }*/

        // second camera ------------------------------------------
        {
            ImageBarrier imageBarrier(_tmpImage->GetImage(),
                0, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT);

            ImageBarrier depthImageBarrier(_depthBuffer->GetImage(),
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_DEPTH_BIT);
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier, depthImageBarrier }, std::vector<BufferBarrier>{ });
        }

        graphicsPushConstants[1] = 1;
        BeginRender(cmd, { _tmpImage->GetImageView(), _depthBuffer->GetImageView(), {_swapchain->GetExtent().width, _swapchain->GetExtent().height } });
        BindGraphicsPipeline(cmd, _graphicsPipeline->GetPipeline());
        SetViewportAndScissor(cmd, _swapchain->GetExtent());
        PushConstant(cmd, _graphicsPipeline->GetPipelineLayout(), 8, graphicsPushConstants.data());
        BindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline->GetPipelineLayout(), 0, _descriptorSetManager->GetBindlessBufferSet());
        DrawIndirect(cmd, i);
        EndRender(cmd);
        {
            ImageBarrier imageBarrier(_tmpImage->GetImage(),
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT , VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT);

            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier }, std::vector<BufferBarrier>{ });
        }
        BlitImage(cmd, _tmpImage->GetImage(), _swapchain->GetImage(i),
            glm::ivec4(0, 0, _swapchain->GetExtent().width, _swapchain->GetExtent().height), 
            glm::ivec4(0, 0, _swapchain->GetExtent().width / 4, _swapchain->GetExtent().height / 4));

        {
            ImageBarrier imageBarrier(_swapchain->GetImage(i),
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                0, 0,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_IMAGE_ASPECT_COLOR_BIT);
            Barrier::PipelineBarrier(cmd, std::vector<ImageBarrier> { imageBarrier}, std::vector<BufferBarrier>());
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
    _depthBuffer = new Image(*_device, width, height, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    _tmpImage = new Image(*_device, width, height, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Application::CreateHizDepthImage() {
    _hizMipLevels = Util::CalHighBit(_maxMipSize) + 1;
    _hizImage = new Image(*_device, _maxMipSize, _maxMipSize, _hizMipLevels, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Application::CreateImageSampler() {
    _depthSampler = new ImageSampler(_device->GetDevice(), VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, true);
    _hizSampler = new ImageSampler(_device->GetDevice(), VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, false);
}

void Application::BindImageDescriptorSets() {
    std::vector<std::pair<VkImageView, VkSampler>> depthImageSample;
    depthImageSample.emplace_back(_depthBuffer->GetImageView(), _depthSampler->GetSampler());
    for (auto i = 0; i < _hizMipLevels; i++) {
        depthImageSample.emplace_back(_hizImage->GetImageView(i), _depthSampler->GetSampler());
    }
    Image::UpdateDescriptorSets(depthImageSample, _device->GetDevice(), _descriptorSetManager->GetBindlessImageSet(), 0);

    // todo : make a function to create image view.
    _hizImage->CreateImageView(_device->GetDevice(), VK_FORMAT_D32_SFLOAT, 0, VK_IMAGE_ASPECT_DEPTH_BIT, _hizMipLevels, _hizImage->GetImage(), _hizImageView);
    /*VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = _hizImage->GetImage();
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = VK_FORMAT_D32_SFLOAT;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = _hizMipLevels;
    createInfo.subresourceRange.layerCount = 1;

    Check(vkCreateImageView(_device->GetDevice(), &createInfo, nullptr, &_hizImageView),
        "create image view");*/

    std::pair<VkImageView, VkSampler> hizImageSample = { _hizImageView, _hizSampler->GetSampler() };
    Image::UpdateDescriptorSets(std::vector<std::pair<VkImageView, VkSampler>>{ hizImageSample }, _device->GetDevice(), _descriptorSetManager->GetBindlessImageSet(), depthImageSample.size());
}

void Application::CreateDescriptorSetManager()
{
    _descriptorSetManager = new DescriptorSetManager(_device->GetDevice());
}

void Application::CreateGraphicsPipeline(uint32_t pushConstantSize, bool isWireFrame)
{
    {
        RenderInfo info{};
        info.viewWidth = _swapchain->GetExtent().width;
        info.viewHeight = _swapchain->GetExtent().height;
        info.useInstance = true;
        info.shaderName = { "shaders/shaderInstance.vert", "shaders/shader.frag" };
        info.compareOp = VK_COMPARE_OP_GREATER;
        info.pushConstantSize = pushConstantSize;
        info.colorAttachmentFormats = std::vector<VkFormat>{ _swapchain->GetImageFormat() };
        info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        _graphicsPipeline = new GraphicsPipeline(*_device, *_descriptorSetManager, info, isWireFrame);
    }
    {
        RenderInfo info{};
        info.viewWidth = _maxMipSize;
        info.viewHeight = _maxMipSize;
        info.useInstance = true;
        info.shaderName = { "shaders/hiz.vert", "shaders/hiz.frag" };
        info.compareOp = VK_COMPARE_OP_ALWAYS;
        info.pushConstantSize = 16;
        info.colorAttachmentFormats = std::vector<VkFormat>{ _swapchain->GetImageFormat() };
        info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        _hizGraphicsPipeline = new GraphicsPipeline(*_device, *_descriptorSetManager, info, false);
    }
}

void Application::CreateComputePipeline(uint32_t pushConstantSize) {
    _computePipeline = new ComputePipeline(*_device, *_descriptorSetManager, pushConstantSize);
}

void Application::BeginRender(VkCommandBuffer cmd, const RenderPassInfo& renderPassInfo)
{
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    std::vector<VkRenderingAttachmentInfo> depthAttachments;

    VkRenderingAttachmentInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    info.imageView = renderPassInfo.colorImageView;
    info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    info.clearValue.color = { 0.1, 0.1, 0.1, 1.0 };
    colorAttachments.push_back(info);

    VkRenderingAttachmentInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthInfo.imageView = renderPassInfo.depthImageView;
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    depthInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    //depthInfo.clearValue.depthStencil.depth = 1.0;
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

void Application::BlitImage(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, const glm::ivec4& srcRegion, const glm::ivec4& dstRegion) {
    VkImageBlit blitInfo{};
    blitInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitInfo.srcSubresource.layerCount = 1;

    blitInfo.srcOffsets[0] = VkOffset3D{ srcRegion.x, srcRegion.y, 0 };
    blitInfo.srcOffsets[1] = VkOffset3D{ srcRegion.z, srcRegion.w, 1 };

    blitInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitInfo.dstSubresource.layerCount = 1;

    blitInfo.dstOffsets[0] = VkOffset3D{ dstRegion.x, dstRegion.y, 0 };
    blitInfo.dstOffsets[1] = VkOffset3D{ dstRegion.z, dstRegion.w, 1 };

    vkCmdBlitImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitInfo, VK_FILTER_LINEAR);
}

void Application::EndRender(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
}

void Application::PushConstant(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, uint32_t size, void* p)
{
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0, size, p);
}

void Application::BindGraphicsPipeline(VkCommandBuffer cmd, VkPipeline pipeline)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
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

void Application::BindDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint usage, VkPipelineLayout pipelineLayout, uint32_t id, VkDescriptorSet descriptorSet)
{
    vkCmdBindDescriptorSets(cmd, usage, pipelineLayout, id, 1, &descriptorSet, 0, nullptr);
}

void Application::SetViewportAndScissor(VkCommandBuffer cmd, VkExtent2D extent2D)
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

    glm::mat4 model2 = glm::mat4(1.f);
    glm::mat4 view2 = _camera2->getViewMatrix();
    glm::mat4 proj2 = _camera2->getProjMatrix();

    _ubo.mvp = proj * view * model;
    _ubo.view = view * model;
    _ubo.proj = proj;
    _ubo.mvp2 = proj2 * view2 * model2;

    if (GetMouseLeftDown()) {
        _camera->rotateByScreenX(_camera->getTarget(), GetMouseHorizontalMove() * 0.015);
        _camera->rotateByScreenY(_camera->getTarget(), GetMouseVerticalMove() * 0.01);
    } else if (GetMouseRightDown()) {
        _camera->moveCamera(GetMouseHorizontalMove(), GetMouseVerticalMove());
    }

    _uniformBuffers[imageId]->Update(&_ubo, sizeof(UniformBuffers));
}

void Application::InitIndirectBuffer(uint32_t imageId) {
    std::vector<uint32_t> initBuffer = { 3 * 128, 0, 0, 0 };
    _indirectBuffers[imageId]->Update(initBuffer.data(), initBuffer.size() * sizeof(uint32_t));
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
    CleanUp(_tmpImage);
    _hizImage->CleanUpImageView(_device->GetDevice(),_hizImageView);
    CleanUp(_hizImage);
    CleanUp(_depthSampler);
    CleanUp(_hizSampler);
    CleanUp(_packedBuffer);
    CleanUp(_constContextBuffer);
    CleanUp(_instanceOffsetBuffer);
   /* for (auto& buffer : _packedClusters)
        CleanUp(buffer);*/
    CleanUp(_descriptorSetManager);
    if (!_useInstance)
        CleanBuffer(_device->GetDevice());
    CleanUp(_graphicsPipeline);
    CleanUp(_hizGraphicsPipeline);
    CleanUp(_computePipeline);
    CleanUp(_syncObjects);
    CleanUp(_commandPool);
    CleanUp(_commandBuffers);
    CleanUp(_window);
    CleanUp(_device);
    CleanUp(_camera);
    CleanUp(_camera2);
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