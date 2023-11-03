add_requires("vulkansdk", "glfw", "shaderc")

target("vk")
    set_kind("static")
    -- add_rules("utils.symbols.export_all", {export_classes = true})
    add_files("**.cpp")
    add_includedirs(".",{public=true})
    add_packages("vulkansdk", "glm", "glfw", {public = true})
    add_packages("shaderc")
target_end()