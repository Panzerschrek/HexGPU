cmake_minimum_required(VERSION 3.15)

project(HexGPU)

# Setup language options.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

# Setup compiler options.
if(MSVC)
	add_compile_options(/utf-8)
	add_definitions(
		-DNOMINMAX
		-D_CRT_SECURE_NO_WARNINGS)
else()
	add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)
	add_compile_options(-fdata-sections -ffunction-sections) # simplify later gc-sections.

	# Clear garbage.
	string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -s -Wl,--gc-sections")
	string(APPEND CMAKE_EXE_LINKER_FLAGS_MINSIZEREL " -s -Wl,--gc-sections")
	string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " -s -Wl,--gc-sections")
	string(APPEND CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL " -s -Wl,--gc-sections")
endif()

#
# Preparedependencies.
#

# snappy
set(SNAPPY_BUILD_TESTS NO CACHE BOOL "")
set(SNAPPY_BUILD_BENCHMARKS NO CACHE BOOL "")
add_subdirectory(../snappy snappy)

# SDL2
if(WIN32)
	set(SDL2_INCLUDE_DIRS ${SDL2_DIR}/include)
	if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
		set(ARCH_DIR x64)
	else()
		set(ARCH_DIR x86)
	endif()
	set(SDL2_LIBRARIES ${SDL2_DIR}/lib/${ARCH_DIR}/SDL2.lib ${SDL2_DIR}/lib/${ARCH_DIR}/SDL2main.lib)
else()
	find_package(SDL2 REQUIRED)
endif()

# Vulkan
find_package(Vulkan REQUIRED)

# glslangValidator
find_program(GLSLANGVALIDATOR glslangValidator)
if(NOT GLSLANGVALIDATOR)
	message(FATAL_ERROR "glslangValidator not found")
endif()

# Add ImGui. Do this before specifying our compiler warning options.
set(IMGUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../imgui/")
set(IMGUI_SOURCES
	${IMGUI_ROOT}/imgui.cpp
	${IMGUI_ROOT}/imgui_draw.cpp
	${IMGUI_ROOT}/imgui_tables.cpp
	${IMGUI_ROOT}/imgui_widgets.cpp
	${IMGUI_ROOT}/backends/imgui_impl_sdl2.cpp
	${IMGUI_ROOT}/backends/imgui_impl_vulkan.cpp
	)

add_library(ImGui STATIC ${IMGUI_SOURCES})

target_include_directories(
	ImGui
		PUBLIC
			${SDL2_INCLUDE_DIRS}
			${Vulkan_INCLUDE_DIRS}
			"${IMGUI_ROOT}"
			"${IMGUI_ROOT}/backends"
	)

# Setup compiler warnign options.
if(MSVC)
	add_compile_options(/W3 /WX)
else()
	add_compile_options(-Werror -Wall -Wextra -Wconversion)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG)
endif()

# Compile shaders.
file(GLOB SHADERS "shaders/*.glsl")
file(GLOB SHADERS_INCLUDE "shaders/inc/*")
foreach(SHADER_FILE ${SHADERS})
	file(RELATIVE_PATH OUT_FILE ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER_FILE})
	set(OUT_FILE_BASE ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE})
	string(REPLACE ".glsl" ".h" OUT_FILE_H ${OUT_FILE_BASE})
	get_filename_component(VARIABLE_NAME ${SHADER_FILE} NAME)
	string(REPLACE ".glsl" "" VARIABLE_NAME ${VARIABLE_NAME})
	string(REPLACE "." "_" VARIABLE_NAME ${VARIABLE_NAME})
	add_custom_command(
		OUTPUT ${OUT_FILE_H}
		DEPENDS ${SHADER_FILE} ${SHADERS_INCLUDE}
		COMMAND ${GLSLANGVALIDATOR} -V ${SHADER_FILE} --vn ${VARIABLE_NAME} -o ${OUT_FILE_H}
		)

	list(APPEND SHADERS_COMPILED ${OUT_FILE_H})
	set(SHADER_VARIABLES_LIST "${SHADER_VARIABLES_LIST}\t${VARIABLE_NAME},\n")
	set(SHADER_INCLUDES_LIST "${SHADER_INCLUDES_LIST}#include \"${OUT_FILE_H}\"\n")
	set(SHADER_LOOKUP_LIST "${SHADER_LOOKUP_LIST}PROCESS_SHADER(${VARIABLE_NAME})\n\t\t")
endforeach()

# Write shader list files.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ShaderList.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/ShaderList.hpp)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ShaderList.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/ShaderList.cpp)
set(GENERATED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/ShaderList.hpp ${CMAKE_CURRENT_BINARY_DIR}/ShaderList.cpp)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/ShaderList.cpp PROPERTIES OBJECT_DEPENDS "${SHADERS_COMPILED}")

# Add main executable.
file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp")

add_executable(
	HexGPU
		${SOURCES}
		${GENERATED_SOURCES}
		${SHADERS}
		${SHADERS_INCLUDE}
		${SHADERS_COMPILED}
	)

target_include_directories(
	HexGPU
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_CURRENT_BINARY_DIR}
			${SDL2_INCLUDE_DIRS}
			${Vulkan_INCLUDE_DIRS}
	)

target_link_libraries(
	HexGPU
		PRIVATE
			${SDL2_LIBRARIES}
			${Vulkan_LIBRARIES}
			ImGui
			snappy
	)
