/*
 Copyright (c) 2013 yvt

 This file is part of OpenSpades.

 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.

 */

#include <algorithm> //std::sort
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <regex>
#include <utility>
#include <vector>

#if (!defined(__APPLE__) && (__unix || __unix__)) || defined(__HAIKU__)
#include <sys/stat.h>
#include <sys/types.h>
#endif
#ifndef WIN32
#include <unistd.h>
#endif

#include <Imports/SDL.h>
#include <zlib.h>

#include "Main.h"
#include "MainScreen.h"
#include "Runner.h"
#include "SplashWindow.h"
#include <Client/Client.h>
#include <Client/Fonts.h>
#include <Client/GameMap.h>
#include <Core/ConcurrentDispatch.h>
#include <Core/CpuID.h>
#include <Core/Debug.h>
#include <Core/DirectoryFileSystem.h>
#include <Core/FileManager.h>
#include <Core/ServerAddress.h>
#include <Core/Settings.h>
#include <Core/Strings.h>
#include <Core/Thread.h>
#include <Core/ZipFileSystem.h>
#include <Gui/ConsoleScreen.h>
#include <Gui/PackageUpdateManager.h>
#include <Gui/StartupScreen.h>
#include <OpenSpades.h>

#include <Core/VoxelModel.h>
#include <Draw/GLOptimizedVoxelModel.h>

#include <ScriptBindings/ScriptManager.h>

#include <Core/Bitmap.h>
#include <Core/MemoryStream.h>

#if _MSC_VER >= 1900 // Visual Studio 2015 or higher
extern "C" {
FILE __iob_func[3] = {*stdin, *stdout, *stderr};
}
#endif

DEFINE_SPADES_SETTING(cl_showStartupWindow, "1");
// Empty by default: archives in the per-user Resources directory are opt-in mods.
// cl_activeMod is retained only to migrate configurations written by the single-mod manager.
DEFINE_SPADES_SETTING(cl_activeMod, "");
DEFINE_SPADES_SETTING(cl_activeMods, "");

#ifdef WIN32
// windows.h must be included before DbgHelp.h and shlobj.h.
#include <windows.h>

#include <DbgHelp.h>
#include <shlobj.h>

#define strncasecmp(x, y, z) _strnicmp(x, y, z)
#define strcasecmp(x, y) _stricmp(x, y)

DEFINE_SPADES_SETTING(core_win32BeginPeriod, "1");

namespace {
	class ThreadQuantumSetter {
	public:
		ThreadQuantumSetter() {
			if (core_win32BeginPeriod) {
				timeBeginPeriod(1);
				SPLog("Thread quantum was modified to 1ms by timeBeginPeriod");
				SPLog("(to disable this behavior, set core_win32BeginPeriod to 0)");
			} else {
				SPLog("Thread quantum is not modified");
				SPLog("(to enable this behavior, set core_win32BeginPeriod to 1)");
			}
		}
		~ThreadQuantumSetter() {
			if (core_win32BeginPeriod) {
				timeEndPeriod(1);
				SPLog("Thread quantum was restored");
			}
		}
	};

	// lm: without doing it this way, we will get a low-res icon or an ugly resampled icon in our
	// window.
	// we cannot use the fltk function on the console window, because it's not an Fl_Window...
	void setIcon(HWND hWnd) {
		HINSTANCE hInstance = GetModuleHandle(NULL);
		HICON hIcon =
		  (HICON)LoadImageA(hInstance, "AppIcon", IMAGE_ICON, GetSystemMetrics(SM_CXICON),
		                    GetSystemMetrics(SM_CYICON), 0);
		if (hIcon) {
			SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		}
		hIcon = (HICON)LoadImageA(hInstance, "AppIcon", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
		                          GetSystemMetrics(SM_CYSMICON), 0);
		if (hIcon) {
			SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}
	}

	LONG WINAPI UnhandledExceptionProc(LPEXCEPTION_POINTERS lpEx) {
		typedef BOOL(WINAPI * PDUMPFN)(HANDLE hProcess, DWORD ProcessId, HANDLE hFile,
		                               MINIDUMP_TYPE DumpType,
		                               PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
		                               PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
		                               PMINIDUMP_CALLBACK_INFORMATION CallbackParam);
		HMODULE hLib = LoadLibrary("DbgHelp.dll");
		PDUMPFN pMiniDumpWriteDump = (PDUMPFN)GetProcAddress(hLib, "MiniDumpWriteDump");

		static char buf[MAX_PATH + 120] = {0}; // this is our display buffer.
		if (pMiniDumpWriteDump) {
			static char fullBuf[MAX_PATH + 120] = {0};
			if (SUCCEEDED(
			      SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0,
			                      buf))) { // max length = MAX_PATH (temp abuse this buffer space)
				strcat_s(buf, "\\");       // ensure we end with a slash.
			} else {
				buf[0] = 0; // empty it, the file will now end up in the working directory :(
			}
			sprintf(fullBuf, "%sOpenSpadesCrash%d.dmp", buf,
			        GetTickCount()); // some sort of randomization.
			HANDLE hFile = CreateFile(fullBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			                          FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE) {
				MINIDUMP_EXCEPTION_INFORMATION mdei = {0};
				mdei.ThreadId = GetCurrentThreadId();
				mdei.ExceptionPointers = lpEx;
				mdei.ClientPointers = TRUE;
				MINIDUMP_TYPE mdt = MiniDumpNormal;
				BOOL rv = pMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, mdt,
				                             (lpEx != 0) ? &mdei : 0, 0, 0);
				CloseHandle(hFile);
				sprintf_s(buf,
				          "Something went horribly wrong, please send the file \n%s\nfor analysis.",
				          fullBuf);
			} else {
				sprintf_s(buf,
				          "Something went horribly wrong,\ni even failed to store information "
				          "about the problem... (0x%08x)",
				          lpEx ? lpEx->ExceptionRecord->ExceptionCode : 0xffffffff);
			}
		} else {
			sprintf_s(buf,
			          "Something went horribly wrong,\ni even failed to retrieve information "
			          "about the problem... (0x%08x)",
			          lpEx ? lpEx->ExceptionRecord->ExceptionCode : 0xffffffff);
		}
		MessageBoxA(NULL, buf, "Oops, we crashed...", MB_OK | MB_ICONERROR);
		ExitProcess(-1);
		// return EXCEPTION_EXECUTE_HANDLER;
	}
} // namespace
#else
namespace {
	class ThreadQuantumSetter {};
} // namespace
#endif

namespace {
	bool g_autoconnect = false;
	std::string g_autoconnectHostName;
	spades::ProtocolVersion g_autoconnectProtocolVersion = spades::ProtocolVersion::v075;

	bool g_printVersion = false;
	bool g_printHelp = false;
	bool g_modRestartLaunch = false;
	bool g_restartRequested = false;

	void printHelp(char *binaryName) {
		printf("usage: %s [server_address] [v=protocol_version] [-h|--help] [-v|--version] \n",
		       binaryName);
	}

	int handleCommandLineArgument(int argc, char **argv, int &i) {
		if (char *a = argv[i]) {
			static std::regex hostNameRegex{"aos://.*"};
			static std::regex v075Regex{"(?:v=)?0?\\.?75"};
			static std::regex v076Regex{"(?:v=)?0?\\.?76"};

			if (std::regex_match(a, hostNameRegex)) {
				g_autoconnect = true;
				g_autoconnectHostName = a;
				return ++i;
			}
			if (std::regex_match(a, v075Regex)) {
				g_autoconnectProtocolVersion = spades::ProtocolVersion::v075;
				return ++i;
			}
			if (std::regex_match(a, v076Regex)) {
				g_autoconnectProtocolVersion = spades::ProtocolVersion::v076;
				return ++i;
			}
			if (!strcasecmp(a, "--version") || !strcasecmp(a, "-v")) {
				g_printVersion = true;
				return ++i;
			}
			if (!strcasecmp(a, "--help") || !strcasecmp(a, "-h")) {
				g_printHelp = true;
				return ++i;
			}
			if (!strcasecmp(a, "--mod-restart")) {
				g_modRestartLaunch = true;
				return ++i;
			}
		}

		return 0;
	}

	bool RelaunchApplication(int argc, char **argv, std::string &error) {
#ifdef WIN32
		(void)argc;
		(void)argv;
		std::vector<wchar_t> executablePath(32768);
		DWORD executablePathCapacity = static_cast<DWORD>(executablePath.size());
		DWORD executablePathLength =
		  GetModuleFileNameW(nullptr, executablePath.data(), executablePathCapacity);
		if (executablePathLength == 0 || executablePathLength >= executablePathCapacity) {
			error = spades::Format("GetModuleFileNameW failed with error {0}", GetLastError());
			return false;
		}

		std::wstring commandLine(GetCommandLineW());
		if (!g_modRestartLaunch)
			commandLine += L" --mod-restart";

		std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
		mutableCommandLine.push_back(L'\0');

		STARTUPINFOW startupInfo = {};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo = {};
		if (!CreateProcessW(executablePath.data(), mutableCommandLine.data(), nullptr, nullptr, FALSE,
		                    0, nullptr, nullptr, &startupInfo, &processInfo)) {
			error = spades::Format("CreateProcessW failed with error {0}", GetLastError());
			return false;
		}

		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return true;
#else
		std::vector<char *> launchArguments;
		launchArguments.reserve(static_cast<size_t>(argc) + 2);
		for (int i = 0; i < argc; ++i)
			launchArguments.push_back(argv[i]);
		char restartArgument[] = "--mod-restart";
		if (!g_modRestartLaunch)
			launchArguments.push_back(restartArgument);
		launchArguments.push_back(nullptr);

		fflush(nullptr);
		execvp(launchArguments[0], launchArguments.data());
		error = spades::Format("execvp failed: {0}", std::string(std::strerror(errno)));
		return false;
#endif
	}
} // namespace

namespace {
	spades::DirectoryFileSystem *g_userResourceFileSystem = nullptr;
	std::vector<std::string> g_loadedUserMods;

	bool IsPackageArchiveName(const std::string &name) {
		if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
			return false;
		std::string lower = name;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return lower.size() > 4 &&
		       (lower.compare(lower.size() - 4, 4, ".pak") == 0 ||
		        lower.compare(lower.size() - 4, 4, ".zip") == 0 ||
		        lower.compare(lower.size() - 4, 4, ".pzk") == 0);
	}
}

namespace spades {
	std::string g_userResourceDirectory;

	void RequestApplicationRestart() {
		g_restartRequested = true;
		SPLog("A clean restart was requested to apply the user mod selection");
	}

	std::vector<std::string> GetAvailableUserMods() {
		std::vector<std::string> mods;
		if (g_userResourceDirectory.empty())
			return mods;

		DirectoryFileSystem userFiles(g_userResourceDirectory, false);
		for (const std::string &name : userFiles.EnumFiles("")) {
			if (IsPackageArchiveName(name) && userFiles.FileExists(name.c_str()))
				mods.push_back(name);
		}
		std::sort(mods.begin(), mods.end(), [](const std::string &a, const std::string &b) {
			return std::lexicographical_compare(
			  a.begin(), a.end(), b.begin(), b.end(), [](unsigned char x, unsigned char y) {
				  return std::tolower(x) < std::tolower(y);
			  });
		});
		return mods;
	}

	namespace {
		std::string ValidateUserModName(const std::string &name) {
			const std::vector<std::string> mods = GetAvailableUserMods();
			if (std::find(mods.begin(), mods.end(), name) == mods.end())
				return "The selected mod is no longer present in the user Resources directory.";
			return std::string();
		}

		std::unique_ptr<ZipFileSystem> OpenUserModArchive(const std::string &name) {
			DirectoryFileSystem userFiles(g_userResourceDirectory, false);
			std::unique_ptr<IStream> stream(userFiles.OpenForReading(name.c_str()));
			std::unique_ptr<ZipFileSystem> fileSystem(new ZipFileSystem(stream.get()));
			stream.release(); // ZipFileSystem owns it after successful construction.
			return fileSystem;
		}
	} // namespace

	namespace {
		std::string SerializeUserMods(const std::vector<std::string> &mods) {
			std::string value;
			for (const std::string &name : mods) {
				value += std::to_string(name.size());
				value += ':';
				value += name;
			}
			return value;
		}

		bool DeserializeUserMods(const std::string &value, std::vector<std::string> &mods) {
			mods.clear();
			size_t cursor = 0;
			while (cursor < value.size()) {
				size_t colon = value.find(':', cursor);
				if (colon == std::string::npos || colon == cursor)
					return false;

				size_t length = 0;
				for (size_t i = cursor; i < colon; ++i) {
					unsigned char c = static_cast<unsigned char>(value[i]);
					if (!std::isdigit(c))
						return false;
					size_t digit = static_cast<size_t>(c - '0');
					if (length > (std::numeric_limits<size_t>::max() - digit) / 10)
						return false;
					length = length * 10 + digit;
				}

				cursor = colon + 1;
				if (length == 0 || length > value.size() - cursor)
					return false;
				std::string name = value.substr(cursor, length);
				if (std::find(mods.begin(), mods.end(), name) == mods.end())
					mods.push_back(name);
				cursor += length;
			}
			return true;
		}

		void PersistActiveUserMods(const std::vector<std::string> &mods) {
			cl_activeMods = SerializeUserMods(mods);
			cl_activeMod = std::string();
			Settings::GetInstance()->Flush();
		}
	} // namespace

	std::vector<std::string> GetActiveUserMods() {
		std::string value = cl_activeMods;
		if (!value.empty()) {
			std::vector<std::string> mods;
			if (DeserializeUserMods(value, mods))
				return mods;
			return std::vector<std::string>();
		}

		// Transparently import the old single archive setting. It is rewritten in the new
		// length-prefixed format as soon as the selection changes or startup validates it.
		std::string legacyMod = cl_activeMod;
		if (!legacyMod.empty())
			return std::vector<std::string>(1, legacyMod);
		return std::vector<std::string>();
	}

	std::vector<std::string> GetLoadedUserMods() { return g_loadedUserMods; }

	std::string SetUserModEnabled(const std::string &name, bool enabled) {
		std::vector<std::string> mods = GetActiveUserMods();
		auto existing = std::find(mods.begin(), mods.end(), name);

		if (!enabled) {
			if (existing != mods.end())
				mods.erase(existing);
			PersistActiveUserMods(mods);
			SPLog("User mod disabled for next launch: %s", name.c_str());
			return std::string();
		}
		if (existing != mods.end())
			return std::string();

		std::string error = ValidateUserModName(name);
		if (!error.empty())
			return error;

		// Construct the filesystem now to reject corrupt or unsupported archives, but do not
		// change the live resource stack. Scripts, renderer/audio caches, and models can retain
		// resources from the old stack; replacing it in-process creates mixed assets and can
		// invalidate streams. Startup mounts every selection before any of those systems exist.
		try {
			OpenUserModArchive(name);
		} catch (const std::exception &ex) {
			return Format("Unable to load mod '{0}': {1}", name, ex.what());
		}

		// The most recently enabled archive has highest priority. This makes collisions
		// deterministic while still allowing disjoint archives to be combined.
		mods.insert(mods.begin(), name);
		PersistActiveUserMods(mods);
		SPLog("User mod enabled for next launch at highest priority: %s", name.c_str());
		return std::string();
	}

	std::string DisableAllUserMods() {
		PersistActiveUserMods(std::vector<std::string>());
		SPLog("All user mods will be disabled on the next launch");
		return std::string();
	}

	std::string MountUserModsForStartup(const std::vector<std::string> &names) {
		using PendingMod = std::pair<std::string, std::unique_ptr<ZipFileSystem>>;
		std::vector<PendingMod> pending;
		std::vector<std::string> validNames;
		std::string errors;

		// Open and validate every archive before changing the resource search path.
		for (const std::string &name : names) {
			std::string error = ValidateUserModName(name);
			if (error.empty()) {
				try {
					pending.emplace_back(name, OpenUserModArchive(name));
					validNames.push_back(name);
				} catch (const std::exception &ex) {
					error = Format("Unable to open archive: {0}", ex.what());
				}
			}
			if (!error.empty()) {
				if (!errors.empty())
					errors += "\n";
				errors += Format("{0}: {1}", name, error);
			}
		}

		// validNames is highest-to-lowest priority. Prepending in reverse preserves that
		// order ahead of the built-in resources, so the newest selection wins path conflicts.
		for (size_t i = pending.size(); i > 0; --i) {
			FileManager::PrependFileSystem(pending[i - 1].second.get());
			pending[i - 1].second.release();
		}
		g_loadedUserMods = validNames;
		for (size_t i = 0; i < validNames.size(); ++i) {
			SPLog("User mod enabled during startup (priority %u): %s",
			      static_cast<unsigned int>(i + 1), validNames[i].c_str());
		}

		// Drop missing/corrupt packages without disabling the other valid selections, and
		// complete migration from the legacy cl_activeMod setting after validation.
		if (validNames != names || !std::string(cl_activeMod).empty())
			PersistActiveUserMods(validNames);
		return errors;
	}

	void StartClient(const spades::ServerAddress &addr) {
		class ConcreteRunner : public spades::gui::Runner {
			spades::ServerAddress addr;

		protected:
			spades::gui::View *CreateView(spades::client::IRenderer *renderer,
			                              spades::client::IAudioDevice *audio) override {
				Handle<client::FontManager> fontManager(new client::FontManager(renderer), false);
				Handle<gui::View> innerView{new spades::client::Client(renderer, audio, addr, fontManager), false};
				return new spades::gui::ConsoleScreen(renderer, audio, fontManager, std::move(innerView));
			}

		public:
			ConcreteRunner(const spades::ServerAddress &addr) : addr(addr) {}
		};
		ConcreteRunner runner(addr);
		runner.RunProtected();
	}
	void StartMainScreen() {
		class ConcreteRunner : public spades::gui::Runner {
		protected:
			spades::gui::View *CreateView(spades::client::IRenderer *renderer,
			                              spades::client::IAudioDevice *audio) override {
				Handle<client::FontManager> fontManager(new client::FontManager(renderer), false);
				Handle<gui::View> innerView{new spades::gui::MainScreen(renderer, audio, fontManager), false};
				return new spades::gui::ConsoleScreen(renderer, audio, fontManager, std::move(innerView));
			}

		public:
		};
		ConcreteRunner runner;
		runner.RunProtected();
	}
} // namespace spades

static uLong computeCrc32ForStream(spades::IStream *s) {
	uLong crc = crc32(0L, Z_NULL, 0);

	char buf[16384];
	size_t sz;

	while ((sz = s->Read(buf, 16384)) != 0) {
		crc = crc32(crc, reinterpret_cast<const Bytef *>(buf), static_cast<uInt>(sz));
	}

	return crc;
}

#ifdef WIN32
static std::string Utf8FromWString(const wchar_t *ws) {
	auto *s = (char *)SDL_iconv_string("UTF-8", "UCS-2-INTERNAL", (char *)(ws), wcslen(ws) * 2 + 2);
	if (!s)
		return "";
	std::string ss(s);
	SDL_free(s);
	return ss;
}
#endif

int main(int argc, char **argv) {
#ifdef WIN32
	SetUnhandledExceptionFilter(UnhandledExceptionProc);
#endif

	for (int i = 1; i < argc;) {
		int ret = handleCommandLineArgument(argc, argv, i);
		if (!ret) {
			// ignore unknown arg
			i++;
		}
	}

	if (g_printVersion) {
		printf("%s\n", PACKAGE_STRING);
		return 0;
	}

	if (g_printHelp) {
		printHelp(argv[0]);
		return 0;
	}

	std::unique_ptr<spades::SplashWindow> splashWindow;
	bool cleanShutdownCompleted = false;

	try {

		// start recording backtrace
		spades::reflection::Backtrace::StartBacktrace();
		SPADES_MARK_FUNCTION();

		// show splash window
		// NOTE: splash window uses image loader, which assumes backtrace is already initialized.
		splashWindow.reset(new spades::SplashWindow());
		auto showSplashWindowTime = SDL_GetTicks();
		auto pumpEvents = [&splashWindow] { splashWindow->PumpEvents(); };

		// initialize threads
		spades::Thread::InitThreadSystem();
		spades::DispatchQueue::GetThreadQueue()->MarkSDLVideoThread();

		SPLog("Package: " PACKAGE_STRING);

// setup user-specific default resource directories
#ifdef WIN32
		static wchar_t buf[4096];
		GetModuleFileNameW(NULL, buf, 4096);
		std::wstring appdir = buf;
		appdir = appdir.substr(0, appdir.find_last_of(L'\\') + 1);

		// Switch to "portable" mode if "UserResources" exists
		std::wstring userAppDir = appdir + L"UserResources";

		DWORD userAppDirAttrib = GetFileAttributesW(userAppDir.c_str());
		if (userAppDirAttrib != INVALID_FILE_ATTRIBUTES &&
		    (userAppDirAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
			SPLog("UserResources found - switching to 'portable' mode");

			spades::g_userResourceDirectory = Utf8FromWString(userAppDir.c_str());
			g_userResourceFileSystem =
			  new spades::DirectoryFileSystem(spades::g_userResourceDirectory, true);
			spades::FileManager::AddFileSystem(g_userResourceFileSystem);
		} else {
			if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, buf))) {
				std::wstring datadir = buf;
				datadir += L"\\OpenSpades\\Resources";

				spades::g_userResourceDirectory = Utf8FromWString(datadir.c_str());
				g_userResourceFileSystem =
				  new spades::DirectoryFileSystem(spades::g_userResourceDirectory, true);
				spades::FileManager::AddFileSystem(g_userResourceFileSystem);
			} else {
				SPLog("SHGetFolderPathW failed.");
			}
		}

		spades::FileManager::AddFileSystem(
		  new spades::DirectoryFileSystem(Utf8FromWString((appdir + L"Resources").c_str()), false));

		// fltk has a console window on windows (can disable while building, maybe use a builtin
		// console for a later release?)
		HWND hCon = GetConsoleWindow();
		if (NULL != hCon) {
			setIcon(hCon);
		}

#elif defined(__APPLE__)
		std::string home = getenv("HOME");
		spades::FileManager::AddFileSystem(new spades::DirectoryFileSystem("./Resources", false));

		// OS X application is made of Bundle, which contains its own Resources directory.
		{
			char *baseDir = SDL_GetBasePath();
			if (baseDir) {
				spades::FileManager::AddFileSystem(new spades::DirectoryFileSystem(baseDir, false));
				SDL_free(baseDir);
			}
		}

		spades::g_userResourceDirectory =
		  home + "/Library/Application Support/OpenSpades/Resources";

		g_userResourceFileSystem =
		  new spades::DirectoryFileSystem(spades::g_userResourceDirectory, true);
		spades::FileManager::AddFileSystem(g_userResourceFileSystem);
#else
		std::string home = getenv("HOME");

		spades::FileManager::AddFileSystem(new spades::DirectoryFileSystem("./Resources", false));

		spades::FileManager::AddFileSystem(new spades::DirectoryFileSystem(
		  CMAKE_INSTALL_PREFIX "/" OPENSPADES_INSTALL_RESOURCES, false));

		std::string xdg_data_home = home + "/.local/share";

		if (getenv("XDG_DATA_HOME") == NULL) {
			SPLog("XDG_DATA_HOME not defined. Assuming that XDG_DATA_HOME is ~/.local/share");
		} else {
			xdg_data_home = getenv("XDG_DATA_HOME");
			SPLog("XDG_DATA_HOME is %s", xdg_data_home.c_str());
		}

		struct stat info;

		if (stat((xdg_data_home + "/openspades").c_str(), &info) != 0) {
			if (stat((home + "/.openspades").c_str(), &info) != 0) {
			} else if (info.st_mode & S_IFDIR) {
				SPLog("Openspades directory in XDG_DATA_HOME not found, though old directory "
				      "exists. Trying to resolve compatibility problem.");

				if (rename((home + "/.openspades").c_str(),
				           (xdg_data_home + "/openspades").c_str()) != 0) {
					SPLog("Failed to move old directory to new.");
				} else {
					SPLog("Successfully moved old directory.");

					if (mkdir((home + "/.openspades").c_str(),
					          S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0) {
						SDL_RWops *io = SDL_RWFromFile(
						  (home + "/.openspades/CONTENT_MOVED_TO_NEW_DIR").c_str(), "wb");
						if (io != NULL) {
							std::string text = ("Content of this directory moved to " + xdg_data_home + "/openspades");
							io->write(io, text.c_str(), text.length(), 1);
							io->close(io);
						}
					}
				}
			}
		}

		spades::g_userResourceDirectory = xdg_data_home + "/openspades/Resources";

		g_userResourceFileSystem =
		  new spades::DirectoryFileSystem(spades::g_userResourceDirectory, true);
		spades::FileManager::AddFileSystem(g_userResourceFileSystem);

#endif

		// start log output to SystemMessages.log
		try {
			spades::StartLog();
		} catch (const std::exception &ex) {
			SDL_InitSubSystem(SDL_INIT_VIDEO);
			auto msg = spades::Format(
			  "Failed to start recording log because of the following error:\n{0}\n\n"
			  "OpenSpades will continue to run, but any critical events are not logged.",
			  ex.what());
			if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "OpenSpades Log System Failure",
			                             msg.c_str(), splashWindow->GetWindow())) {
				// showing dialog failed.
			}
		}
		SPLog("Log Started.");

		// load preferences.
		spades::Settings::GetInstance()->Load();
		pumpEvents();

		// dump CPU info (for debugging?)
		{
			spades::CpuID cpuid;
			SPLog("---- CPU Information ----");
			SPLog("Vendor ID: %s", cpuid.GetVendorId().c_str());
			SPLog("Brand ID: %s", cpuid.GetBrand().c_str());
			SPLog("Supports MMX: %s", cpuid.Supports(spades::CpuFeature::MMX) ? "YES" : "NO");
			SPLog("Supports SSE: %s", cpuid.Supports(spades::CpuFeature::SSE) ? "YES" : "NO");
			SPLog("Supports SSE2: %s", cpuid.Supports(spades::CpuFeature::SSE2) ? "YES" : "NO");
			SPLog("Supports SSE3: %s", cpuid.Supports(spades::CpuFeature::SSE3) ? "YES" : "NO");
			SPLog("Supports SSSE3: %s", cpuid.Supports(spades::CpuFeature::SSSE3) ? "YES" : "NO");
			SPLog("Supports FMA: %s", cpuid.Supports(spades::CpuFeature::FMA) ? "YES" : "NO");
			SPLog("Supports AVX: %s", cpuid.Supports(spades::CpuFeature::AVX) ? "YES" : "NO");
			SPLog("Supports AVX2: %s", cpuid.Supports(spades::CpuFeature::AVX2) ? "YES" : "NO");
			SPLog("Supports AVX512F: %s",
			      cpuid.Supports(spades::CpuFeature::AVX512F) ? "YES" : "NO");
			SPLog("Supports AVX512CD: %s",
			      cpuid.Supports(spades::CpuFeature::AVX512CD) ? "YES" : "NO");
			SPLog("Supports AVX512ER: %s",
			      cpuid.Supports(spades::CpuFeature::AVX512ER) ? "YES" : "NO");
			SPLog("Supports AVX512PF: %s",
			      cpuid.Supports(spades::CpuFeature::AVX512PF) ? "YES" : "NO");
			SPLog("Simultaneous Multithreading: %s",
			      cpuid.Supports(spades::CpuFeature::SimultaneousMT) ? "YES" : "NO");
			SPLog("Misc:");
			SPLog("%s", cpuid.GetMiscInfo().c_str());
			SPLog("-------------------------");
		}

// register resource directory specified by Makefile (or something)
#if defined(RESDIR_DEFINED)
		spades::FileManager::AddFileSystem(new spades::DirectoryFileSystem(RESDIR, false));
#endif

		// Force PackageUpdateManager's instance to be created
		// (If PackageInfo.json is missing, the startup process will be halted here)
		// Load PackageInfo.json earlier than .pak files to prevent certain security issus
		try {
			spades::PackageUpdateManager::GetInstance();
		} catch (const std::exception &ex) {
			SPRaise("Failed to load the package information. In most cases this happens when a "
			        "file named PackageInfo.json "
			        "is corrupted or not installed properly.\n\n"
			        "Please make sure all required files are installed. "
			        "If the problem persists, please contact the package maintainer.\n\n%s",
			        ex.what());
		}

		// Search application resources for packaged data. Archives directly inside the user
		// Resources directory are mods and stay unmounted unless cl_activeMods selects them.
		{
			std::vector<spades::IFileSystem *> fss;
			std::vector<spades::IFileSystem *> fssImportant;

			std::vector<std::string> files =
			  g_userResourceFileSystem
			    ? spades::FileManager::EnumFilesExcluding("", g_userResourceFileSystem)
			    : spades::FileManager::EnumFiles("");

			struct Comparator {
				static int GetPakId(const std::string &str) {
					if (str.size() >= 4 && str[0] == 'p' && str[1] == 'a' && str[2] == 'k' &&
					    (str[3] >= '0' && str[3] <= '9')) {
						return atoi(str.c_str() + 3);
					} else {
						return 32767;
					}
				}
				static bool Compare(const std::string &a, const std::string &b) {
					int pa = GetPakId(a);
					int pb = GetPakId(b);
					if (pa == pb) {
						return a < b;
					} else {
						return pa < pb;
					}
				}
			};

			std::sort(files.begin(), files.end(), Comparator::Compare);

			for (size_t i = 0; i < files.size(); i++) {
				std::string name = files[i];

				if (!IsPackageArchiveName(name)) {
					SPLog("Ignored loose file: %s", name.c_str());
					continue;
				}
				{
					std::unique_ptr<spades::IStream> stream(
					  g_userResourceFileSystem
					    ? spades::FileManager::OpenForReadingExcluding(name.c_str(),
					                                                       g_userResourceFileSystem)
					    : spades::FileManager::OpenForReading(name.c_str()));
					uLong crc = computeCrc32ForStream(stream.get());

					stream->SetPosition(0);

					spades::ZipFileSystem *fs = new spades::ZipFileSystem(stream.get());
					stream.release();
					if (name[0] == '_' && false) { // last resort for #198
						SPLog("Pak registered: %s: %08lx (marked as 'important')", name.c_str(),
						      static_cast<unsigned long>(crc));
						fssImportant.push_back(fs);
					} else {
						SPLog("Pak registered: %s: %08lx", name.c_str(),
						      static_cast<unsigned long>(crc));
						fss.push_back(fs);
					}
				}
			}
			for (size_t i = fss.size(); i > 0; i--) {
				spades::FileManager::AppendFileSystem(fss[i - 1]);
			}
			for (size_t i = 0; i < fssImportant.size(); i++) {
				spades::FileManager::PrependFileSystem(fssImportant[i]);
			}

			std::vector<std::string> requestedMods = spades::GetActiveUserMods();
			if (!requestedMods.empty()) {
				std::string errors = spades::MountUserModsForStartup(requestedMods);
				if (!errors.empty())
					SPLog("Some configured user mods could not be enabled:\n%s", errors.c_str());
			} else if (!std::string(cl_activeMods).empty()) {
				// A non-empty value that decodes to no entries is malformed. Repair it so the
				// application does not repeatedly retry an invalid configuration.
				SPLog("Discarding malformed cl_activeMods setting");
				spades::DisableAllUserMods();
			}
		}
		pumpEvents();

		// initialize localization system
		SPLog("Initializing localization system");
		spades::LoadCurrentLocale();
		_Tr("Main", "Localization System Loaded");
		pumpEvents();

		// parse args

		// initialize AngelScript
		SPLog("Initializing script engine");
		spades::ScriptManager::GetInstance();
		pumpEvents();

		ThreadQuantumSetter quantumSetter;
		(void)quantumSetter; // suppress "unused variable" warning

		SDL_InitSubSystem(SDL_INIT_VIDEO);

		// We normally keep the splash visible long enough to be readable. A mod restart has
		// already shown the UI and should return to it as quickly as initialization allows.
		pumpEvents();
		auto ticks = SDL_GetTicks();
		if (!g_modRestartLaunch && ticks < showSplashWindowTime + 1500) {
			SDL_Delay(showSplashWindowTime + 1500 - ticks);
		}
		pumpEvents();

		// everything is now ready!
		if (!g_autoconnect) {
			if (g_modRestartLaunch ||
			    !((int)cl_showStartupWindow != 0 || splashWindow->IsStartupScreenRequested())) {
				splashWindow.reset();

				SPLog("Starting main screen");
				spades::StartMainScreen();
			} else {
				splashWindow.reset();

				SPLog("Starting startup window");
				::spades::gui::StartupScreen::Run();
			}
		} else {
			splashWindow.reset();

			spades::ServerAddress host(g_autoconnectHostName, g_autoconnectProtocolVersion);
			spades::StartClient(host);
		}

		spades::Settings::GetInstance()->Flush();

		if (g_restartRequested) {
			SPLog("Closing application resources before relaunching to apply the user mod");
			spades::CloseLog();
		}
		spades::FileManager::Close();
		cleanShutdownCompleted = true;
	} catch (const spades::ExitRequestException &) {
		// user changed his/her mind.
	} catch (const std::exception &ex) {

		try {
			splashWindow.reset(nullptr);
		} catch (...) {
		}

		std::string msg = ex.what();
		msg = _Tr("Main",
		          "A serious error caused OpenSpades to stop working:\n\n{0}\n\nSee "
		          "SystemMessages.log for more details.",
		          msg);

		SPLog("[!] Terminating due to the fatal error: %s", ex.what());

		SDL_InitSubSystem(SDL_INIT_VIDEO);
		if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
		                             _Tr("Main", "OpenSpades Fatal Error").c_str(), msg.c_str(),
		                             nullptr)) {
			// showing dialog failed.
			// TODO: do appropriate action
		}
	}

	if (g_restartRequested && cleanShutdownCompleted) {
		std::string error;
		if (!RelaunchApplication(argc, argv, error)) {
			fprintf(stderr, "OpenSpades could not restart automatically: %s\n", error.c_str());
			std::string message = spades::Format(
			  "OpenSpades could not restart automatically. Please start it manually to apply "
			  "the selected mod.\n\n{0}",
			  error);
			SDL_InitSubSystem(SDL_INIT_VIDEO);
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenSpades Restart Failed",
			                         message.c_str(), nullptr);
			return 1;
		}
	}

	return 0;
}
