set_languages("c++20")

target("util")
    set_kind("static")
    add_files("*.cpp")
    add_packages("glm")
    add_includedirs(".",{public=true})
target_end()