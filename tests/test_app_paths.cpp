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
	    {"join dir", testJoinDir},
	    {"bundle path detect", testBundlePathDetect},
	    {"cd path valid", testCdPathLooksValid},
	});
}
