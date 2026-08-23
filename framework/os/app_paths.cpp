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
	if (!base.empty() && isRelativeDefaultPath(Options::modPath.get(), kDefaultModPath))
	{
		Options::modPath.set(joinDir(joinDir(base, "data"), "mods"));
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
}

} // namespace OpenApoc
