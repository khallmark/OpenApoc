#include "framework/os/app_paths.h"
#include "framework/configfile.h"
#include "framework/fs.h"
#include "framework/logger.h"
#include "framework/options.h"
#include "framework/filesystem.h"
#include <SDL.h>
#include <fstream>
#include <physfs.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace OpenApoc
{

bool isPortableMode()
{
	std::ifstream portableFile("./portable.txt");
	return static_cast<bool>(portableFile);
}

bool pathLooksLikeAppBundle(const UString &path)
{
	if (path.find(".app/Contents/") != UString::npos)
	{
		return true;
	}
	if (path.find(".app/") != UString::npos)
	{
		return true;
	}
	return path.size() >= 4 && ends_with(path, ".app");
}

bool runningFromAppBundle(const UString &programPath)
{
#ifdef __APPLE__
#if TARGET_OS_IPHONE
	(void)programPath;
	return true;
#else
	if (pathLooksLikeAppBundle(programPath))
	{
		return true;
	}
	const UString base = sdlBasePath();
	return pathLooksLikeAppBundle(base) || base.find("/Contents/Resources") != UString::npos;
#endif
#else
	(void)programPath;
	return false;
#endif
}

UString sdlBasePath()
{
	char *base = SDL_GetBasePath();
	if (!base)
	{
		return "";
	}
	UString path(base);
	SDL_free(base);
	return path;
}

UString physfsPrefDir()
{
	if (!PHYSFS_isInit())
	{
		PHYSFS_init(PROGRAM_NAME);
	}
	const char *pref = PHYSFS_getPrefDir(PROGRAM_ORGANISATION, PROGRAM_NAME);
	if (!pref)
	{
		return "";
	}
	return pref;
}

UString joinDir(const UString &dir, const UString &name)
{
	if (dir.empty())
	{
		return name;
	}
	fs::path p(dir);
	p /= name;
	return p.string();
}

bool isRelativeDefaultPath(const UString &value, const char *defaultValue)
{
	return value == defaultValue;
}

bool cdPathLooksValid(const UString &path)
{
	if (path.empty())
	{
		return false;
	}
	std::error_code ec;
	fs::path p(path);
	if (fs::is_regular_file(p, ec))
	{
		return true;
	}
	if (!fs::is_directory(p, ec))
	{
		return false;
	}
	return fs::exists(p / "music", ec) || fs::exists(p / "MUSIC", ec) ||
	       fs::exists(p / "xcom3", ec) || fs::exists(p / "XCOM3", ec);
}

static bool pathIsInsideAppBundle(const UString &path)
{
	return pathLooksLikeAppBundle(path);
}

void applyAppBundlePathDefaults(const UString &programPath)
{
	if (isPortableMode())
	{
		LogInfo("portable.txt present; leaving cwd-relative data paths");
		return;
	}
	if (!runningFromAppBundle(programPath))
	{
		return;
	}

	const UString base = sdlBasePath();
	const UString pref = physfsPrefDir();
	LogInfo("App bundle paths: base \"{0}\" pref \"{1}\"", base, pref);

	if (!base.empty() && isRelativeDefaultPath(Options::dataPathOption.get(), kDefaultDataPath))
	{
		Options::dataPathOption.set(joinDir(base, "data"));
	}
	if (!pref.empty() && isRelativeDefaultPath(Options::saveDirOption.get(), kDefaultSaveDir))
	{
		Options::saveDirOption.set(joinDir(pref, "saves"));
	}
	if (!pref.empty() && isRelativeDefaultPath(Options::cdPathOption.get(), kDefaultCdPath))
	{
		Options::cdPathOption.set(joinDir(pref, "cd.iso"));
	}
}

bool dataPathLooksValid(const UString &path)
{
	if (path.empty())
	{
		return false;
	}
	std::error_code ec;
	fs::path p(path);
	if (!fs::is_directory(p, ec))
	{
		return false;
	}
	// A directory that exists but holds none of these is a leftover, not a data directory.
	return fs::exists(p / "mods", ec) || fs::exists(p / "fonts", ec) ||
	       fs::exists(p / "city", ec) || fs::exists(p / "forms", ec);
}

// settings.conf keeps whatever Framework.Data was last used, and that path can simply stop
// existing -- a deleted checkout or worktree is the common way. Nothing validated it, so the
// game carried on with a dead path: every asset load failed one by one and the first visible
// symptom was an unrelated-looking mod error. Fall back to the data that shipped with the
// binary instead, and say so.
void applyDataPathFallback()
{
	const UString configured = Options::dataPathOption.get();
	if (dataPathLooksValid(configured))
	{
		return;
	}
	const UString base = sdlBasePath();
	const UString bundled = base.empty() ? UString() : joinDir(base, "data");
	if (dataPathLooksValid(bundled))
	{
		LogWarning("Data path \"{0}\" is missing; falling back to \"{1}\"", configured, bundled);
		Options::dataPathOption.set(bundled);
		return;
	}
	if (dataPathLooksValid(kDefaultDataPath))
	{
		LogWarning("Data path \"{0}\" is missing; falling back to \"{1}\"", configured,
		           kDefaultDataPath);
		Options::dataPathOption.set(kDefaultDataPath);
		return;
	}
	LogWarning("Data path \"{0}\" is missing and no fallback was found", configured);
}

UString resolveModPath(const UString &dataPath, const UString &currentModPath)
{
	// Anything other than the untouched default was resolved deliberately -- by the bundle
	// defaults above, by settings.conf, or by the command line -- so leave it alone.
	if (!isRelativeDefaultPath(currentModPath, kDefaultModPath))
	{
		return currentModPath;
	}
	if (dataPath.empty())
	{
		return currentModPath;
	}
	return joinDir(dataPath, "mods");
}

// Mods live inside the data directory, so the mod path has to follow it. It used to be
// derived only from the app bundle's own Resources, which meant that pointing
// --Framework.Data at a data directory elsewhere left ModPath behind at the cwd-relative
// "./data/mods". Portable mode made that the only resolution there is, so mods failed to
// load with a hard error while every other asset came from the directory that was asked for.
void applyDataRelativeModPath()
{
	// Deliberately keyed off the *value* rather than config().optionOverridden(): settings.conf
	// routinely carries a persisted "ModPath=./data/mods", which is the default written back
	// out, not a choice the user made. Treating that as an override is what left the mod path
	// pinned to the working directory while Data pointed somewhere else entirely.
	const UString resolved = resolveModPath(Options::dataPathOption.get(), Options::modPath.get());
	if (resolved != Options::modPath.get())
	{
		LogInfo("Mod path follows data path: \"{0}\"", resolved);
		Options::modPath.set(resolved);
	}
}

void revertBundleInternalPathsForSave()
{
	if (pathIsInsideAppBundle(Options::dataPathOption.get()))
	{
		Options::dataPathOption.set(kDefaultDataPath);
	}
	if (pathIsInsideAppBundle(Options::modPath.get()))
	{
		Options::modPath.set(kDefaultModPath);
	}
	// A mod path this run derived from the data path is not a choice the user made, and
	// persisting it would pin them to whatever data directory they happened to launch with.
	if (Options::modPath.get() == resolveModPath(Options::dataPathOption.get(), kDefaultModPath))
	{
		Options::modPath.set(kDefaultModPath);
	}
}

} // namespace OpenApoc
