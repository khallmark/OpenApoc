set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME iOS)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_OSX_DEPLOYMENT_TARGET "16.0")
set(VCPKG_OSX_SYSROOT iphonesimulator)

# vcpkg maps any Apple target to "<arch>-apple-darwin", which equals the host's
# own build triplet, so autoconf decides it is not cross-compiling and tries to
# run iOS test binaries on macOS (libiconv, via boost-locale, dies there).
# Naming the real target restores cross-compile mode.
set(VCPKG_MAKE_BUILD_TRIPLET "--host=aarch64-apple-ios")
