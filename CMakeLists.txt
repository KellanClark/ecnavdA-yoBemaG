cmake_minimum_required(VERSION 3.10)
project(ecnavdA-yoBemaG)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(SDL2 REQUIRED)

add_subdirectory(modules/nativefiledialog-extended)
add_subdirectory(modules/fmt)

include_directories(
	include/
	${SDL2_INCLUDE_DIRS}
	modules/imgui/
	modules/imgui_club/imgui_memory_editor
)

add_executable(ecnavda-yobemag
	modules/imgui/imgui_draw.cpp
	modules/imgui/imgui_demo.cpp
	modules/imgui/imgui_tables.cpp
	modules/imgui/imgui_widgets.cpp
	modules/imgui/imgui.cpp
	modules/imgui/backends/imgui_impl_sdl.cpp
	modules/imgui/backends/imgui_impl_opengl3.cpp

	src/main.cpp
	src/ppudebug.cpp
	src/scheduler.cpp
	src/gba.cpp
	src/arm7tdmi.cpp
	src/cpu.cpp
	src/ppu.cpp
)

target_compile_options(ecnavda-yobemag PRIVATE
	-ftemplate-depth=5000
	-fconstexpr-depth=5000
	-MMD
	-MP
	-Wall
#[[	-fsanitize=address
	-fsanitize=leak
	-fsanitize=pointer-compare
	-fsanitize=pointer-subtract
	-fsanitize=undefined
	-fstack-protector-all
	#-fsanitize=thread]]
)

target_link_libraries(ecnavda-yobemag PRIVATE
	${SDL2_LIBRARIES}
	nfd
	fmt
	-lstdc++
	-lSDL2
	-ldl
	-lGL
	-lm
	-lpthread
#[[	-fsanitize=address
	-fsanitize=leak
	-fsanitize=undefined
	-fsanitize=pointer-compare
	-fsanitize=pointer-subtract
	-fstack-protector-all
	#-fsanitize=thread]]
)

set(CMAKE_CXX_FLAGS_DEBUG "-g -Og")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3")