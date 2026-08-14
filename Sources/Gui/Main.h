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
#pragma once

#include <string>
#include <vector>

namespace spades {
	class ServerAddress;

	/** The path to the user resource directory. Can be empty. */
	extern std::string g_userResourceDirectory;

	/** Returns supported package archives directly inside the user resource directory. */
	std::vector<std::string> GetAvailableUserMods();
	/** Returns the mod selected for the next launch, or an empty string. */
	std::string GetActiveUserMod();
	/** Returns the user mod loaded during this process's startup, or an empty string. */
	std::string GetLoadedUserMod();
	/** Validates and persists a mod selection, or disables mods when name is empty. */
	std::string SetActiveUserMod(const std::string &name);
	/** Requests a clean process relaunch after the current main-screen runner closes. */
	void RequestApplicationRestart();

	void StartClient(const ServerAddress &, const std::string &playerName);
	void StartMainScreen();
}
