cmake_minimum_required(VERSION 2.8.1)

project(colourenhancement)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED)

#INCLUDE_DIRECTORIES(${OPENGL_INCLUDE_DIRS})
#LINK_DIRECTORIES(${OPENGL_LIBRARY_DIRS})



add_subdirectory(3rdparty/glfw)
add_subdirectory(3rdparty/glad)

add_executable(${PROJECT_NAME} main.cpp icelut_opengl_demo.cpp)

target_link_libraries(${PROJECT_NAME} ${X11_LIBRARIES} ${OPENGL_LIBRARIES} glfw glad)
target_include_directories(${PROJECT_NAME} PUBLIC include/)


