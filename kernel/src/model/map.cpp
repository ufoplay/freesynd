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

#include "fs-kernel/model/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cassert>

#include "fs-utils/log/log.h"
#include "fs-engine/gfx/tilemanager.h"

namespace fs_knl {

Map::Map(fs_eng::TileManager * tileManager, uint16_t anId) : tileManager_(tileManager)
{
    id_ = anId;
    a_tiles_ = nullptr;
    assert(tileManager != nullptr);
    minScrollTile_ = {0, 0};
    maxScrollTile_ = {0, 0};
}

Map::~Map()
{
    delete[] a_tiles_;
}

/*!
 * Set the list of tiles used for this map.
 * Loading tiles from game files is done in MapManager.
 * @param maxX Number of tile on X axis
 * @param maxY Number of tile on Y axis
 * @param maxZ Number of tile on Z axis
 * @param tiles The array of tiles. The array has one level more than
 * in game file for range checking
 */
void Map::setTiles(int maxX, int maxY, int maxZ, fs_eng::Tile **tiles) {
    maxTx_ = maxX;
    maxTy_ = maxY;
    maxTz_ = maxZ;

    int size = maxTx_ * maxTy_ * maxTz_;
    a_tiles_ = new fs_eng::Tile*[size];
    
    if (tiles != nullptr) {
        for (int i=0; i<size; i++) {
            a_tiles_[i] = tiles[i];
        }
    }
}

void Map::setScrollLimits(Point2D minScrollTile, Point2D  maxScrollTile) {
    LOG(Log::k_FLG_GAME, "Map", "setScrollLimits", ("Min Tx Ty(%d, %d), max Tx Ty(%d, %d)\n", minScrollTile.x, minScrollTile.y, maxScrollTile.x, maxScrollTile.y));
    minScrollTile_ = minScrollTile;
    maxScrollTile_ = maxScrollTile;
}

/*!
 * @brief 
 * @param point 
 */
void Map::clipToScrollLimits(fs_knl::TilePoint &point) {
    if (point.tx < minScrollTile_.x)
        point.tx = minScrollTile_.x;
    else if (point.tx > maxScrollTile_.x) {
        point.tx = maxScrollTile_.x;
    }

    if (point.ty < minScrollTile_.y)
        point.ty = minScrollTile_.y;
    else if (point.ty > maxScrollTile_.y) {
        point.ty = maxScrollTile_.y;
    }
}


void Map::mapDimensions(int *x, int *y, int *z)
{
    *x = maxTx();
    *y = maxTy();
    *z = maxTz();
}

/*!
 * Clip x,y,z to map dimensions.
 */
void Map::adjXYZ(int &x, int &y, int &z) {
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (z < 0 || z >= maxTz())
        z = 0;
    if (x >= maxTx())
        x = maxTx() - 1;
    if (y >= maxTy())
        y = maxTy() - 1;
}

void Map::clip(Point2D *pPoint) {
    if (pPoint->x < 0) {
        pPoint->x = 0;
    } else if (pPoint->x >= maxTx()) {
        pPoint->x = maxTx();
    }

    if (pPoint->y < 0) {
        pPoint->y = 0;
    } else if (pPoint->y >= maxTy()) {
        pPoint->y = maxTy();
    }
}

void Map::clip(TilePoint *pPoint) {
    if (pPoint->tx < 0) {
        pPoint->tx = 0;
    } else if (pPoint->tx >= maxTx()) {
        pPoint->tx = maxTx() - 1;
    }

    if (pPoint->ty < 0) {
        pPoint->ty = 0;
    } else if (pPoint->ty >= maxTy()) {
        pPoint->ty = maxTy() - 1;
    }

    if (pPoint->tz < 0 || pPoint->tz >= maxTz()) {
        pPoint->tz = 0;
    }
}

/**
 * Checks if a tile point is within the map boundaries.
 * @return true if point is within boundaries
 */
bool Map::isWithinMapBounds(const TilePoint &point) const {
    return point.tz >= 0 && point.tz < maxTz() &&
           point.tx >= 0 && point.tx < maxTx() &&
           point.ty >= 0 && point.ty < maxTy();
}

/**
 * Projects a tile point to a specific Z level using isometric transformation.
 * Each Z level shifts position by half a tile (128 units) in both X and Y.
 */
TilePoint Map::projectToZLevel(const TilePoint &point, int targetZ) {
    TilePoint result;
    
    // Convert to continuous coordinates with isometric Z offset
    int worldX = point.tx * 256 + point.ox + 128 * (targetZ - 1);
    int worldY = point.ty * 256 + point.oy + 128 * (targetZ - 1);
    
    // Convert back to tile + offset coordinates
    result.tx = worldX / 256;
    result.ox = worldX % 256;
    result.ty = worldY / 256;
    result.oy = worldY % 256;
    result.tz = targetZ;
    
    return result;
}

float scalexPx = 256.0f;
float scalexPy = 256.0f;
float scaleyPx = 256.0f;
float scaleyPy = 256.0f;


void Map::tileToScreenPoint(const TilePoint &tPt, Point2D *pScp) {
    float fx = static_cast<float> (tPt.tx) + tPt.ox / scalexPx;
    float fy = tPt.ty + tPt.oy / scalexPy;

    pScp->x = (int) ((maxTx_ * fs_eng::Tile::kTileWidth / 2) + (fx - fy) * fs_eng::Tile::kTileWidth / 2
                  + fs_eng::Tile::kTileWidth / 2);

    pScp->y = (int) ((maxTz_ + 1) * fs_eng::Tile::kTileHeight / 3 + (fx + fy) * fs_eng::Tile::kTileHeight / 3);
}

/*!
 * Returns the tile position corresponding to the screen position.
 * Usually after the user clicked on the map, this method is used
 * to find what tile has been clicked.
 * \param x The x position on the screen (in pixel).
 * \param y The y position on the screen (in pixel).
 */
TilePoint Map::screenToTilePoint(int x, int y)
{
    TilePoint mtp;

    x -= (maxTx_ + 1) * (fs_eng::Tile::kTileWidth / 2);
    // x now equals fx * Tile::kTileWidth / 2 - fy * Tile::kTileWidth / 2
    // which equals Tile::kTileWidth/2 * (fx - fy)
    y -= (maxTz_ + 1) * (fs_eng::Tile::kTileHeight / 3);
    // y now equals (fx + fy) * Tile::kTileHeight / 3
    float dx = (float) x / (fs_eng::Tile::kTileWidth / 2);
    float dy = (float) y / (fs_eng::Tile::kTileHeight / 3);

    // dx equals fx - fy
    // dy equals fx + fy
    float f_tx = (dx + dy) / 2;
    mtp.tx = (int) f_tx;
    mtp.ox = (int) ((f_tx - mtp.tx) * 256.0f);

    float f_ty = (dy - dx) / 2;
    mtp.ty = (int) f_ty;
    mtp.oy = (int) ((f_ty - mtp.ty) * 256.0f);

    return mtp;
}

int Map::maxZAt(int x, int y)
{
    assert(x < maxTx_);
    assert(y < maxTy_);
    /*
    int idx = y * maxTx_ + x;
    int mz = 0;
    for (int z = 0; z < maxTz_; z++) {
        int tile = map_data_[idx * maxTz_ + z];
        if (tile > 5) {
            mz = z;
        }
    }
    */
    // TODO: disabling this thing, causes a lot of speed drain
    // find a better for such optimization, not all objects are drawn
    // that is why I disabled it
    return maxTz_ - 1;
}

fs_eng::Tile * Map::getTileAt(int x, int y, int z)
{
    if (x < 0 || x >= maxTx_ || y < 0 || y >= maxTy_) {
        return tileManager_->getTile(z < 2 ? 6 : 0);
    }

    if (z < 0 || z >= maxTz_) {
        return tileManager_->getTile(0);
    }

    return a_tiles_[(y * maxTx_ + x) * maxTz_ + z];
}

/*!
 * Return the tile at given position.
 * @param tilePt Coord of the tile. Only uses tx, ty and tz
 * @return If coord are out of map limit, return a default tile
 */
fs_eng::Tile * Map::getTileAt(const TilePoint &tilePt) {
    return getTileAt(tilePt.tx, tilePt.ty, tilePt.tz);
}

int Map::getTileIdAt(int tx, int ty, int tz)
{
    if (tx < 0 || tx >= maxTx_)
        return tz < 2 ? 6 : 0;
    if (ty < 0 || ty >= maxTy_)
        return tz < 2 ? 6 : 0;
    if (tz < 0 || tz >= maxTz_)
        return 0;

    return a_tiles_[(ty * maxTx_ + tx) * maxTz_ + tz]->id();
}

void Map::patchMap(int x, int y, int z, uint8_t tileNum)
{
    assert((x >= 0 && x < maxTx_)
        && (y >= 0 && y < maxTy_)
        && (z >= 0 && z < maxTz_));
    a_tiles_[(y * maxTx_ + x) * maxTz_ + z] = tileManager_->getTile(tileNum);
}


/**
 * Return true if tile at given position is traversable by a car.
 * \param x int X coordinate
 * \param y int Y coordinate
 * \param z int Z coordinate
 * \return bool
 *
 */
bool Map::isTileWalkableByCar(int x, int y, int z)
{
    fs_eng::Tile *pTile = getTileAt(x, y, z);
    int tileId = pTile->id();

    if(tileId == 80) {
        fs_eng::Tile::EType near_type = getTileAt(x, y - 1, z)->type();
        if((near_type < fs_eng::Tile::kRoadSideEW || near_type > fs_eng::Tile::kRoadSideNS)
            && near_type != fs_eng::Tile::kRoadPedCross) {
            return false;
        }
        near_type = getTileAt(x, y + 1, z)->type();
         if((near_type < fs_eng::Tile::kRoadSideEW || near_type > fs_eng::Tile::kRoadSideNS)
             && near_type != fs_eng::Tile::kRoadPedCross)
         {
            return false;
         }
        return true;
    }
    if(tileId == 81) {
        fs_eng::Tile::EType near_type = getTileAt(x - 1, y, z)->type();
         if((near_type < fs_eng::Tile::kRoadSideEW || near_type > fs_eng::Tile::kRoadSideNS)
             && near_type != fs_eng::Tile::kRoadPedCross)
         {
            return false;
         }
        near_type = getTileAt(x + 1, y, z)->type();
         if((near_type < fs_eng::Tile::kRoadSideEW || near_type > fs_eng::Tile::kRoadSideNS)
             && near_type != fs_eng::Tile::kRoadPedCross)
         {
            return false;
         }
        return true;
    }
    if(tileId == 72) {
        return false;
    }

    if(tileId == 119) {
        return false;
    }
    return  pTile->isRoad();
}

/*!
 * Returns a 16-bit bitmask encoding the valid exit directions for the road tile at (x, y, z).
 *
 * The bitmask is divided into four nibbles (4 bits each), one per cardinal direction:
 * \code
 *   bits 15-12  kDirMaskWest  : West  (tx-1)
 *   bits 11-8   kDirMaskNorth : North (ty-1)
 *   bits  7-4   kDirMaskEast  : East  (tx+1)
 *   bits  3-0   kDirMaskSouth : South (ty+1)
 * \endcode
 * A nibble equal to 0xF means the direction is \b blocked; any other value means it is \b allowed
 * (the value encodes the kForbidDir* constant for that direction).
 *
 * Special return values:
 * - kTileDirNone (0x0000) : tile is not a road — no movement allowed
 * - kTileDirAll  (0xFFFF) : intersection — all directions are open
 *
 * \param x Tile X coordinate
 * \param y Tile Y coordinate
 * \param z Tile Z coordinate (road level, i.e. pos_.tz - 1)
 * \return Direction bitmask for the tile.
 */
uint16_t Map::getPossibleDirectionsFromRoadTile(int x, int y, int z) {
    uint16_t dir = kTileDirNone;
    int near_tile;

    switch(getTileIdAt(x, y, z)){
        case fs_eng::Tile::kTileRoadEW:
            if(getTileIdAt(x + 1, y, z) == fs_eng::Tile::kTileRoadEW)
                dir = (0)|(0xFFF0);
            if(getTileIdAt(x - 1, y, z) == fs_eng::Tile::kTileRoadEW)
                dir = (4<<8)|(0xF0FF);
            break;
        case fs_eng::Tile::kTileRoadNS:
            if(getTileIdAt(x, y - 1, z) == fs_eng::Tile::kTileRoadNS)
                dir = (2<<4)|(0xFF0F);
            if(getTileIdAt(x, y + 1, z) == fs_eng::Tile::kTileRoadNS)
                dir = (6<<12)|(0x0FFF);
            break;
        case fs_eng::Tile::kTileRoadNtoS:
            dir = (0)|(2<<4)|(6<<12)|(0x0F00);

            if(getTileIdAt(x + 1, y - 1, z) != fs_eng::Tile::kTileRoundAbout)
                dir |= 0x0FF0;
            if(getTileIdAt(x + 1, y + 1, z) != fs_eng::Tile::kTileRoundAbout)
                dir |= 0xFF00;
            near_tile = getTileIdAt(x + 1, y, z);
            if (near_tile == fs_eng::Tile::kTileRoadWtoE || near_tile == fs_eng::Tile::kTileRoadEtoW)
                dir = (dir & kDirClearWest) | kForbidDirWest;

            break;
        case fs_eng::Tile::kTileRoadStoN:
            dir = (2<<4)|(4<<8)|(6<<12)|(0x000F);

            if(getTileIdAt(x - 1, y - 1, z) != fs_eng::Tile::kTileRoundAbout)
                dir |= 0x00FF;
            if(getTileIdAt(x - 1, y + 1, z) != fs_eng::Tile::kTileRoundAbout)
                dir |= 0xF00F;
            near_tile = getTileIdAt(x - 1, y, z);
            if (near_tile == fs_eng::Tile::kTileRoadWtoE || near_tile == fs_eng::Tile::kTileRoadEtoW)
                dir = (dir & kDirClearEast) | kForbidDirEast;

            break;
        case fs_eng::Tile::kTileRoadWtoE:
            dir = (0)|(2<<4)|(4<<8)|(0xF000);

            if(getTileIdAt(x + 1, y - 1, z) != fs_eng::Tile::kTileRoundAbout)
                dir |= 0xF00F;
            if(getTileIdAt(x - 1, y - 1, z) != fs_eng::Tile::kTileRoundAbout)
                dir |= 0xFF00;
            near_tile = getTileIdAt(x, y - 1, z);
            if (near_tile == fs_eng::Tile::kTileRoadNtoS || near_tile == fs_eng::Tile::kTileRoadStoN)
                dir = dir & kDirClearSouth;

            break;
        case fs_eng::Tile::kTileRoadEtoW:
            dir = (0)|(4<<8)|(6<<12)|(0x00F0);

            if(getTileIdAt(x + 1, y + 1, z) != fs_eng::Tile::kTileRoundAbout)
                dir |= 0x00FF;
            if(getTileIdAt(x - 1, y + 1, z) != fs_eng::Tile::kTileRoundAbout)
                dir |= 0x0FF0;
            near_tile = getTileIdAt(x, y + 1, z);
            if (near_tile == fs_eng::Tile::kTileRoadNtoS || near_tile == fs_eng::Tile::kTileRoadStoN)
                dir = (dir & kDirClearNorth) | kForbidDirNorth;

            break;
        case fs_eng::Tile::kTileCurveWtoS:
            dir = (0) | (2<<4)|(0xFF00);
            break;
        case fs_eng::Tile::kTileCurveNtoW:
            dir = (0) | (6<<12)|(0x0FF0);
            break;
        case fs_eng::Tile::kTileCurveStoE:
            dir = (2<<4)|(4<<8)|(0xF00F);
            break;
        case fs_eng::Tile::kTileCurveEtoN:
            dir = (4<<8)|(6<<12)|(0x00FF);
            break;
        /*case 119:
            // TODO: Greenland map needs fixing
            dir = kTileDirAll;
            near_tile = pMap_->tileAt(x, y + 1, z);
            if (near_tile == kTileJunctionSW || near_tile == kTilePedCrossEW || near_tile == kTilePedCrossNS)
                dir = (dir & kDirClearNorth) | kForbidDirNorth;
            near_tile = pMap_->tileAt(x, y + 1, z);
            if (near_tile == kTileJunctionSE || near_tile == kTilePedCrossEW || near_tile == kTilePedCrossNS)
               dir &= kDirClearSouth;
            near_tile = pMap_->tileAt(x + 1, y, z);
            if (near_tile == kTileJunctionNW || near_tile == kTilePedCrossEW || near_tile == kTilePedCrossNS)
                dir = (dir & kDirClearEast) | kForbidDirEast;
            near_tile = pMap_->tileAt(x - 1, y, z);
            if (near_tile == kTileJunctionNE || near_tile == kTilePedCrossEW || near_tile == kTilePedCrossNS)
                dir = (dir & kDirClearWest) | kForbidDirWest;
            if (dir == kTileDirAll)
                dir = kTileDirNone;
            break;*/
        case fs_eng::Tile::kTileCurveNtoE:
            dir = (0)|(2<<4)|(0xFF00);
            break;
        case fs_eng::Tile::kTileCurveEtoS:
            dir = (0)|(6<<12)|(0x0FF0);
            break;
        case fs_eng::Tile::kTileExtCurveStoW:
            dir = (4<<8)|(6<<12)|(0x00FF);
            break;
        case fs_eng::Tile::kTileExtCurveWtoN:
            dir = (2<<4)|(4<<8)|(0xF00F);
            break;
        case fs_eng::Tile::kTilePedCrossNS:/*
            if(pMap_->getTileAt(x + 1, y, z)->type() == Tile::kRoadPedCross)
                dir = (0)|(0xFFF0);
            else if(pMap_->getTileAt(x - 1, y, z)->type() == Tile::kRoadPedCross)
                dir = (4<<8)|(0xF0FF);
            else {*/
                dir = kTileDirAll;
                near_tile = getTileIdAt(x, y + 1, z);
                if (/*near_tile == 119 || */near_tile == fs_eng::Tile::kTileRoadNtoS
                    || near_tile == fs_eng::Tile::kTileRoadStoN || near_tile == fs_eng::Tile::kTileRoadEW || near_tile == fs_eng::Tile::kTilePedCrossNS)
                    dir = (dir & kDirClearNorth) | kForbidDirNorth;
                near_tile = getTileIdAt(x, y - 1, z);
                if (/*near_tile == 119 || */near_tile == fs_eng::Tile::kTileRoadNtoS
                    || near_tile == fs_eng::Tile::kTileRoadStoN || near_tile == fs_eng::Tile::kTileRoadEW || near_tile == fs_eng::Tile::kTilePedCrossNS)
                    dir &= kDirClearSouth;
                near_tile = getTileIdAt(x + 1, y, z);
                if (/*near_tile == 119 || */near_tile == fs_eng::Tile::kTileRoadWtoE || near_tile == fs_eng::Tile::kTileRoadNS)
                    dir = (dir & kDirClearEast) | kForbidDirEast;
                near_tile = getTileIdAt(x - 1, y, z);
                if (/*near_tile == 119 || */near_tile == fs_eng::Tile::kTileRoadEtoW || near_tile == fs_eng::Tile::kTileRoadNS)
                    dir = (dir & kDirClearWest) | kForbidDirWest;
                if (dir == kTileDirAll)
                    dir = kTileDirNone;
            //}
            break;
        case fs_eng::Tile::kTilePedCrossEW:/*
            if(pMap_->getTileAt(x, y - 1, z)->type() == Tile::kRoadPedCross)
                dir = (2<<4)|(0xFF0F);
            else if(pMap_->getTileAt(x, y + 1, z)->type() == Tile::kRoadPedCross)
                dir = (6<<12)|(0x0FFF);
            else {*/
                dir = kTileDirAll;
                near_tile = getTileIdAt(x, y + 1, z);
                if (/*near_tile == 119 || */near_tile == fs_eng::Tile::kTileRoadNtoS || near_tile == fs_eng::Tile::kTileRoadEW)
                    dir = (dir & kDirClearNorth) | kForbidDirNorth;
                near_tile = getTileIdAt(x, y - 1, z);
                if (/*near_tile == 119 || */near_tile == fs_eng::Tile::kTileRoadStoN || near_tile == fs_eng::Tile::kTileRoadEW)
                    dir &= kDirClearSouth;
                near_tile = getTileIdAt(x + 1, y, z);
                if (/*near_tile == 119 || */near_tile == fs_eng::Tile::kTileRoadWtoE || near_tile == fs_eng::Tile::kTileRoadEtoW
                    || near_tile == fs_eng::Tile::kTileRoadNS || near_tile == fs_eng::Tile::kTilePedCrossEW)
                    dir = (dir & kDirClearEast) | kForbidDirEast;
                near_tile = getTileIdAt(x - 1, y, z);
                if (/*near_tile == 119 || */near_tile == fs_eng::Tile::kTileRoadWtoE || near_tile == fs_eng::Tile::kTileRoadEtoW
                    || near_tile == fs_eng::Tile::kTileRoadNS || near_tile == fs_eng::Tile::kTilePedCrossEW)
                    dir = (dir & kDirClearWest) | kForbidDirWest;
                if (dir == kTileDirAll)
                    dir = kTileDirNone;
            //}
            break;
        default:
            dir = kTileDirAll;
    }

    return dir;
}

const uint8_t MiniMap::kOverlayNone = 0;
const uint8_t MiniMap::kOverlayOurAgent = 1;
const uint8_t MiniMap::kOverlayEnemyAgent = 2;

/*!
 * Construct the minimap.
 */
MiniMap::MiniMap() :
    a_minimap_(nullptr),
    mmax_x_(0), mmax_y_(0),
    p_target_(nullptr) {}

MiniMap::~MiniMap() {
    if (a_minimap_) {
        free(a_minimap_);
        a_minimap_ = nullptr;
    }
}

bool MiniMap::init(Map *p_map) {
    // walkdata based colours
    uint8_t minimap_colours[] = {
        8,  7,  7,  7,
        7,  7, 10, 10,
       10, 10,  0, 10,
       15, 15, 10, 10,
       0,   0,  0,  0,
    };

    mmax_x_ = p_map->maxTx();
    mmax_y_ = p_map->maxTy();

    a_minimap_ = (uint8_t *)( malloc(mmax_x_ * mmax_y_) );
    if(a_minimap_ == NULL) {
        FSERR(Log::k_FLG_MEM, "MiniMap", "MiniMap", ("memory allocation failed"));
        return false;
    }
    for (unsigned short y = 0; y < mmax_y_; y++) {
        unsigned short yadd = y * mmax_x_;
        for (unsigned short x = 0; x < mmax_x_; x++) {
            fs_eng::Tile::EType type = p_map->getTileAt(x, y, 0)->type();
            a_minimap_[x + yadd] = minimap_colours[type];
        }
    }

    return true;
}

/*!
 * Return the color at the given point.
 * \param x
 * \param y
 */
uint8_t MiniMap::getColourAt(int x, int y) {

    if (x < 0 || y < 0 || x >= mmax_x_ || y >= mmax_y_)
        return 0;

    if (a_minimap_ != 0)
        return a_minimap_[x + y * mmax_x_];
    return 0;
}

//! Defines a source on the minimap for the signal
void MiniMap::setTarget(MapObject *pTarget) {
    p_target_ = pTarget;
}
//! Clear the target source
void MiniMap::clearTarget() {
    p_target_ = NULL;
}

}