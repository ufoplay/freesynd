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

#ifndef MAP_H
#define MAP_H

#include "fs-utils/common.h"
#include "fs-engine/enginecommon.h"
#include "fs-engine/gfx/tilemanager.h"
#include "fs-kernel/model/position.h"
#include "fs-kernel/model/mapobject.h"

namespace fs_knl {

/*!
 * Map class.
 */
class Map {
public:
    Map(fs_eng::TileManager *tileManager, uint16_t anId);
    ~Map();

    /**
     * @name Map attibutes
     */
    ///@{
    uint16_t id() { return id_; }
    int maxX() { return max_x_; }
    int maxY() { return max_y_; }
    int maxZ() { return max_z_; }
    int width() { return map_width_; }
    int height() { return map_height_; }
    fs_eng::TileManager * getTileManager() { return tileManager_; }
    ///@}

    /**
     * @name Map initialization
     */
    ///@{
    //! Set the array of tiles in the map
    void setTiles(int maxX, int maxY, int maxZ, fs_eng::Tile **tiles);
    //! Set the limits for scrolling over the map
    void setScrollLimits(Point2D minScrollTile, Point2D  maxScrollTile);
    ///@}

    void mapDimensions(int *x, int *y, int *z);
    //! Clip x,y,z to map dimensions
    void adjXYZ(int &x, int &y, int &z);
    //! Clip x and y to map dimensions.
    void clip(Point2D *point);
    //! Clip x, y and z to map dimensions.
    void clip(TilePoint *point);

    /*!
     * @brief Compare the given point with the minimum tile for this map
     * @param point 
     * @return True is tx coordinate of the point is less than min tx
     */
    bool isScrollMinLimitHitOnTx(const fs_knl::TilePoint &point) {
        return point.tx < minScrollTile_.x;
    }
    /*!
     * @brief Compare the given point with the minimum tile for this map
     * @param point 
     * @return True is ty coordinate of the point is less than min ty
     */
    bool isScrollMinLimitHitOnTy(const fs_knl::TilePoint &point) {
        return point.ty < minScrollTile_.y;
    }
    /*!
     * @brief Compare the given point with the maximum tile for this map
     * @param point 
     * @return True is tx coordinate of the point is greater than max Tx
     */
    bool isScrollMaxLimitHitOnTx(const fs_knl::TilePoint &point) {
        return point.tx > maxScrollTile_.x;
    }
    /*!
     * @brief Compare the given point with the maximum tile for this map
     * @param point 
     * @return True is ty coordinate of the point is greater than max Ty
     */
    bool isScrollMaxLimitHitOnTy(const fs_knl::TilePoint &point) {
        return point.ty > maxScrollTile_.y;
    }

    //! Check that given point is whithin scroll limit and change if necessary
    void clipToScrollLimits(fs_knl::TilePoint &point);

    //! Converts a Map tile position to a screen position
    void tileToScreenPoint(const TilePoint &tPt, Point2D *pScp);
    //! Converts a screen position in pixel into a Map tile position
    TilePoint screenToTilePoint(int x, int y);

    int maxZAt(int x, int y);

    fs_eng::Tile * getTileAt(int x, int y, int z);
    //! Return the tile at given position. Only uses tx, ty and tz
    fs_eng::Tile * getTileAt(const TilePoint &tilePt);
    int tileAt(int x, int y, int z);
    void patchMap(int x, int y, int z, uint8_t tileNum);
    //! Return true if tile at given position is traversable by car
    bool isTileWalkableByCar(int x, int y, int z);

protected:
    /*!  Every map has a unique ID which is used to identify the
    name of the file containing map data.*/
    uint16_t id_;
    int max_x_, max_y_, max_z_;
    fs_eng::Tile **a_tiles_;
    fs_eng::TileManager *tileManager_;
    int map_width_, map_height_;
    /*!
     * This is the coordinate of the tile that the game viewport cannot cross on the top left
     * when scrolling.
     * Its value is given in tile on X and Y axis and it comes from the game file in MapInfo.
     */
    Point2D minScrollTile_;
    /*!
     * This is the coordinate of the tile that the game viewport cannot cross on the bottom right
     * when scrolling.
     * Its value is given in tile on X and Y axis and it comes from the game file in MapInfo.
     */
    Point2D maxScrollTile_;
};

/*!
 * A MiniMap is a small representation of the real map.
 */
class MiniMap {
public:
    /*! Constant for the minimap overlay : no overlay */
    static const uint8_t kOverlayNone;
    /*! Constant for the minimap overlay : the agent is our. */
    static const uint8_t kOverlayOurAgent;
    /*! Constant for the minimap overlay : this is an enemy agent. */
    static const uint8_t kOverlayEnemyAgent;

    MiniMap();
    ~MiniMap();

    /*! Returns the map width in tiles.*/
    int max_x() { return mmax_x_;}
    /*! Returns the map height in tiles.*/
    int max_y() { return mmax_y_;}

    bool init(Map *p_map);

    uint8_t getColourAt(int x, int y);

    //! Defines a source on the minimap for the signal
    void setTarget(MapObject *pTarget);
    //! Return the curent target. May be null
    MapObject * target() { return p_target_; }
    //! Clear the target source
    void clearTarget();

private:
    /* An array with the same size of the real map but containing
     a color for each type of tile. */
    uint8_t *a_minimap_;
    /* Size of the minimap (same as the map).*/
    int mmax_x_;
    /* Height of the minimap (same as the map).*/
    int mmax_y_;
    /*! Current target emitting a signal.*/
    MapObject *p_target_;
};

}
#endif
