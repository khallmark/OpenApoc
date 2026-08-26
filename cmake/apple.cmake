# Shared Apple bundle helpers (macOS + iOS).

set(APPLE_BUNDLE_ID "org.openapoc.OpenApoc" CACHE STRING "CFBundleIdentifier")
set(APPLE_TEAM_ID "" CACHE STRING "Apple Development / Developer ID team")
set(APPLE_CODESIGN_IDENTITY "" CACHE STRING
	"codesign identity (Developer ID Application or Apple Development)")
set(APPLE_ICON_NAME "OpenApocIcon" CACHE STRING
	"Icon Composer bundle in cmake/apple/<name>.icon used as the app icon")
option(APPLE_REQUIRE_APP_ICON
	"Fail the build when the app icon cannot be compiled" OFF)

# Simulator builds differ from device builds in two places: actool needs a
# different --platform, and code signing is ad-hoc rather than provisioned.
set(APPLE_IOS_SIMULATOR FALSE)
if(IOS AND CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
	set(APPLE_IOS_SIMULATOR TRUE)
endif()

if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
	if(IOS)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "16.0" CACHE STRING "" FORCE)
	else()
		set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "" FORCE)
	endif()
endif()

# actool ships with a full Xcode; a Command Line Tools developer directory has
# no asset compiler, so xcrun cannot find it there.
function(_openapoc_find_actool dest_var)
	if(APPLE_ACTOOL)
		set(${dest_var} "${APPLE_ACTOOL}" PARENT_SCOPE)
		return()
	endif()
	execute_process(COMMAND xcrun --find actool
		OUTPUT_VARIABLE _actool OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET RESULT_VARIABLE _result)
	if(NOT _result EQUAL 0)
		set(_actool "")
		foreach(_dev "$ENV{DEVELOPER_DIR}" "/Applications/Xcode.app/Contents/Developer")
			if(_dev AND EXISTS "${_dev}/usr/bin/actool")
				set(_actool "${_dev}/usr/bin/actool")
				break()
			endif()
		endforeach()
	endif()
	set(APPLE_ACTOOL "${_actool}" CACHE FILEPATH "Xcode asset catalog compiler")
	set(${dest_var} "${_actool}" PARENT_SCOPE)
endfunction()

# Compile cmake/apple/${APPLE_ICON_NAME}.icon into the app bundle. This is a
# POST_BUILD step rather than a generated source so the destination is explicit
# on both a flat iOS bundle and a macOS Contents/Resources one; the script
# itself skips the work when the icon has not changed. Call it before
# openapoc_sign_mac_app so the icon is in place when the bundle is signed.
function(openapoc_add_app_icon target)
	set(_icon "${CMAKE_SOURCE_DIR}/cmake/apple/${APPLE_ICON_NAME}.icon")
	if(NOT IS_DIRECTORY "${_icon}")
		message(WARNING "No ${_icon}; ${target} will ship without an app icon")
		return()
	endif()
	_openapoc_find_actool(_actool)
	if(NOT _actool)
		if(APPLE_REQUIRE_APP_ICON)
			message(FATAL_ERROR "actool not found; a full Xcode is needed to compile ${APPLE_ICON_NAME}.icon")
		endif()
		message(WARNING "actool not found (needs a full Xcode, not just the Command "
			"Line Tools); ${target} will ship without an app icon")
		return()
	endif()
	if(IOS)
		set(_platform "iphoneos")
		if(APPLE_IOS_SIMULATOR)
			set(_platform "iphonesimulator")
		endif()
		set(_devices "iphone,ipad")
		set(_dest "$<TARGET_BUNDLE_DIR:${target}>")
	else()
		set(_platform "macosx")
		set(_devices "mac")
		set(_dest "$<TARGET_BUNDLE_DIR:${target}>/Contents/Resources")
	endif()
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}"
			-D "ACTOOL=${_actool}"
			-D "ICON=${_icon}"
			-D "ICON_NAME=${APPLE_ICON_NAME}"
			-D "PLATFORM=${_platform}"
			-D "DEVICES=${_devices}"
			-D "MIN_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
			-D "STAGE=${CMAKE_BINARY_DIR}/apple-icon/${_platform}"
			-D "DEST=${_dest}"
			-D "REQUIRED=${APPLE_REQUIRE_APP_ICON}"
			-P "${CMAKE_SOURCE_DIR}/cmake/apple/compile_app_icon.cmake"
		COMMENT "Compiling ${APPLE_ICON_NAME}.icon into ${target}")
	# POST_BUILD only fires when the target itself is rebuilt, so editing the art
	# alone would leave the old icon in the bundle. Make the icon a link
	# dependency: touching it relinks, which reruns the step above. The Xcode
	# generator ignores LINK_DEPENDS but runs post-build rules on every build.
	file(GLOB_RECURSE _icon_files CONFIGURE_DEPENDS "${_icon}/*")
	set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${_icon_files})
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
			XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
			XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"
			XCODE_ATTRIBUTE_INFOPLIST_KEY_UILaunchStoryboardName "LaunchScreen"
			XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED YES)
		if(APPLE_IOS_SIMULATOR)
			# The simulator has no provisioning; ad-hoc signing is what Xcode
			# itself uses there, and a real identity would fail the build.
			set_target_properties(${target} PROPERTIES
				XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
				XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO)
		else()
			set_target_properties(${target} PROPERTIES
				XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development")
		endif()
		set(_story "${CMAKE_SOURCE_DIR}/cmake/ios/LaunchScreen.storyboard")
		target_sources(${target} PRIVATE "${_story}")
		set_source_files_properties("${_story}" PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources")
	else()
		set_target_properties(${target} PROPERTIES
			MACOSX_BUNDLE_ICON_FILE "${APPLE_ICON_NAME}"
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
