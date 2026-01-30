cmake_minimum_required(VERSION 3.20)

project(ProjectCodex LANGUAGES CXX)

add_executable(hello
    src/main.cpp
)

target_compile_features(hello PRIVATE cxx_std_17)

find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

target_link_libraries(hello PRIVATE glad::glad glfw OpenGL::GL)
