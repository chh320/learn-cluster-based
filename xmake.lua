set_languages("c++20")
add_rules("mode.debug", "mode.release")

add_requires("glm", "assimp")

includes("vk")
includes("mesh")
includes("meshSimplify")
includes("virtualMesh")
includes("util")
includes("application")
includes("encode")