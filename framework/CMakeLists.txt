# project name, and type
PROJECT(OpenApoc_Framework CXX C)

option(DIALOG_ON_ERROR "Pop up a dialog box showing errors" ON)


set (FRAMEWORK_SOURCE_FILES
	configfile.cpp
	data.cpp
	event.cpp
	font.cpp
	framework.cpp
	harness.cpp
	image.cpp
	jukebox.cpp
	logger.cpp
	palette.cpp
	physfs_fs.cpp
	renderer.cpp
	serialization/serialize.cpp
	serialization/providers/filedataprovider.cpp
	serialization/providers/zipdataprovider.cpp
	sound.cpp
	stagestack.cpp
	video/smk.cpp
	logger_sdldialog.cpp
	logger_file.cpp
	options.cpp
	modinfo.cpp
	os/app_paths.cpp
	os/display_size.cpp)

source_group(framework\\sources FILES ${FRAMEWORK_SOURCE_FILES})
list(APPEND ALL_SOURCE_FILES ${FRAMEWORK_SOURCE_FILES})

set (FRAMEWORK_HEADER_FILES
	configfile.h
	data.h
	event.h
	font.h
	framework.h
	filesystem.h
	harness.h
	fs.h
	image.h
	imageloader_interface.h
	jukebox.h
	keycodes.h
	logger.h
	musicloader_interface.h
	palette.h
	renderer.h
	renderer_interface.h
	sampleloader_interface.h
	serialization/serialize.h
	sound.h
	sound_interface.h
	stage.h
	stagestack.h
	video.h
	options.h
	modinfo.h
	os/app_paths.h
	os/display_size.h
	os/file_picker.h)

source_group(framework\\headers FILES ${FRAMEWORK_HEADER_FILES})
list(APPEND ALL_HEADER_FILES ${FRAMEWORK_HEADER_FILES})

set (APOCRESOURCES_SOURCE_FILES
	apocresources/apocfont.cpp
	apocresources/apocpalette.cpp
	apocresources/cursor.cpp
	apocresources/loftemps.cpp
	apocresources/pck.cpp
	apocresources/rawimage.cpp)

source_group(framework\\apocresources\\sources FILES
		${APOCRESOURCES_SOURCE_FILES})
list(APPEND ALL_SOURCE_FILES ${APOCRESOURCES_SOURCE_FILES})

set (APOCRESOURCES_HEADER_FILES
	apocresources/apocfont.h
	apocresources/apocpalette.h
	apocresources/cursor.h
	apocresources/loftemps.h
	apocresources/pck.h
	apocresources/rawimage.h)

source_group(framework\\apocresources\\headers FILES
		${APOCRESOURCES_HEADER_FILES})
list(APPEND ALL_HEADER_FILES ${APOCRESOURCES_HEADER_FILES})


set (IMAGELOADER_SOURCE_FILES
	imageloader/lodepng_image.cpp
	imageloader/pcx.cpp)

source_group(framework\\imageloader\\sources FILES
		${IMAGELOADER_SOURCE_FILES})
list(APPEND ALL_SOURCE_FILES ${IMAGELOADER_SOURCE_FILES})

set (IMAGELOADER_HEADER_FILES
	)

source_group(framework\\imageloader\\headers FILES
	${IMAGELOADER_HEADER_FILES})
list(APPEND ALL_HEADER_FILES ${IMAGELOADER_HEADER_FILES})

set (SAMPLELOADER_SOURCE_FILES
	sampleloader/rawsound.cpp
	sampleloader/wavsound.cpp)

source_group(framework\\sampleloader\\sources FILES
		${SAMPLELOADER_SOURCE_FILES})
list(APPEND ALL_SOURCE_FILES ${SAMPLELOADER_SOURCE_FILES})

set (SAMPLELOADER_HEADER_FILES)

source_group(framework\\sampleloader\\headers FILES
	${SAMPLELOADER_HEADER_FILES})
list(APPEND ALL_HEADER_FILES ${SAMPLELOADER_HEADER_FILES})

set (MUSICLOADER_SOURCE_FILES
	musicloader/music.cpp
	musicloader/vorbis.cpp)

source_group(framework\\musicloader\\sources FILES
		${MUSICLOADER_SOURCE_FILES})
list(APPEND ALL_SOURCE_FILES ${MUSICLOADER_SOURCE_FILES})

set (MUSICLOADER_HEADER_FILES)

source_group(framework\\musicloader\\headers FILES
	${MUSICLOADER_HEADER_FILES})
list(APPEND ALL_HEADER_FILES ${MUSICLOADER_HEADER_FILES})

set (SOUND_SOURCE_FILES
	sound/null_backend.cpp
	sound/sdlraw_backend.cpp)

source_group(framework\\sound\\sources FILES
		${SOUND_SOURCE_FILES})
list(APPEND ALL_SOURCE_FILES ${SOUND_SOURCE_FILES})

set (SOUND_HEADER_FILES)

source_group(framework\\sound\\headers FILES
	${SOUND_HEADER_FILES})
list(APPEND ALL_HEADER_FILES ${SOUND_HEADER_FILES})

set (RENDERER_SOURCE_FILES
	render/gles30_v2/ogles_3_0_renderer_v2.cpp
	render/gles30_v2/gleswrap_gles3.cpp)
if(NOT IOS)
	list(APPEND RENDERER_SOURCE_FILES render/gl20/ogl_2_0_renderer.cpp)
endif()

source_group(framework\\renderer\\sources FILES
	${RENDERER_SOURCE_FILES})
	list(APPEND ALL_SOURCE_FILES ${RENDERER_SOURCE_FILES})

set (RENDERER_HEADER_FILES
	render/gles30_v2/gleswrap_gles3.h
	render/gles30_v2/gleswrap.h
	render/gles30_v2/stb_rect_pack.h
	)
if(NOT IOS)
	list(APPEND RENDERER_HEADER_FILES
		render/gl20/gl_2_0.hpp
		render/gl20/gl_2_0.inl)
endif()

source_group(framework\\renderer\\headers FILES
	${RENDERER_HEADER_FILES})
	list(APPEND ALL_HEADER_FILES ${RENDERER_HEADER_FILES})

set (FILESYSTEM_SOURCE_FILES
	fs/physfs_archiver_cue.cpp)

source_group(framework\\fs\\sources FILES ${FILESYSTEM_SOURCE_FILES})
list(APPEND ALL_SOURCE_FILES ${FILESYSTEM_SOURCE_FILES})

set (FILESYSTEM_HEADER_FILES
	fs/physfs_archiver_cue.h)

source_group(framework\\fs\\headers FILES ${FILESYSTEM_HEADER_FILES})
list(APPEND ALL_HEADER_FILES ${FILESYSTEM_HEADER_FILES})

add_library(OpenApoc_Framework STATIC
	${FRAMEWORK_SOURCE_FILES} ${FRAMEWORK_HEADER_FILES}
	${APOCRESOURCES_SOURCE_FILES} ${APOCRESOURCES_HEADER_FILES}
	${IMAGELOADER_SOURCE_FILES} ${IMAGELOADER_HEADER_FILES}
	${SAMPLELOADER_SOURCE_FILES} ${SAMPLELOADER_HEADER_FILES}
	${MUSICLOADER_SOURCE_FILES} ${MUSICLOADER_HEADER_FILES}
	${SOUND_SOURCE_FILES} ${SOUND_HEADER_FILES}
	${RENDERER_SOURCE_FILES} ${RENDERER_HEADER_FILES}
	${FILESYSTEM_SOURCE_FILES} ${FILESYSTEM_HEADER_FILES})

if(IOS)
	target_sources(OpenApoc_Framework PRIVATE os/ios_file_picker.mm)
	set_source_files_properties(os/ios_file_picker.mm PROPERTIES
		LANGUAGE OBJCXX
		COMPILE_FLAGS "-fobjc-arc")
	target_compile_definitions(OpenApoc_Framework PUBLIC
			"-DRENDERERS=\"GLES_3_0\""
			"-DOPENAPOC_GLES")
	target_compile_definitions(OpenApoc_Framework PRIVATE
			"-DGLESWRAP_PLATFORM_SDL")
	target_compile_definitions(OpenApoc_Framework PRIVATE
			"-DBROKEN_THREAD_LOCAL")
elseif(APPLE)
	target_sources(OpenApoc_Framework PRIVATE os/macos_file_picker.mm)
	set_source_files_properties(os/macos_file_picker.mm PROPERTIES
		LANGUAGE OBJCXX
		COMPILE_FLAGS "-fobjc-arc")
	target_compile_definitions(OpenApoc_Framework PUBLIC
			"-DRENDERERS=\"GLES_3_0:GL_2_0\"")
	target_compile_definitions(OpenApoc_Framework PRIVATE
			"-DGLESWRAP_PLATFORM_MACHO")
	# OSX doesn't seem to support the c++11 'thread_local' keyword
	target_compile_definitions(OpenApoc_Framework PRIVATE
			"-DBROKEN_THREAD_LOCAL")
else()
	target_sources(OpenApoc_Framework PRIVATE os/file_picker_stub.cpp)
	target_compile_definitions(OpenApoc_Framework PUBLIC
			"-DRENDERERS=\"GLES_3_0:GL_2_0\"")
endif()

if(WIN32)
	target_compile_definitions(OpenApoc_Framework PRIVATE "-DGLESWRAP_PLATFORM_WGL")
	target_link_libraries(OpenApoc_Framework PUBLIC ws2_32)
elseif(NOT APPLE)
	target_compile_definitions(OpenApoc_Framework PRIVATE "-DGLESWRAP_PLATFORM_GLX")
endif()

# We use boost::locale for utf conversions and boost::filesystem for
# serialization

set(BOOST_PACKAGES locale program_options)
# Arbitrary version that I remember testing previously
set(BOOST_VERSION 1.50)

target_compile_definitions(OpenApoc_Framework PUBLIC "-DUSE_STD_FILESYSTEM")

find_package(Boost ${BOOST_VERSION} REQUIRED COMPONENTS ${BOOST_PACKAGES})
target_link_libraries(OpenApoc_Framework PUBLIC ${Boost_LIBRARIES})
target_include_directories(OpenApoc_Framework PUBLIC ${Boost_INCLUDE_DIRS})

find_package(Threads)
target_link_libraries(OpenApoc_Framework PUBLIC ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(OpenApoc_Framework PUBLIC ${GLM_INCLIDE_DIR})

#Require SDL2 for input/graphics/sound/pretty much everything
find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
	target_link_libraries(OpenApoc_Framework PUBLIC SDL2::SDL2)
	if(TARGET SDL2::SDL2main)
		target_link_libraries(OpenApoc_Framework PUBLIC SDL2::SDL2main)
	endif()
	if(WIN32)
		set_target_properties(SDL2::SDL2 PROPERTIES
			MAP_IMPORTED_CONFIG_MINSIZEREL Release
			MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
		)
		if(TARGET SDL2::SDL2main)
			set_target_properties(SDL2::SDL2main PROPERTIES
				MAP_IMPORTED_CONFIG_MINSIZEREL Release
				MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
			)
		endif()
	endif()
elseif(WIN32 OR IOS)
	message(FATAL_ERROR "SDL2 CMake package required on this platform")
else()
	find_package(PkgConfig)
	pkg_check_modules(PC_SDL2 REQUIRED sdl2>=2.0)
	if (NOT PC_SDL2_FOUND)
			message(FATAL_ERROR "sdl2 not found")
	endif()
	foreach (SDL2_LIB ${PC_SDL2_LIBRARIES})
			message("Searching for ${SDL2_LIB} in ${PC_SDL2_LIBRARY_DIRS}")
			find_library(SDL2_LIBRARY_PATH-${SDL2_LIB} ${SDL2_LIB} HINTS ${PC_SDL2_LIBRARY_DIRS})
			if (NOT SDL2_LIBRARY_PATH-${SDL2_LIB})
					message(FATAL_ERROR "sdl2 library ${SDL2_LIB} not found in ${PC_SDL2_LIBRARY_DIRS}")
			endif()
			message("Found ${SDL2_LIB} at ${SDL2_LIBRARY_PATH-${SDL2_LIB}}")
			target_link_libraries(OpenApoc_Framework PUBLIC ${SDL2_LIBRARY_PATH-${SDL2_LIB}})
	endforeach()
	target_include_directories(OpenApoc_Framework PUBLIC ${PC_SDL2_INCLUDE_DIRS})
endif()

find_package(Vorbis CONFIG QUIET)
if(TARGET Vorbis::vorbisfile)
	target_link_libraries(OpenApoc_Framework PRIVATE Vorbis::vorbisfile)
elseif(TARGET Vorbis::vorbis)
	target_link_libraries(OpenApoc_Framework PRIVATE Vorbis::vorbis)
else()
	find_path(VORBIS_INCLUDE_DIR vorbis/vorbisfile.h)
	find_library(VORBISFILE_LIBRARY vorbisfile)
	find_library(VORBIS_LIBRARY vorbis)
	find_library(OGG_LIBRARY ogg)
	target_include_directories(OpenApoc_Framework PRIVATE ${VORBIS_INCLUDE_DIR})
	target_link_libraries(OpenApoc_Framework PRIVATE ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})
endif()

# Dialog is handled by SDL2
if(DIALOG_ON_ERROR)
	target_compile_definitions(OpenApoc_Framework PUBLIC -DERROR_DIALOG)
endif()

if(IOS)
	find_library(OPENGLES_LIBRARY OpenGLES REQUIRED)
	find_library(UIKIT_LIBRARY UIKit REQUIRED)
	find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
	find_library(UTTYPES_LIBRARY UniformTypeIdentifiers REQUIRED)
	target_link_libraries(OpenApoc_Framework PUBLIC
		${OPENGLES_LIBRARY} ${UIKIT_LIBRARY} ${FOUNDATION_LIBRARY} ${UTTYPES_LIBRARY})
elseif(APPLE)
	find_library(COCOA_LIBRARY Cocoa REQUIRED)
	target_link_libraries(OpenApoc_Framework PUBLIC ${COCOA_LIBRARY})
	find_package(OpenGL)
	if (NOT OPENGL_FOUND)
		message(WARNING "OpenGL not found for GL_2_0 renderer")
	else()
		target_link_libraries(OpenApoc_Framework PUBLIC ${OPENGL_gl_LIBRARY})
		target_include_directories(OpenApoc_Framework PUBLIC ${OPENGL_INCLUDE_DIR})
	endif()
else()
	find_package(OpenGL)
	if (NOT OPENGL_FOUND)
		message(WARNING "OpenGL not found for GL_2_0 renderer")
	else()
		target_link_libraries(OpenApoc_Framework PUBLIC ${OPENGL_gl_LIBRARY})
		target_link_libraries(OpenApoc_Framework PUBLIC ${OPENGL_glx_LIBRARY})
		target_include_directories(OpenApoc_Framework PUBLIC ${OPENGL_INCLUDE_DIR})
	endif()
endif()
target_link_libraries(OpenApoc_Framework PUBLIC OpenApoc_Library)
target_link_libraries(OpenApoc_Framework PUBLIC OpenApoc_LibSmacker)
target_link_libraries(OpenApoc_Framework PUBLIC OpenApoc_LibPugixml)
target_link_libraries(OpenApoc_Framework PUBLIC OpenApoc_LibLodepng)
target_link_libraries(OpenApoc_Framework PUBLIC OpenApoc_Miniz)
target_link_libraries(OpenApoc_Framework PUBLIC ${CMAKE_DL_LIBS})

if(USE_SYSTEM_PHYSFS)
	target_link_libraries(OpenApoc_Framework PUBLIC ${PHYSFS_LIBRARY})
else()
	target_link_libraries(OpenApoc_Framework PUBLIC physfs-static)
endif()

target_include_directories(OpenApoc_Framework PRIVATE
		${CMAKE_SOURCE_DIR}/dependencies/physfs/src)
target_include_directories(OpenApoc_Framework PUBLIC ${CMAKE_SOURCE_DIR})

if (USE_PCH)
	target_precompile_headers(OpenApoc_Framework PUBLIC framework_pch.h)
endif()