/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
 *   Copyright (C) 2010  Bohdan Stelmakh <chamel@users.sourceforge.net> 
 *   Copyright (C) 2024-2025  Benoit Blancard <benblan@users.sourceforge.net>
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

#ifndef MAPMANAGER_H
#define MAPMANAGER_H

#include <map>
#include "fs-utils/common.h"
#include "fs-kernel/model/map.h"
#include "fs-kernel/model/leveldata.h"

namespace fs_knl {
/*!
 * Map manager class.
 */
class MapManager {
public:
    MapManager(fs_eng::TileManager *pTileManager);
    ~MapManager();

    //! Load the map with the given id if not in cache
    Map * getMap(const LevelData::MapInfos &mapInfo);
    //! Sets the palette for the given mission
    bool loadPalette(int paletteId);

protected:
    //! Create a new Map using the game data
    Map * createMap(uint16_t mapId, uint8_t * mapData, const LevelData::MapInfos &mapInfo);
    //! Use to correct some data after loading the map
    void patchMap(uint16_t mapId);

protected:
    //! a cache of the latest maps
    std::map<int, Map *> maps_;
    fs_eng::TileManager *pTileManager_;
};

}
#endif
