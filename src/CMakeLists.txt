cmake_minimum_required(VERSION 3.1)
project(gsplus)

INCLUDE (CheckFunctionExists)
INCLUDE (CheckLibraryExists)
INCLUDE (CheckTypeSize)
INCLUDE (CheckIncludeFile)
INCLUDE (CheckCSourceCompiles)

INCLUDE (FindPkgConfig)
INCLUDE (TestBigEndian)


set(PACKAGE_NAME "GSPlus")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)


if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	add_compile_options(
		-O2 -Wall -fomit-frame-pointer
	)
endif()


if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
	add_compile_options(
		-O2 -Wall -fomit-frame-pointer
	)
endif()



TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(NOT IS_BIG_ENDIAN)
	add_definitions(-DGSPLUS_LITTLE_ENDIAN)
endif()


pkg_check_modules(SDL2 sdl2)

#
# run ccmake, cmake -LH, or cmake -D...
#
set(DRIVER "SDL" CACHE STRING "Driver (SDL, X11, WIN32, FB, or HEADLESS")
option(ENABLE_DEBUGGER "Enable the debugger" ON)
option(ENABLE_HOST_FST "Enable host fst support" ON)

set(generated_headers 8inst_c.h 16inst_c.h 8inst_s.h 16inst_s.h size_c.h size_s.h 8size_s.h 16size_s.h)
add_custom_command(
	OUTPUT 8inst_c.h 16inst_c.h 8inst_s.h ${CMAKE_CURRENT_BINARY_DIR}/16inst_s.h
	COMMAND perl make_inst c 8 instable.h > ${CMAKE_CURRENT_BINARY_DIR}/8inst_c.h
 	COMMAND perl make_inst c 16 instable.h > ${CMAKE_CURRENT_BINARY_DIR}/16inst_c.h
 	COMMAND perl make_inst s 8 instable.h > ${CMAKE_CURRENT_BINARY_DIR}/8inst_s.h
 	COMMAND perl make_inst s 16 instable.h > ${CMAKE_CURRENT_BINARY_DIR}/16inst_s.h
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/instable.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)


add_custom_command(
	OUTPUT size_c.h size_s.h 8size_s.h 16size_s.h
	COMMAND perl make_size c size_tab.h > ${CMAKE_CURRENT_BINARY_DIR}/size_c.h
	COMMAND perl make_size s size_tab.h > ${CMAKE_CURRENT_BINARY_DIR}/size_s.h
	COMMAND perl make_size 8 size_tab.h > ${CMAKE_CURRENT_BINARY_DIR}/8size_s.h
	COMMAND perl make_size 16 size_tab.h > ${CMAKE_CURRENT_BINARY_DIR}/16size_s.h
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/size_tab.h 
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

if(CYGWIN OR MSYS)
	set(WIN32 1)
	add_definitions(-DWIN32 -D_WIN32)
endif()

if(WIN32)
	add_definitions(-D__USE_W32_SOCKETS -D_WINSOCK2API_)
endif()

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()


add_executable(to_pro to_pro.c)
add_executable(partls partls.c)

add_subdirectory(atbridge)
add_subdirectory(tfe)

if (DRIVER MATCHES "SDL")
	set(driver_code sdl2_driver.c sdl2snd_driver.c)
elseif(DRIVER MATCHES "X11")
	set(driver_code xdriver.c)
elseif(DRIVER MATCHES "FB")
	set(driver_code fbdriver.c)
elseif(DRIVER MATCHES "WIN32")
	set(driver_code scc_windriver.c win32snd_driver.c win_console.c win_generic.c)
elseif(DRIVER MATCHES "HEADLESS")
	set(driver_code headless_driver.c)
else()
	message(FATAL_ERROR "Invalid driver ${DRIVER}")
endif()

if (WIN32)
	set(host_fst_code win32_host_fst.c)
else()
	set(host_fst_code host_fst.c)
endif()


add_executable(gsplus WIN32
	adb.c clock.c config.c dis.c engine_c.c scc.c iwm.c 
	joystick_driver.c moremem.c paddles.c parallel.c printer.cpp sim65816.c
	smartport.c sound.c sound_driver.c video.c scc_socket_driver.c glog.c
	imagewriter.cpp scc_imagewriter.c scc_llap.c host_fst.c


	$<$<BOOL:${ENABLE_DEBUGGER}>:debug.c>
	$<$<BOOL:${ENABLE_HOST_FST}>:${host_fst_code}>
	${driver_code}
	${generated_headers}

	$<$<BOOL:${WIN32}>:win32.rc>
)

target_link_libraries(gsplus atbridge)
target_link_libraries(gsplus tfe)

if (DRIVER MATCHES "WIN32")
	target_link_libraries(gsplus comdlg32 Shlwapi IPHlpApi
		winmm gdi32 dsound comctl32 ws2_32 shell32
	)
endif()

if (DRIVER MATCHES "SDL")
	target_link_libraries(gsplus ${SDL2_LDFLAGS} SDL2_image)
	target_compile_options(gsplus PUBLIC ${SDL2_CFLAGS})
endif()

#if (APPLE AND DRIVER MATCHES "SDL")
#	target_compile_options(gsplus PRIVATE -F${CMAKE_CURRENT_SOURCE_DIR} )
#	target_link_libraries(gsplus -F${CMAKE_CURRENT_SOURCE_DIR} "-framework SDL2" -Wl,-rpath,@executable_path/../Frameworks)
#endif()
