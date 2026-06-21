cmake_minimum_required(VERSION 4.2)
project(SDL1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME} src/main.cpp
        src/App.cpp
        src/App.h
        src/core/Object.h
        src/core/Vector.h
        src/core/Mesh.h
        src/core/Int3.h
        src/core/Matrix3D.h
        src/core/Int3.h
        src/core/Camera.cpp
        src/core/Camera.h
        src/core/Matrix4D.h
        src/core/Vector.cpp
        src/core/Int3.cpp
        src/core/Vertex3D.h
        src/core/Chunk.h
        src/core/noise/SimplexNoise.cpp
        src/core/noise/SimplexNoise.h
        src/Ray.h
        src/core/ChunkManager.cpp
        src/core/ChunkManager.h
)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)

find_package(SDL3_image CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-shared>,SDL3_image::SDL3_image-shared,SDL3_image::SDL3_image-static>)

find_package(SDL3_ttf CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_ttf::SDL3_ttf)

find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

set(SHADER_DIR "${CMAKE_SOURCE_DIR}/src/shaders")

add_custom_command(
        OUTPUT "${SHADER_DIR}/vertex.spv"
        COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_DIR}/vertex.glsl" -S vert -o "${SHADER_DIR}/vertex.spv"
        DEPENDS "${SHADER_DIR}/vertex.glsl"
)

add_custom_command(
        OUTPUT "${SHADER_DIR}/fragment.spv"
        COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_DIR}/fragment.glsl" -S frag -o "${SHADER_DIR}/fragment.spv"
        DEPENDS "${SHADER_DIR}/fragment.glsl"
)

add_custom_target(Shaders DEPENDS
        "${SHADER_DIR}/vertex.spv"
        "${SHADER_DIR}/fragment.spv"
)

add_dependencies(${PROJECT_NAME} Shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${SHADER_DIR}/vertex.spv"
        "${SHADER_DIR}/fragment.spv"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        VERBATIM)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/img
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/src/img
)