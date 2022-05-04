cmake_minimum_required(VERSION 3.14)
project(ecnavdA-yoBemaG)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(SDL2 CONFIG REQUIRED)

add_subdirectory(modules/nativefiledialog-extended)
add_subdirectory(modules/fmt)

include_directories(
	include/
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
	src/arm7tdmidisasm.cpp
	src/gba.cpp
	src/arm7tdmi.cpp
	src/cpu.cpp
	src/hlebios.cpp
	src/apu.cpp
	src/dma.cpp
	src/ppu.cpp
	src/timer.cpp
)

target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
	target_compile_options(ecnavda-yobemag PRIVATE
		/clang:-ftemplate-depth=5000
		/clang:-fconstexpr-depth=5000
		/clang:-MMD
		/clang:-MP
		#-Wall
		#-fsanitize=address
		#-fsanitize=leak
		#-fsanitize=pointer-compare
		#-fsanitize=pointer-subtract
		#-fstack-protector-all
		#-fsanitize=undefined
		#-fsanitize=thread
	)

	target_link_libraries(ecnavda-yobemag PRIVATE
		SDL2::SDL2
		nfd
		fmt
		opengl32
		#-fsanitize=address
		#-fsanitize=leak
		#-fsanitize=pointer-compare
		#-fsanitize=pointer-subtract
		#-fstack-protector-all
		#-fsanitize=undefined
		#-fsanitize=thread
		)
else()
	target_compile_options(ecnavda-yobemag PRIVATE
		-ftemplate-depth=5000
		-fconstexpr-depth=5000
		-MMD
		-MP
		#-Wall
		#-fsanitize=address
		#-fsanitize=leak
		#-fsanitize=pointer-compare
		#-fsanitize=pointer-subtract
		#-fstack-protector-all
		#-fsanitize=undefined
		#-fsanitize=thread
		)

	target_link_libraries(ecnavda-yobemag PRIVATE
		${SDL2_LIBRARIES}
		SDL2::SDL2
		SDL2::SDL2main
		nfd
		fmt
		-lstdc++
		-lSDL2
		-ldl
		-lGL
		-lm
		-lpthread
		#-fsanitize=address
		#-fsanitize=leak
		#-fsanitize=pointer-compare
		#-fsanitize=pointer-subtract
		#-fstack-protector-all
		#-fsanitize=undefined
		#-fsanitize=thread
		)
endif()
