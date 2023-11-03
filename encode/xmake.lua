add_requires("coost")

target("encode")
    set_kind("headeronly")
    add_headerfiles("*.h")
    add_deps("virtualMesh", "util")
    add_packages("coost", {public = true})
    add_includedirs(".", {public=true})
target_end()