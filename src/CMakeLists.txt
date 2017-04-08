cmake_minimum_required(VERSION 3.1)
project(gsplus)

#
# cmake -DCMAKE_BUILD_TYPE=Release
#

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
		-Wall -fomit-frame-pointer
	)
endif()


if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
	add_compile_options(
		-Wall -fomit-frame-pointer
	)
endif()

set(LINUX OFF)
if (CMAKE_SYSTEM MATCHES "Linux")
	set(LINUX ON)
endif()



TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(NOT IS_BIG_ENDIAN)
	add_definitions(-DGSPLUS_LITTLE_ENDIAN)
endif()


pkg_check_modules(SDL2 sdl2)
pkg_check_modules(FREETYPE2 freetype2)


set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

#
# run ccmake, cmake -LH, or cmake -D...
#
set(DRIVER "SDL" CACHE STRING "Driver (SDL, X11, WIN32, FB, or HEADLESS")
option(DEBUGGER "Enable the debugger" ON)
option(HOST_FST "Enable host fst support" ON)
option(HOST_MW "Enable host ModemWorks support" ON)
option(TOGGLE_STATUS "Enable F10 Toggle Status support (win32/x11)" OFF)
option(ETHERNET "Enable Ethernet Support" ON)


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

if (ETHERNET)
	add_subdirectory(atbridge)
	add_subdirectory(tfe)
endif()

add_subdirectory(hash)

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

# https://cmake.org/cmake/help/latest/prop_tgt/MACOSX_BUNDLE_INFO_PLIST.html
set(MACOSX_BUNDLE_INFO_STRING "0.12s, Copyright 2016 Dagen Brock")
set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2016 Dagen Brock")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "0.12s")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING "0.12s")
set(MACOSX_BUNDLE_BUNDLE_VERSION "0.12s")
set(MACOSX_BUNDLE_ICON_FILE gsp-icons.icns)
set(MACOSX_BUNDLE_GUI_IDENTIFIER com.dagenbrock.gsplus)

# https://cmake.org/Wiki/CMake:Bundles_And_Frameworks
# OS X properties.
add_executable(GSPlus WIN32 MACOSX_BUNDLE
	adb.c clock.c config.c dis.c engine_c.c scc.c iwm.c 
	joystick_driver.c moremem.c paddles.c parallel.c printer.cpp sim65816.c
	smartport.c sound.c sound_driver.c video.c scc_socket_driver.c glog.c
	imagewriter.cpp scc_imagewriter.c scc_llap.c host_fst.c


	$<$<BOOL:${DEBUGGER}>:debug.c>
	$<$<BOOL:${HOST_FST}>:${host_fst_code}>
	$<$<BOOL:${HOST_MW}>:host_mw.c>
	${driver_code}
	${generated_headers}

	$<$<BOOL:${WIN32}>:win32.rc>
	$<$<BOOL:${APPLE}>:gsp_icon.icns>
	$<$<BOOL:${APPLE}>:fix_mac_menu.m>
)


SET_SOURCE_FILES_PROPERTIES(
  gsp_icon.icns
  PROPERTIES
  MACOSX_PACKAGE_LOCATION Resources
 )

if (ETHERNET)
	target_link_libraries(GSPlus atbridge)
	target_link_libraries(GSPlus tfe)
endif()

if (DRIVER MATCHES "WIN32")
	target_link_libraries(GSPlus comdlg32 Shlwapi IPHlpApi
		winmm gdi32 dsound comctl32 ws2_32 shell32
	)
endif()

if (DRIVER MATCHES "SDL")
	target_link_libraries(GSPlus ${SDL2_LDFLAGS} ${FREETYPE2_LDFLAGS} SDL2_image)
	target_compile_options(GSPlus PUBLIC ${SDL2_CFLAGS} ${FREETYPE2_CFLAGS}  -DHAVE_SDL)
endif()

if (APPLE)
	target_link_libraries(GSPlus "-framework Cocoa")
endif()
if (TOGGLE_STATUS)
	target_compile_options(GSPlus PUBLIC -DTOGGLE_STATUS)
endif()


if (HOST_MW)
	if (LINUX)
		target_link_libraries(GSPlus -lutil)
	endif()
	target_link_libraries(GSPlus Threads::Threads)
	target_link_libraries(GSPlus hash)
endif()

#if (APPLE AND DRIVER MATCHES "SDL")
#	target_compile_options(GSPlus PRIVATE -F${CMAKE_CURRENT_SOURCE_DIR} )
#	target_link_libraries(GSPlus -F${CMAKE_CURRENT_SOURCE_DIR} "-framework SDL2" -Wl,-rpath,@executable_path/../Frameworks)
#endif()

if (APPLE AND CMAKE_BUILD_TYPE MATCHES "Release")
	add_custom_command(TARGET GSPlus
		POST_BUILD
		COMMAND dylibbundler -od -b -x GSPlus.app/Contents/MacOS/GSPlus -d GSPlus.app/Contents/Frameworks
		#COMMAND cp to_pro partls GSPlus.app/Contents/Resources/
		COMMENT bundling libraries...
	)
endif()
