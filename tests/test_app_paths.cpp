#include "framework/filesystem.h"
#include "framework/os/app_paths.h"
#include "test_helpers.h"
#include <fstream>

using namespace OpenApoc;

static bool testRelativeDefaults()
{
	TEST_REQUIRE(isRelativeDefaultPath("./data", kDefaultDataPath), "data default");
	TEST_REQUIRE(isRelativeDefaultPath("./data/cd.iso", kDefaultCdPath), "cd default");
	TEST_REQUIRE(!isRelativeDefaultPath("/tmp/data", kDefaultDataPath), "absolute is not default");
	return true;
}

static bool testResolveModPath()
{
	// The untouched default follows whatever data path was resolved -- this is the case that
	// used to break: mods stayed at "./data/mods" while every other asset came from --Framework.Data.
	const UString followed = resolveModPath("/opt/game/data", kDefaultModPath);
	TEST_REQUIRE(followed.find("/opt/game/data") != UString::npos, "mod path follows data path");
	TEST_REQUIRE(followed.find("mods") != UString::npos, "mod path ends in mods");
	// A path already resolved elsewhere (bundle defaults, settings.conf, command line) is kept.
	TEST_REQUIRE(resolveModPath("/opt/game/data", "/elsewhere/mods") == "/elsewhere/mods",
	             "explicit mod path is untouched");
	// No data path to derive from means no change rather than a bare "mods".
	TEST_REQUIRE(resolveModPath("", kDefaultModPath) == kDefaultModPath, "empty data path");
	// The plain relative case must keep behaving exactly as it did.
	TEST_REQUIRE(resolveModPath(kDefaultDataPath, kDefaultModPath).find("mods") != UString::npos,
	             "relative default still resolves");
	return true;
}

static bool testDataPathLooksValid()
{
	const fs::path tmp = fs::temp_directory_path() / "openapoc-data-test";
	fs::remove_all(tmp);
	// The case that used to go unnoticed: settings.conf naming a checkout that has been deleted.
	TEST_REQUIRE(!dataPathLooksValid(tmp.string()), "missing path");
	fs::create_directories(tmp);
	TEST_REQUIRE(!dataPathLooksValid(tmp.string()), "empty directory is not data");
	fs::create_directories(tmp / "mods");
	TEST_REQUIRE(dataPathLooksValid(tmp.string()), "directory holding mods");
	fs::remove_all(tmp);
	TEST_REQUIRE(!dataPathLooksValid(""), "empty string");
	return true;
}

static bool testJoinDir()
{
	const UString joined = joinDir("/tmp/pref/", "saves");
	TEST_REQUIRE(joined.find("saves") != UString::npos, "join keeps name");
	TEST_REQUIRE(joinDir("", "x") == "x", "empty dir");
	return true;
}

static bool testBundlePathDetect()
{
	TEST_REQUIRE(pathLooksLikeAppBundle("/build/bin/OpenApoc.app/Contents/MacOS/OpenApoc"),
	             "mac exe");
	TEST_REQUIRE(pathLooksLikeAppBundle("/var/containers/Bundle/Application/id/OpenApoc.app/"),
	             "ios bundle");
	TEST_REQUIRE(!pathLooksLikeAppBundle("/Users/me/OpenApoc/build/bin/OpenApoc"), "raw exe");
	return true;
}

static bool testCdPathLooksValid()
{
	const fs::path tmp = fs::temp_directory_path() / "openapoc-cd-test";
	fs::remove_all(tmp);
	TEST_REQUIRE(!cdPathLooksValid(tmp.string()), "missing path");
	fs::create_directories(tmp / "xcom3");
	TEST_REQUIRE(cdPathLooksValid(tmp.string()), "extracted tree");
	const fs::path iso = tmp / "dummy.iso";
	{ std::ofstream out(iso.string()); out << "x"; }
	TEST_REQUIRE(cdPathLooksValid(iso.string()), "iso file");
	fs::remove_all(tmp);
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	return TestHelpers::runTestSuite({
	    {"relative defaults", testRelativeDefaults},
	    {"resolve mod path", testResolveModPath},
	    {"data path valid", testDataPathLooksValid},
	    {"join dir", testJoinDir},
	    {"bundle path detect", testBundlePathDetect},
	    {"cd path valid", testCdPathLooksValid},
	});
}
