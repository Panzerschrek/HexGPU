cmake_minimum_required(VERSION 3.15)

project(HexGPU)

# Setup language options.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

# Setup compiler options.
if(MSVC)
	add_compile_options(/utf-8)
	add_compile_options(/W3 /WX)
	add_definitions(
		-DNOMINMAX
		-D_CRT_SECURE_NO_WARNINGS)
else()
	add_compile_options(-Werror -Wall -Wextra -Wconversion)
	add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)
	add_compile_options(-fdata-sections -ffunction-sections) # simplify later gc-sections.

	# Clear garbage.
	string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -s -Wl,--gc-sections")
	string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " -s -Wl,--gc-sections")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG)
endif()

# Search dependencies.
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

find_package(Vulkan REQUIRED)

find_program(GLSLANGVALIDATOR glslangValidator)
if(NOT GLSLANGVALIDATOR)
	message(FATAL_ERROR "glslangValidator not found")
endif()

# Compile shaders.
file(GLOB SHADERS "shaders/*")
foreach(SHADER_FILE ${SHADERS})
	file(RELATIVE_PATH OUT_FILE ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER_FILE})
	set(OUT_FILE_BASE ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE})
	string(REPLACE ".glsl" ".h" OUT_FILE_H ${OUT_FILE_BASE})
	get_filename_component(VARIABLE_NAME ${SHADER_FILE} NAME)
	string(REPLACE ".glsl" "" VARIABLE_NAME ${VARIABLE_NAME})
	string(REPLACE "." "_" VARIABLE_NAME ${VARIABLE_NAME})
	add_custom_command(
		OUTPUT ${OUT_FILE_H}
		DEPENDS ${SHADER_FILE}
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
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/ShaderList.cpp PROPERTIES OBJECT_DEPENDS ${SHADERS_COMPILED})

# Add main executable.
file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp")

add_executable(
	HexGPU
		${SOURCES}
		${GENERATED_SOURCES}
		${SHADERS}
		${SHADERS_COMPILED})

target_include_directories(
	HexGPU
		PRIVATE
			${CMAKE_CURRENT_BINARY_DIR}
			${SDL2_INCLUDE_DIRS}
			${Vulkan_INCLUDE_DIRS}
	)

target_link_libraries(
	HexGPU
		PRIVATE
			${SDL2_LIBRARIES}
			${Vulkan_LIBRARIES}
	)
