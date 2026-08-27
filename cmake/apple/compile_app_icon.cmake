# Compile an Icon Composer .icon bundle into an already-built app bundle.
# Run via `cmake -P` from a POST_BUILD step; see openapoc_add_app_icon().
#
# actool is the only thing that reads .icon, and it ships with a full Xcode.
# Where it cannot run -- a Command Line Tools-only install, or an iOS build on
# a machine with no simulator runtime -- warn and leave the bundle without an
# icon rather than failing the build, unless REQUIRED is set.
#
# Expects: ACTOOL ICON ICON_NAME PLATFORM DEVICES MIN_TARGET STAGE DEST REQUIRED

cmake_minimum_required(VERSION 3.30)

function(openapoc_compile_app_icon)
	if(NOT STAGE OR NOT DEST OR NOT ICON_NAME)
		message(FATAL_ERROR "compile_app_icon.cmake: STAGE, DEST and ICON_NAME are required")
	endif()

	set(_car "${DEST}/Assets.car")
	set(_icns "${DEST}/${ICON_NAME}.icns")

	# actool is slow enough to be worth skipping on an unchanged icon, and this
	# runs on every build. Glob the bundle rather than trusting its mtime: the
	# directory does not change when a layer image inside it does.
	file(GLOB_RECURSE _inputs "${ICON}/*")
	list(APPEND _inputs "${CMAKE_CURRENT_LIST_FILE}")

	set(_stale FALSE)
	if(NOT EXISTS "${_car}")
		set(_stale TRUE)
	elseif(PLATFORM STREQUAL "macosx" AND NOT EXISTS "${_icns}")
		set(_stale TRUE)
	else()
		foreach(_in IN LISTS _inputs)
			# IS_NEWER_THAN is also true for equal timestamps, so a tie
			# recompiles instead of silently keeping a stale icon.
			if("${_in}" IS_NEWER_THAN "${_car}")
				set(_stale TRUE)
				break()
			endif()
		endforeach()
	endif()
	if(NOT _stale)
		return()
	endif()

	set(_device_args)
	string(REPLACE "," ";" _devices "${DEVICES}")
	foreach(_device IN LISTS _devices)
		list(APPEND _device_args --target-device "${_device}")
	endforeach()

	file(REMOVE_RECURSE "${STAGE}")
	file(MAKE_DIRECTORY "${STAGE}")

	# --output-partial-info-plist is mandatory for app icons: without it actool
	# refuses to compile. The keys it emits are already in our Info.plist.in, so
	# the file itself is only kept for debugging.
	execute_process(
		COMMAND "${ACTOOL}"
			--output-format human-readable-text
			--notices --warnings
			--app-icon "${ICON_NAME}"
			--output-partial-info-plist "${STAGE}/icon-partial.plist"
			--development-region en
			${_device_args}
			--minimum-deployment-target "${MIN_TARGET}"
			--platform "${PLATFORM}"
			--compile "${STAGE}"
			"${ICON}"
		RESULT_VARIABLE _result
		OUTPUT_VARIABLE _stdout
		ERROR_VARIABLE _stderr)

	if(NOT _result EQUAL 0 OR NOT EXISTS "${STAGE}/Assets.car")
		set(_report "actool could not compile ${ICON_NAME}.icon for ${PLATFORM}:\n${_stdout}${_stderr}")
		if(REQUIRED)
			message(FATAL_ERROR "${_report}")
		endif()
		message(WARNING "${_report}\
The bundle will ship without an app icon. Compiling icons for iOS also needs an \
installed simulator runtime: xcodebuild -downloadPlatform iOS")
		return()
	endif()

	file(MAKE_DIRECTORY "${DEST}")
	file(COPY "${STAGE}/Assets.car" DESTINATION "${DEST}")
	# actool emits an .icns for macOS only; iOS reads the icon out of Assets.car.
	if(EXISTS "${STAGE}/${ICON_NAME}.icns")
		file(COPY "${STAGE}/${ICON_NAME}.icns" DESTINATION "${DEST}")
	endif()
	message(STATUS "Compiled ${ICON_NAME}.icon into ${DEST}")
endfunction()

openapoc_compile_app_icon()
