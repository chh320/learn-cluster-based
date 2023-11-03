#include "ShaderModule.h"
#include <fstream>
#include <sstream>
#include <stdexcept>


namespace Vk {
// for .spv
ShaderModule::ShaderModule(VkDevice device, const std::string& filename)
    : ShaderModule(device, ReadFile(filename))
{
}

ShaderModule::ShaderModule(VkDevice device, const std::vector<char>& code)
    : device_(device)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    Check(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule_),
        "create shader module");
}

// for .vert/.frag
ShaderModule::ShaderModule(VkDevice device, const std::vector<uint32_t>& code)
    : device_(device)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * 4;
    createInfo.pCode = code.data();

    Check(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule_),
        "create shader module");
}

ShaderModule::~ShaderModule()
{
    if (shaderModule_ != nullptr) {
        vkDestroyShaderModule(device_, shaderModule_, nullptr);
        shaderModule_ = nullptr;
    }
}

VkPipelineShaderStageCreateInfo ShaderModule::CreateShaderStage(VkShaderStageFlagBits stage) const
{
    VkPipelineShaderStageCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.stage = stage;
    createInfo.module = shaderModule_;
    createInfo.pName = "main";

    return createInfo;
}

std::vector<char> ShaderModule::ReadFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file '" + filename + "'");
    }

    const auto fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

std::vector<uint32_t> ShaderModule::ReadFile(const std::string& filename,
    shaderc_shader_kind kind,
    bool optimize)
{
    static auto read_file = [](const std::string& fn) -> std::string {
        std::ifstream fin(fn, std::ios::in | std::ios::binary);

        if (!fin.is_open()) {
            throw std::runtime_error("failed to open shader file");
        }

        std::stringstream ss;
        ss << fin.rdbuf();

        return ss.str();
    };

    std::string source = read_file(filename);

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    // Just like -DMY_DEFINE=1
    // options.AddMacroDefinition("MY_DEFINE", "1");
    if (optimize) {
        options.SetOptimizationLevel(shaderc_optimization_level_size);
    }

    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, filename.c_str(), options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        return std::vector<uint32_t>();
    }

    return { module.cbegin(), module.cend() };
}
}