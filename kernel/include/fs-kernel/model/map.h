/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
 *   Copyright (C) 2024-2026  Benoit Blancard <benblan@users.sourceforge.net>
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
    // forbiddenDir values: encode "from which direction we came" to prevent U-turns.
    static const uint16_t kForbidDirSouth = 0x0000; ///< Came from South — no going back South
    static const uint16_t kForbidDirEast  = 0x0020; ///< Came from East  — no going back East
    static const uint16_t kForbidDirNorth = 0x0400; ///< Came from North — no going back North
    static const uint16_t kForbidDirWest  = 0x6000; ///< Came from West  — no going back West

    static const uint8_t kConnexionMaskEntryNorth; ///< Used to test if we can enter the tile by north
    static const uint8_t kConnexionMaskEntrySouth; ///< Used to test if we can enter the tile by south 
    static const uint8_t kConnexionMaskEntryEast; ///< Used to test if we can enter the tile by east
    static const uint8_t kConnexionMaskEntryWest; ///< Used to test if we can enter the tile by west

    static const uint8_t kConnexionMaskExitNorth; ///< Used to test if we can exit the tile by north
    static const uint8_t kConnexionMaskExitSouth; ///< Used to test if we can exit the tile by south
    static const uint8_t kConnexionMaskExitEast; ///< Used to test if we can exit the tile by east
    static const uint8_t kConnexionMaskExitWest; ///< Used to test if we can exit the tile by west
    
public:
    Map(fs_eng::TileManager *tileManager, uint16_t anId);
    ~Map();

    /**
     * @name Map attibutes
     */
    ///@{
    uint16_t id() { return id_; }
    //! Return the maximum tiles on Tx dimension
    int maxTx() const { return maxTx_; }
    int maxTy() const { return maxTy_; }
    int maxTz() const { return maxTz_; }

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
    //!
    bool isWithinMapBounds(const TilePoint &point) const;
    //!
    TilePoint projectToZLevel(const TilePoint &point, int targetZ);

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
    //! Return the id of the tile at given position
    int getTileIdAt(int tx, int ty, int tz);

    void patchMap(int x, int y, int z, uint8_t tileNum);

    /**
     * @name Road related methods
     */
    ///@{
    //! Return true if tile at given position is traversable by car
    bool isTileWalkableByCar(int x, int y, int z);
    //! Return a bitmask indicating what directions are possible when leaving this tile
    uint8_t getPossibleConnexionsForRoadTile(const TilePoint &tilePt);
    //! Return true if this points to a road tile and adjust the point in some cases
    bool adjustClickOnRoad(TilePoint &tilePt);
    ///@}

protected:
    /*!  Every map has a unique ID which is used to identify the
    name of the file containing map data.*/
    uint16_t id_;
    //! Maximum tiles for each dimension
    int maxTx_, maxTy_, maxTz_;
    fs_eng::Tile **a_tiles_;
    fs_eng::TileManager *tileManager_;
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
