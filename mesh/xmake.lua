
target("mesh")
    set_kind("static")
    add_files("*.cpp")
    add_deps("util")
    add_packages("glm", "assimp")
    add_includedirs(".", {public=true})
target_end()