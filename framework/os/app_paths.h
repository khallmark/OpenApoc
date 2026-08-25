#pragma once

#include "library/strings.h"

namespace OpenApoc
{

// Relative option defaults from options.cpp. Bundle launch replaces these at runtime.
inline constexpr const char *kDefaultDataPath = "./data";
inline constexpr const char *kDefaultCdPath = "./data/cd.iso";
inline constexpr const char *kDefaultSaveDir = "./saves";
inline constexpr const char *kDefaultModPath = "./data/mods";

bool isPortableMode();
bool pathLooksLikeAppBundle(const UString &path);
bool runningFromAppBundle(const UString &programPath);
UString sdlBasePath();
UString physfsPrefDir();
UString joinDir(const UString &dir, const UString &name);
bool isRelativeDefaultPath(const UString &value, const char *defaultValue);

// True if path is a CD ISO file or an extracted CD directory (music/ or xcom3/).
bool cdPathLooksValid(const UString &path);

// Apply bundle Resources + Application Support defaults when the current values
// are still the cwd-relative defaults. Does not persist bundle-internal Data/ModPath.
void applyAppBundlePathDefaults(const UString &programPath);

// Before writing settings.conf, restore Data/ModPath that point inside the .app.
void revertBundleInternalPathsForSave();

} // namespace OpenApoc
