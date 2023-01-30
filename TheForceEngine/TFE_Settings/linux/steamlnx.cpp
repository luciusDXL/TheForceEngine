#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/fileutil.h>
#include <cstring>
#include <iostream>
#include <string>
#include "vdf_parser.hpp"

#define LINUX_STEAM_ROOT	"/.steam/root"

namespace LinuxSteam
{
	static bool findSteamBase(std::string& path)
	{
		path = std::getenv("HOME");
		if (path.length() < 1)
			return false;
		path += LINUX_STEAM_ROOT;
		return true;
	}

	static bool steamPathFromLibrary(const std::string& appid, const std::string& libpath, std::string& out)
	{
		std::string pbase, p;
		std::ifstream f;

		// read the app manifest to find the installed path
		pbase = libpath + "/steamapps";
		p = pbase + "/appmanifest_" + appid + ".acf";
		f.open(p);
		if (!f.is_open()) {
			// in the early days, it was CamelCase
			pbase = libpath + "/SteamApps";
			p = pbase + "/appmanifest_" + appid + ".acf";
			f.open(p);
			if (!f.is_open())
				return false;
		}
		auto acf = tyti::vdf::read(f);
		if (acf.name != "AppState")
			return false;
		std::string& dir = acf.attribs["installdir"];
		if (dir.length() < 1)
			return false;
		out = pbase + "/common/" + dir + "/";
		return true;
	}

	static bool findSteamAppIdPath(const std::string& appid, const std::string& steambasepath, std::string& out)
	{
		std::string lf;
		std::ifstream f;

		out.clear();
		lf = steambasepath + "/config/libraryfolders.vdf";
		f.open(lf);
		if (!f.is_open())
			return false;

		auto vdf_lf = tyti::vdf::read(f);
		if (vdf_lf.name != "libraryfolders")
			return false;

		auto i1 = vdf_lf.childs.begin();
		for (; i1 != vdf_lf.childs.end(); i1++) {
			std::string& libpath = i1->second->attribs["path"];
			if (libpath.length() < 1)
				continue;
			auto i2 = i1->second->childs.begin();
			for (; i2 != i1->second->childs.end(); i2++) {
				if (i2->first != "apps")
					continue;
				std::string& tstamp = i2->second->attribs[appid];
				if (tstamp.length() > 0)
					return steamPathFromLibrary(appid, libpath, out);
			}
		}
		return false;
	}

	// try to find the AppId installed somewhere in steam libraries.
	bool getSteamPath(u32 aid, const char *lsp, const char *testfile, char* outPath)
	{
		std::string out, base;
		bool found = findSteamBase(base);
		if (!found)
			return false;
		found = findSteamAppIdPath(std::to_string(aid), base, out);
		if (!found)
			return false;

		out += lsp;
		std::string tf = out + testfile;
		if (FileUtil::exists(tf.c_str())) {
			strncpy(outPath, out.c_str(), out.length());
			return true;
		}
		return false;
	}
}
