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
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <sstream>

#include "testcase.h"

void initWeaponConfigFile( ConfigFile &config ) {
    config.add("weapon.1.name", "WEAPON_PISTOL");
    config.add("weapon.1.icon.small", 15);
    config.add("weapon.1.icon.big", 65);
    config.add("weapon.1.cost", 0);
    config.add("weapon.1.ammo.nb", 13);
    config.add("weapon.1.ammo.price", 0);
    config.add("weapon.1.range", 1280);
    config.add("weapon.1.rank", 1);
    config.add("weapon.1.anim", 368);
    config.add("weapon.1.ammopershot", 1);
    config.add("weapon.1.timereload", 600);
    config.add("weapon.1.damagerange", 0);
    config.add("weapon.1.shotangle", 5.0);
    config.add("weapon.1.shotaccuracy", 0.9);
    config.add("weapon.1.shotspeed", 0);
    config.add("weapon.1.dmg_per_shot", 2);
    config.add("weapon.1.ammo.impactNb", 1);
    config.add("weapon.1.weight", 1);

    config.add("weapon.2.name", "GAUSS_GUN");
    config.add("weapon.2.icon.small", 15);
    config.add("weapon.2.icon.big", 65);
    config.add("weapon.2.cost", 0);
    config.add("weapon.2.ammo.nb", 13);
    config.add("weapon.2.ammo.price", 0);
    config.add("weapon.2.range", 1280);
    config.add("weapon.2.rank", 1);
    config.add("weapon.2.anim", 368);
    config.add("weapon.2.ammopershot", 1);
    config.add("weapon.2.timereload", 600);
    config.add("weapon.2.damagerange", 0);
    config.add("weapon.2.shotangle", 5.0);
    config.add("weapon.2.shotaccuracy", 0.9);
    config.add("weapon.2.shotspeed", 0);
    config.add("weapon.2.dmg_per_shot", 2);
    config.add("weapon.2.ammo.impactNb", 1);
    config.add("weapon.2.weight", 15);

    config.add("weapon.12.name", "scanner");
    config.add("weapon.12.icon.small", 26);
    config.add("weapon.12.icon.big", 76);
    config.add("weapon.12.cost", 1000);
    config.add("weapon.12.range", 256);
    config.add("weapon.12.anim", 379);
    config.add("weapon.12.timereload", 1);
    config.add("weapon.12.weight", 1);

    config.add("weapon.9.name", "WEAPON_SCANNER");
    config.add("weapon.9.icon.small", 23);
    config.add("weapon.9.icon.big", 73);
    config.add("weapon.9.cost", 500);
    config.add("weapon.9.range", 4096);
    config.add("weapon.9.anim", 376);
    config.add("weapon.9.weight", 1);

    config.add("weapon.13.name", "WEAPON_ENERGY_SHIELD");
    config.add("weapon.13.icon.small", 28);
    config.add("weapon.13.icon.big", 78);
    config.add("weapon.13.cost", 8000);
    config.add("weapon.13.ammo.nb", 200);
    config.add("weapon.13.ammo.price", 15);
    config.add("weapon.13.range", 768);
    config.add("weapon.13.anim", 381);
    config.add("weapon.13.ammopershot", 1);
    config.add("weapon.13.auto.fire_rate", 75);
    config.add("weapon.13.ammo.impactNb", 1);
    config.add("weapon.13.weight", 8);
}

bool loadMapFromCsv(const std::string &filepath,
                    fs_eng::TileManager &tileMgr,
                    fs_knl::Map &map) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    int maxX = 0, maxY = 0, maxZ = 0;
    std::string line;

    // Read dimensions from the first non-comment line
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        char sep;
        ss >> maxX >> sep >> maxY >> sep >> maxZ;
        break;
    }
    if (maxX <= 0 || maxY <= 0 || maxZ <= 0) return false;

    int total = maxX * maxY * maxZ;
    fs_eng::Tile **tiles = new fs_eng::Tile*[total];
    // Default every slot to the transparent tile
    for (int i = 0; i < total; i++) {
        tiles[i] = tileMgr.getTile(
            static_cast<uint8_t>(fs_eng::TileManager::kIndexTransparentTile));
    }

    int previousZ = -1;
    int y = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        char sep;
        int z;
        ss >> z >> sep;
        y++;
        if (y < maxY) {
            if (z != previousZ) {
                previousZ = z;
                y = 0;
            }
            for (int x = 0; x < maxX; x++) {
                int tileId;
                ss >> tileId;
                if (x + 1 < maxX || y + 1 < maxY) ss >> sep; // consume comma
                tiles[(y * maxX + x) * maxZ + z] =
                    tileMgr.getTile(static_cast<uint8_t>(tileId));
            }
        }
    }

    map.setTiles(maxX, maxY, maxZ, tiles);
    delete[] tiles;
    return true;
}

MemoryTileManager::MemoryTileManager() : fs_eng::TileManager() {
    // Default transparent tile (kNone — not road, not ped crossing, not road mark)
    tiles_[0]   = new fs_eng::Tile(0,   false, fs_eng::Tile::kNone,        {0, 0});
    // Road tile (kRoadNtoS id, kRoadSideEW type — isRoad() == true)
    tiles_[106] = new fs_eng::Tile(106, false, fs_eng::Tile::kRoadSideEW,  {0, 0});
    // Pedestrian crossing tiles (kRoadPedCross type — isRoad() and isPedCrossing() both true)
    tiles_[225] = new fs_eng::Tile(225, false, fs_eng::Tile::kRoadPedCross, {0, 0});
    tiles_[226] = new fs_eng::Tile(226, false, fs_eng::Tile::kRoadPedCross, {0, 0});
    // Road mark separator tiles (kRoadMark type — isRoadMark() == true, isRoad() == false)
    tiles_[100] = new fs_eng::Tile(100, false, fs_eng::Tile::kRoadMark,    {0, 0});
    tiles_[101] = new fs_eng::Tile(101, false, fs_eng::Tile::kRoadMark,    {0, 0});
    tiles_[114] = new fs_eng::Tile(114, false, fs_eng::Tile::kRoadMark,    {0, 0});
    tiles_[115] = new fs_eng::Tile(115, false, fs_eng::Tile::kRoadMark,    {0, 0});
    tiles_[116] = new fs_eng::Tile(116, false, fs_eng::Tile::kRoadMark,    {0, 0});
    tiles_[117] = new fs_eng::Tile(117, false, fs_eng::Tile::kRoadMark,    {0, 0});
}

TEST_CASE( "1: All test cases reside in other .cpp files (empty)", "[multi-file:1]" ) {
}