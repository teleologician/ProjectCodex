cmake_minimum_required(VERSION 3.20)

project(VampireDiary LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/bin/${CONFIG}")
endforeach()

add_executable(vampire
    src/main.cpp
)

target_include_directories(vampire PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

target_compile_definitions(vampire PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(vampire PRIVATE
    glad::glad
    glm::glm
    SDL2::SDL2
    SDL2::SDL2main
    OpenGL::GL
)
