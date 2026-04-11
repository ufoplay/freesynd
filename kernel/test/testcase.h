/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2013, 2024-2026  Benoit Blancard <benblan@users.sourceforge.net>
 *
 *   This program is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>. 
 * 
 */

 #include <string>

 #include "fs-utils/io/configfile.h"
 #include "fs-kernel/model/map.h"
 #include "fs-engine/gfx/tilemanager.h"

 void initWeaponConfigFile( ConfigFile &config );

/*!
 * @brief Loads a tile map from a CSV file and initializes the Map via setTiles().
 *
 * File format:
 *   - Lines starting with '#' are ignored (comments).
 *   - The first non-comment line contains the map dimensions: maxX,maxY,maxZ
 *   - Each subsequent line describes one Z level:
 *       z_value,tileId(x=0,y=0),tileId(x=1,y=0),...,tileId(x=maxX-1,y=0),tileId(x=0,y=1),...
 *     Tile IDs are listed in row-major order (y increasing, then x increasing).
 *
 * @param filepath Absolute or relative path to the CSV file.
 * @param tileMgr  TileManager used to resolve tile IDs.
 * @param map      Map to initialize.
 * @return true if loading succeeded, false otherwise.
 */
 bool loadMapFromCsv(const std::string &filepath,
                     fs_eng::TileManager &tileMgr,
                     fs_knl::Map &map);