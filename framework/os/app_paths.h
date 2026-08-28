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

// True if path is a directory that actually holds OpenApoc data.
bool dataPathLooksValid(const UString &path);

// Fall back to the shipped data directory when the configured Framework.Data no longer exists.
void applyDataPathFallback();

// Apply bundle Resources + Application Support defaults when the current values
// are still the cwd-relative defaults. Does not persist bundle-internal Data/ModPath.
void applyAppBundlePathDefaults(const UString &programPath);

// The mod path that should be used for a given resolved data path. An untouched default
// follows the data path; anything else is returned unchanged.
UString resolveModPath(const UString &dataPath, const UString &currentModPath);

// Point Game.ModPath at <Framework.Data>/mods unless the user set it explicitly.
void applyDataRelativeModPath();

// Before writing settings.conf, restore Data/ModPath that point inside the .app.
void revertBundleInternalPathsForSave();

} // namespace OpenApoc
