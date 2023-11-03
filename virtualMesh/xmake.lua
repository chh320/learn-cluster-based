add_requires("metis")

target("virtualMesh")
    set_kind("static")
    add_files("*.cpp")
    add_deps("meshSimplify")
    add_packages("metis", "glm")
    add_includedirs(".",{public=true})
target_end()