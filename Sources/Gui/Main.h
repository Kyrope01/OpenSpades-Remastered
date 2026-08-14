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
	/** Returns the user mod package mounted for the current process, or an empty string. */
	std::string GetActiveUserMod();
	/** Mounts and persists a mod, or disables mods when name is empty. Returns an error string. */
	std::string SetActiveUserMod(const std::string &name);

	void StartClient(const ServerAddress &, const std::string &playerName);
	void StartMainScreen();
}
