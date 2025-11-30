/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
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

#include "fs-kernel/mgr/mapmanager.h"

#include <format>

#include "fs-utils/io/file.h"
#include "fs-utils/log/log.h"

namespace fs_knl {

MapManager::MapManager(fs_eng::TileManager *pTileManager) : pTileManager_(pTileManager)
{
}

MapManager::~MapManager()
{
    for (unsigned int i = 0; i < maps_.size(); i++)
        delete maps_[i];
}

/*!
 * Loads the given map.
 * First look in the map cache if the map already exists.
 * If the map exists, returns it. Otherwise, creates a new one.
 * \param mapNum The map id.
 * \return NULL if map could not be loaded
 */
Map * MapManager::getMap(uint16_t mapNum)
{
    LOG(Log::k_FLG_IO, "MapManager", "getMap", ("get map %i", mapNum));
    // First look in cache
    if (maps_.find(mapNum) != maps_.end()) {
        LOG(Log::k_FLG_IO, "MapManager", "getMap", ("Map is already in cache"));
        return maps_[mapNum];
    }

    // Not found so construct new one
    std::string filename = std::format("map{:02}.dat", mapNum);
    LOG(Log::k_FLG_IO, "MapManager", "getMap", ("Load new map from file %s", filename.c_str()));
    size_t size;
    uint8 *mapData = fs_utl::File::loadOriginalFile(filename, size);
    if (mapData == NULL) {
        return NULL;
    }

    maps_[mapNum] = createMap(mapNum, mapData);
    patchMap(mapNum);

    delete[] mapData;

    LOG(Log::k_FLG_GFX, "MapManager", "loadMap", ("Loading finished"));

    return maps_[mapNum];
}

/*!
 * 
 * @param mapNum 
 * @param mapData 
 * @return 
 */
Map * MapManager::createMap(uint16_t mapNum, uint8_t * mapData) {
    int maxX = fs_utl::READ_LE_UINT32(mapData + 0);
    int maxY = fs_utl::READ_LE_UINT32(mapData + 4);
    int maxZ = fs_utl::READ_LE_UINT32(mapData + 8);

    LOG(Log::k_FLG_GFX, "MapManager", "createMap",
        ("Map size in tiles: max_x = %d, max_y = %d, max_z = %d.", maxX, maxY, maxZ));

    uint32_t *lookup = new uint32_t[maxX * maxY];
    // NOTE : increased map height by 1 to enable range check on higher tiles
    int maxZWithRangeCheck = maxZ + 1;
    fs_eng::Tile **tiles = new fs_eng::Tile*[maxX * maxY * maxZWithRangeCheck];

    for (int i = 0; i < maxX * maxY; i++)
        lookup[i] = fs_utl::READ_LE_UINT32(mapData + 12 + i * 4);
    // Fill the tiles array except for the higher level
    for (int y = 0; y < maxY; y++)
        for (int x = 0; x < maxX; x++) {
            int idx = y * maxX + x;

            for (int z = 0; z < maxZ; z++) {
                uint8_t tileNum = *(mapData + 12 + lookup[idx] + z);
                tiles[idx * maxZWithRangeCheck + z] = pTileManager_->getTile(tileNum);
            }
        }
    delete[] lookup;

    maxZ++;
    for (int y = 0, z = maxZ - 1; y < maxY; y++) {
        for (int x = 0; x < maxX; x++) {
            tiles[(y * maxX + x) * maxZ + z] = pTileManager_->getTile(0);
        }
    }

    Map *pMap = new Map(pTileManager_, mapNum);
    pMap->setTiles(maxX, maxY, maxZ, tiles);

    delete[] tiles;

    return pMap;
}

void MapManager::patchMap(uint16_t mapNum) {
    // patch for "YUKON" map
    if (mapNum == 0x27) {
        maps_[mapNum]->patchMap(60, 63, 1, 0x27);
        maps_[mapNum]->patchMap(61, 63, 1, 0x27);
        maps_[mapNum]->patchMap(62, 63, 1, 0x27);
        maps_[mapNum]->patchMap(60, 64, 1, 0x27);
        maps_[mapNum]->patchMap(61, 64, 1, 0x27);
        maps_[mapNum]->patchMap(62, 64, 1, 0x27);
        maps_[mapNum]->patchMap(60, 65, 1, 0x27);
        maps_[mapNum]->patchMap(61, 65, 1, 0x27);
        maps_[mapNum]->patchMap(62, 65, 1, 0x27);
        maps_[mapNum]->patchMap(60, 66, 1, 0x27);
        maps_[mapNum]->patchMap(61, 66, 1, 0x27);
        maps_[mapNum]->patchMap(62, 66, 1, 0x27);
        maps_[mapNum]->patchMap(60, 67, 1, 0x27);
        maps_[mapNum]->patchMap(61, 67, 1, 0x27);
        maps_[mapNum]->patchMap(62, 67, 1, 0x27);
    }
    // patch for "INDONESIA" map
    // TODO: find better way to block access for our agents
    if (mapNum == 0x5B) {
        maps_[mapNum]->patchMap(49, 27, 2, 0);
        maps_[mapNum]->patchMap(49, 28, 2, 0);
        maps_[mapNum]->patchMap(49, 29, 2, 0);
    }
}

/*!
 * @brief 
 * @param paletteId 
 * @return 
 */
bool MapManager::loadPalette(int paletteId) {    
    return pTileManager_->setPalette(paletteId);
}

}
