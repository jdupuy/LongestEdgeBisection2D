cmake_minimum_required (VERSION 3.1)

project(LongestEdgeBisection2D)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall  -Wextra")
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# disable GLFW docs, examples and tests
# see http://www.glfw.org/docs/latest/build_guide.html
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# set path to dependencies
add_subdirectory(submodules/glfw)
include_directories(submodules/glfw/include)
include_directories(submodules/imgui)
include_directories(submodules/stb)
include_directories(submodules/dj_algebra)
include_directories(submodules/dj_opengl)
include_directories(submodules/libcbt)
include_directories(submodules/libleb)

# imgui source files
set(IMGUI_SRC_DIR submodules/imgui)
aux_source_directory(${IMGUI_SRC_DIR} IMGUI_SRC_FILES)

# Add dependencies
find_package(OpenMP)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# ------------------------------------------------------------------------------
set(DEMO subdivision)
set(SRC_DIR subdivision)
include_directories(${SRC_DIR})
aux_source_directory(${SRC_DIR} SRC_FILES)
add_executable(${DEMO} ${IMGUI_SRC_FILES} ${SRC_FILES} ${SRC_DIR}/glad/glad.c)
target_link_libraries(${DEMO} glfw)
target_compile_definitions(
    ${DEMO} PUBLIC
    -DPATH_TO_SRC_DIRECTORY="${CMAKE_SOURCE_DIR}/"
    -DIMGUI_IMPL_OPENGL_LOADER_GLAD
)
unset(SRC_FILES)
unset(DEMO)

# ------------------------------------------------------------------------------
set(DEMO terrain)
set(SRC_DIR terrain)
include_directories(${SRC_DIR})
aux_source_directory(${SRC_DIR} SRC_FILES)
add_executable(${DEMO} ${IMGUI_SRC_FILES} ${SRC_FILES} ${SRC_DIR}/glad/glad.c)
target_link_libraries(${DEMO} glfw)
target_compile_definitions(
    ${DEMO} PUBLIC
    -DPATH_TO_SRC_DIRECTORY="${CMAKE_SOURCE_DIR}/"
    -DIMGUI_IMPL_OPENGL_LOADER_GLAD
)
unset(SRC_FILES)
unset(DEMO)

