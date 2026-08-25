# Shared Apple bundle helpers (macOS + iOS).

set(APPLE_BUNDLE_ID "org.openapoc.OpenApoc" CACHE STRING "CFBundleIdentifier")
set(APPLE_TEAM_ID "" CACHE STRING "Apple Development / Developer ID team")
set(APPLE_CODESIGN_IDENTITY "" CACHE STRING
	"codesign identity (Developer ID Application or Apple Development)")

if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
	if(IOS)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "16.0" CACHE STRING "" FORCE)
	else()
		set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "" FORCE)
	endif()
endif()

function(openapoc_generate_mac_icon dest_var)
	set(_icon_dir "${CMAKE_BINARY_DIR}/apple-icon")
	set(_icns "${_icon_dir}/OpenApoc.icns")
	find_package(Python3 COMPONENTS Interpreter)
	if(NOT Python3_Interpreter_FOUND)
		message(WARNING "Python3 not found; skipping OpenApoc.icns")
		set(${dest_var} "" PARENT_SCOPE)
		return()
	endif()
	add_custom_command(OUTPUT "${_icns}"
		COMMAND "${Python3_EXECUTABLE}"
			"${CMAKE_SOURCE_DIR}/cmake/macos/generate_icon.py" "${_icon_dir}"
		DEPENDS "${CMAKE_SOURCE_DIR}/cmake/macos/generate_icon.py"
		COMMENT "Generating OpenApoc.icns")
	add_custom_target(openapoc-icns DEPENDS "${_icns}")
	set(${dest_var} "${_icns}" PARENT_SCOPE)
endfunction()

function(openapoc_copy_app_data target)
	if(IOS)
		set(_dest "$<TARGET_BUNDLE_DIR:${target}>/data")
	else()
		set(_dest "$<TARGET_BUNDLE_DIR:${target}>/Contents/Resources/data")
	endif()
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}"
			-D "SRC=${CMAKE_SOURCE_DIR}/data"
			-D "DST=${_dest}"
			-P "${CMAKE_SOURCE_DIR}/cmake/apple/copy_bundle_data.cmake"
		COMMENT "Copying OpenApoc data into ${target} (excluding ISO)")
endfunction()

function(openapoc_configure_apple_app target)
	set(_plist "${CMAKE_SOURCE_DIR}/cmake/macos/Info.plist.in")
	if(IOS)
		set(_plist "${CMAKE_SOURCE_DIR}/cmake/ios/Info.plist.in")
	endif()
	set_target_properties(${target} PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_INFO_PLIST "${_plist}"
		MACOSX_BUNDLE_BUNDLE_NAME "OpenApoc"
		MACOSX_BUNDLE_GUI_IDENTIFIER "${APPLE_BUNDLE_ID}"
		MACOSX_BUNDLE_BUNDLE_VERSION "${OPENAPOC_VERSION_STRING}"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
		MACOSX_BUNDLE_COPYRIGHT "OpenApoc contributors"
		MACOSX_BUNDLE_INFO_STRING "OpenApoc ${OPENAPOC_VERSION_STRING}"
		XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${APPLE_BUNDLE_ID}"
		XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${APPLE_TEAM_ID}")
	if(IOS)
		set_target_properties(${target} PROPERTIES
			XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
			XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
			XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"
			XCODE_ATTRIBUTE_INFOPLIST_KEY_UILaunchStoryboardName "LaunchScreen"
			XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED YES)
		set(_story "${CMAKE_SOURCE_DIR}/cmake/ios/LaunchScreen.storyboard")
		target_sources(${target} PRIVATE "${_story}")
		set_source_files_properties("${_story}" PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources")
	else()
		set_target_properties(${target} PROPERTIES
			MACOSX_BUNDLE_ICON_FILE "OpenApoc"
			XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
		set_target_properties(${target} PROPERTIES
			BUILD_RPATH "@executable_path/../Frameworks"
			INSTALL_RPATH "@executable_path/../Frameworks")
		if(APPLE_CODESIGN_IDENTITY)
			set_target_properties(${target} PROPERTIES
				XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${APPLE_CODESIGN_IDENTITY}")
		endif()
	endif()
endfunction()

function(openapoc_sign_mac_app target)
	if(IOS OR NOT APPLE_CODESIGN_IDENTITY)
		return()
	endif()
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_SOURCE_DIR}/cmake/macos/sign.sh"
			"$<TARGET_BUNDLE_DIR:${target}>"
			"${APPLE_CODESIGN_IDENTITY}"
			"${CMAKE_SOURCE_DIR}/cmake/macos/OpenApoc.entitlements"
		COMMENT "codesign ${target} (inside-out, no --deep)")
endfunction()
