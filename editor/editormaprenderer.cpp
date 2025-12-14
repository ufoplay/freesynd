/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
 *   Copyright (C) 2006  Tarjei Knapstad <tarjei.knapstad@gmail.com>
 *   Copyright (C) 2010  Bohdan Stelmakh <chamel@users.sourceforge.net>
 *   Copyright (C) 2013, 2024-2025  Benoit Blancard <benblan@users.sourceforge.net>
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

#include "editormaprenderer.h"

#include "fs-engine/gfx/tilemanager.h"
#include "fs-engine/system/system.h"
#include "fs-kernel/model/mission.h"
#include "fs-kernel/model/vehicle.h"
#include "fs-kernel/model/squad.h"
#include "fs-kernel/mgr/agentmanager.h"

#include "fs-engine/config.h"

const int EditorMapRenderer::kGameplayPanelWidth = 129;

void EditorMapRenderer::init(fs_knl::Mission *pMission, const fs_knl::TilePoint &center) {
    pMission_ = pMission;
    pMap_ = pMission->map();
    maxTztoDraw_ = pMap_->maxTz();

    pMap_->tileToScreenPoint(center, &viewportOriginPt_);
}

int EditorMapRenderer::incrMaxTztoDraw() {
    if (maxTztoDraw_ < pMap_->maxTz()) {
        maxTztoDraw_++;
    }

    return maxTztoDraw_;
}

int EditorMapRenderer::decrMaxTztoDraw() {
    if (maxTztoDraw_ > 0) {
        maxTztoDraw_--;
    }

    return maxTztoDraw_;
}

/*!
 * @brief Set maximum z level to draw to zero
 * @return the new max level
 */
int EditorMapRenderer::setMaxTztoDrawToMin() {
    maxTztoDraw_ = 0;
    return maxTztoDraw_;
}

/*!
 * @brief Set maximum z level to draw to maxTz
 * @return the new max level
 */
int EditorMapRenderer::setMaxTztoDrawToMax() {
    maxTztoDraw_ = pMap_->maxTz();
    return maxTztoDraw_;
}

/**
 * Draw tiles and map objects.
 */
void EditorMapRenderer::render() {
    // TODO: list of bugs to fix in rendering
    //  - Some advert panels lack a corner
    fs_knl::TilePoint mtp = pMap_->screenToTilePoint(viewportOriginPt_.x, viewportOriginPt_.y);
    int sw = mtp.tx;
    int chk = fs_eng::kScreenWidth / (fs_eng::Tile::kTileWidth / 2) + 2
        + fs_eng::kScreenHeight / (fs_eng::Tile::kTileHeight / 3) + pMap_->maxTz() * 2;
    int sh = mtp.ty - 8;

    int shm = sh + chk;

    DEBUG_SPEED_INIT

    listObjectsToDraw(viewportOriginPt_);

    int cmw = viewportOriginPt_.x + fs_eng::kScreenWidth -
                kGameplayPanelWidth + 128;
    int cmh = viewportOriginPt_.y + fs_eng::kScreenHeight + 128;
    int cmx = viewportOriginPt_.x - kGameplayPanelWidth;
     //  z = 0 - is minimap data and mapdata
    int chky = sh < 0 ? 0 : sh;
    int zr = shm + pMap_->maxTz() + 1;
    for (int inc = 0; inc < zr; ++inc) {
        int ye = sh + inc;
        int ys = ye - pMap_->maxTz() - 2;
        int tile_z = pMap_->maxTz() + 1;  // the Z coord of the next tile to draw
        for (int yb = ys; yb < ye; ++yb) {
            if (yb < 0 || yb < sh || yb >= shm) {
                --tile_z;
                continue;
            }
            int tile_y = yb;  // The Y coord of the tile to draw
            for (int tile_x = sw; tile_y >= chky && tile_x < pMap_->maxTx(); ++tile_x) {
                if (tile_x < 0 || tile_y >= pMap_->maxTy()) {
                    --tile_y;
                    continue;
                }
                int screen_w = (pMap_->maxTx() + (tile_x - tile_y)) * (fs_eng::Tile::kTileWidth / 2);
                int coord_h = ((pMap_->maxTz() + tile_x + tile_y) - (tile_z - 1)) * (fs_eng::Tile::kTileHeight / 3);
                if (screen_w >= viewportOriginPt_.x - fs_eng::Tile::kTileWidth * 2
                    && screen_w + fs_eng::Tile::kTileWidth * 2 < cmw
                    && coord_h >= viewportOriginPt_.y - fs_eng::Tile::kTileHeight * 2
                    && coord_h + fs_eng::Tile::kTileHeight * 2 < cmh) {
#if 0
                    if (z > 2)
                        continue;
#endif
                    // draw a tile
                    if (tile_z <= maxTztoDraw_) {
                        fs_eng::Tile *pTile = pMap_->getTileAt(tile_x, tile_y, tile_z);
                        if (pTile->notTransparent()) {
                            int dx = 0, dy = 0;
                            if (screen_w - viewportOriginPt_.x < 0)
                                dx = -(screen_w - viewportOriginPt_.x);
                            if (coord_h - viewportOriginPt_.y < 0)
                                dy = -(coord_h - viewportOriginPt_.y);
                            if (dx < fs_eng::Tile::kTileWidth && dy < fs_eng::Tile::kTileHeight) {
                                pMap_->getTileManager()->drawTile(pTile, screen_w - cmx, coord_h - viewportOriginPt_.y);
                            }
                        }
                    }

                    // draw everything that's on the tile
                    if (tile_z - 1 >= 0) {
                        fs_knl::TilePoint currentTile(tile_x, tile_y, tile_z - 1);
                        Point2D screenPos = {screen_w - cmx + fs_eng::Tile::kTileWidth / 2,
                            coord_h - viewportOriginPt_.y + fs_eng::Tile::kTileHeight / 3 * 2};

                        drawObjectsOnTile(currentTile, screenPos);
                    }
                }
                --tile_y;
            }
            --tile_z;
        }
    }

    freeUnreleasedResources();

#ifdef _DEBUG
    /*if (g_System.getKeyModState() & fs_eng::KMD_LALT) {
        fs_eng::FSColor yellow {227, 219, 40, 0xFF};
        for (SquadSelection::Iterator it = pSelection_->begin();
            it != pSelection_->end(); ++it) {
            (*it)->showPath(viewportOriginPt_.x, viewportOriginPt_.y, yellow);
        }
    }*/
#endif

    DEBUG_SPEED_LOG("EditorMapRenderer::render")
}

int EditorMapRenderer::tileHashKey(fs_knl::MapObject * m) {
    return tileHashKey(m->position());
}

void EditorMapRenderer::listObjectsToDraw(const Point2D &viewport) {
    /*if (tilex < 0)
        tilex = 0;
    if (tiley < 0)
        tiley = 0;
    if (maxtilex >= pMap_->maxX())
        maxtilex = pMap_->maxX();
    if (maxtiley >= pMap_->maxY())
        maxtiley = pMap_->maxY();*/


    // Include peds
    for (size_t i = 0; i < pMission_->numPeds(); i++) {
        fs_knl::PedInstance *pPed = pMission_->ped(i);
        if (pPed->isDrawable() && isObjectInsideDrawingArea(pPed, viewport)) {
            addObjectToDraw(pPed);
        }
    }

    // vehicles
    for (size_t i = 0; i < pMission_->numVehicles(); i++) {
        fs_knl::Vehicle *pVehicle = pMission_->vehicle(i);
        if (isObjectInsideDrawingArea(pVehicle, viewport)) {
            addObjectToDraw(pVehicle);
        }
    }

    // weapons
    for (size_t i = 0; i < pMission_->numWeaponsOnGround(); i++) {
        fs_knl::WeaponInstance *pWeapon = pMission_->weaponOnGround(i);
        if (pWeapon->isDrawable() && isObjectInsideDrawingArea(pWeapon, viewport)) {
            addObjectToDraw(pWeapon);
        }
    }

    // statics
    for (size_t i = 0; i < pMission_->numStatics(); i++) {
        fs_knl::Static *pStatic = pMission_->statics(i);
        if (isObjectInsideDrawingArea(pStatic, viewport)) {
            addObjectToDraw(pStatic);
        }
    }

    // sfx objects
    for (size_t i = 0; i < pMission_->numSfxObjects(); i++) {
        fs_knl::SFXObject *pSfx = pMission_->sfxObjects(i);
        if (pSfx->isDrawable() && isObjectInsideDrawingArea(pSfx, viewport)) {
            addObjectToDraw(pSfx);
        }
    }
}

/**
 * Return true if the object appears on the screen and so should be drawn.
 * \param pObject MapObject*
 * \param viewport const Point2D&
 * \return bool
 *
 */
bool EditorMapRenderer::isObjectInsideDrawingArea(fs_knl::MapObject *pObject, const Point2D &viewport) {
    Point2D objectViewport;
    pMap_->tileToScreenPoint(pObject->position(), &objectViewport);

    // Limits are larger than screen size in order to have a smooth display
    // of appearance/disappearance of objects on screen. Otherwise they popup when
    // entering the display screen.
    return  objectViewport.x > (viewport.x - fs_eng::Tile::kTileWidth / 2) && objectViewport.y > viewport.y &&
            objectViewport.x <= (viewport.x + fs_eng::kScreenWidth - kGameplayPanelWidth + 10) &&
            objectViewport.y <= (viewport.y + fs_eng::kScreenHeight + pObject->position().tz * 48);
}

/**
 * Draw all objects on the given tile.
 * \param tilePos const TilePoint& tile coordinates
 * \param screenPos const Point2D& position of tile on the screen
 * \return int number of objects for debug
 *
 */
int EditorMapRenderer::drawObjectsOnTile(const fs_knl::TilePoint & tilePos, const Point2D &screenPos) {
    int tileKey = tileHashKey(tilePos);
    int nbDrawnObjects = 0;

    std::map<int, ObjectToDraw *>::iterator it = objectsByTile_.find(tileKey);
    if(it != objectsByTile_.end()) {
        ObjectToDraw *pObj = it->second;
        objectsByTile_.erase(it);
        while(pObj != NULL) {
            pObj->getObject()->draw(screenPos);
            ObjectToDraw *pNext = pObj->getNext();
            pool_.releaseResource(pObj);
            pObj = pNext;
            nbDrawnObjects++;
        }
    }

    return nbDrawnObjects;
}


/**
 * Adds an object to the list of objects to draw for the tile it's on.
 * For a given tile object are sorted from back to front so that
 * objects in the back are drawn first.
 * \param pObjectToAdd MapObject* Object to add
 * \return void
 *
 */
void EditorMapRenderer::addObjectToDraw(fs_knl::MapObject *pObjectToAdd) {
    int tileKey;
    ObjectToDraw *pNewEntry = pool_.getResource();
    pNewEntry->setObject(pObjectToAdd);

    if (pObjectToAdd->is(fs_knl::MapObject::kNatureVehicle)) {
        // vehicle are associated with the tile just above (z+1)
        // because it is bigger than a tile so all tiles below must be drawn first
        fs_knl::TilePoint vehiclePos( pObjectToAdd->position());
        vehiclePos.tz += 1;
        tileKey = tileHashKey(vehiclePos);
    } else {
        tileKey = tileHashKey(pObjectToAdd);
    }

    std::map<int, ObjectToDraw *>::iterator element = objectsByTile_.find(tileKey);
    if(element == objectsByTile_.end()) {
        // no element has been set with the tile so add the first element
        objectsByTile_[tileKey] = pNewEntry;
    } else {
        // there is at leastone element already set with the tile
        ObjectToDraw *pObjectInList = element->second;
        if (pObjectToAdd->isBehindObjectOnSameTile(pObjectInList->getObject())) {
            // first case is when the new object should be first in the list
            pNewEntry->setNext(pObjectInList);
            objectsByTile_[tileKey] = pNewEntry;
        } else {
            // second case is when new object is somewhere in the list
            while (pObjectInList != NULL) {
                if (pObjectInList->getNext() != NULL) {
                    if (pObjectToAdd->isBehindObjectOnSameTile(pObjectInList->getNext()->getObject())) {
                        pObjectInList->insertNext(pNewEntry);
                        break;
                    } else {
                        pObjectInList = pObjectInList->getNext();
                    }
                } else {
                    pObjectInList->setNext(pNewEntry);
                    break;
                }
            }
        }
    }
}

/**
 * Objects that were listed for drawing may be bigger than objects really drawn.
 * So remove those objects from the list.
 * \return void
 *
 */
void EditorMapRenderer::freeUnreleasedResources() {
    int nbFreed = 0;
    std::map<int, ObjectToDraw *>::iterator itr = objectsByTile_.begin();
    while (itr != objectsByTile_.end()) {
        std::map<int, ObjectToDraw *>::iterator toErase = itr;
        ++itr;
        ObjectToDraw *pObj = toErase->second;
        while(pObj != NULL) {
            ObjectToDraw *pNext = pObj->getNext();
            pool_.releaseResource(pObj);
            nbFreed++;
            pObj = pNext;
        }
        objectsByTile_.erase(toErase);
    }
}

/*!
 * Scroll the map horizontally.
 * Each map has a min and max value for the world origin coords and this
 * method moves that point between those limits. If scrolling hits the
 * map border, the scrolling is made along that border.
 */
void EditorMapRenderer::scrollOnX(int scrollAmount) {
    int newOriginX = viewportOriginPt_.x + scrollAmount;

    fs_knl::TilePoint mpt = pMap_->screenToTilePoint(newOriginX, viewportOriginPt_.y);

    // Scroll to the right
    if (scrollAmount > 0) {
        if (pMap_->isScrollMinLimitHitOnTy(mpt)) {
            // we hit the upper right border of the map
            // so we scroll down until the far right corner
            int newWorldY = viewportOriginPt_.y + scrollAmount;
            newOriginX += scrollAmount;
            mpt = pMap_->screenToTilePoint(newOriginX, newWorldY);

            if (pMap_->isScrollMinLimitHitOnTy(mpt) || pMap_->isScrollMaxLimitHitOnTx(mpt)) {
                // We hit the corner so don't scroll
                return;
            } else {
                viewportOriginPt_.x = newOriginX;
                viewportOriginPt_.y = newWorldY;
            }
        } else if (pMap_->isScrollMaxLimitHitOnTx(mpt)) {
            // we hit the lower right border of the map
            // so we scroll up until the far right corner
            int newWorldY = viewportOriginPt_.y - scrollAmount;
            newOriginX += scrollAmount;
            mpt = pMap_->screenToTilePoint(newOriginX, newWorldY);

            if (pMap_->isScrollMinLimitHitOnTy(mpt) || pMap_->isScrollMaxLimitHitOnTx(mpt)) {
                return;
            } else {
                viewportOriginPt_.x = newOriginX;
                viewportOriginPt_.y = newWorldY;
            }
        } else {
            // This is a regular right scroll
            viewportOriginPt_.x = newOriginX;
        }

    } else { // Scroll to the left
        if (pMap_->isScrollMinLimitHitOnTx(mpt)) {
            // we hit the west border of the map
            // so we scroll toward south border
            int newWorldY = viewportOriginPt_.y - scrollAmount;
            newOriginX += scrollAmount;
            mpt = pMap_->screenToTilePoint(newOriginX, newWorldY);

            if (pMap_->isScrollMinLimitHitOnTx(mpt) || pMap_->isScrollMaxLimitHitOnTy(mpt)) {
                return;
            } else {
                viewportOriginPt_.x = newOriginX;
                viewportOriginPt_.y = newWorldY;
            }
        } else if (pMap_->isScrollMaxLimitHitOnTy(mpt)) {
            // we hit the south border of the map
            // so we scroll towards the west border
            int newWorldY = viewportOriginPt_.y + scrollAmount;
            newOriginX += scrollAmount;
            mpt = pMap_->screenToTilePoint(newOriginX, newWorldY);

            if (pMap_->isScrollMinLimitHitOnTx(mpt) || pMap_->isScrollMaxLimitHitOnTy(mpt)) {
                return;
            } else {
                viewportOriginPt_.x = newOriginX;
                viewportOriginPt_.y = newWorldY;
            }
        } else {
            viewportOriginPt_.x = newOriginX;
        }
    }
}

/*!
 * Scroll the map vertically.
 * Each map has a min and max value for the world origin coords and this
 * method moves that point between those limits. If scrolling hits the
 * map border, the scrolling is made along that border.
 */
void EditorMapRenderer::scrollOnY(int scrollAmount) {
    int newWorldY = viewportOriginPt_.y + scrollAmount;

    fs_knl::TilePoint mpt = pMap_->screenToTilePoint(viewportOriginPt_.x, newWorldY);

    // Scroll down
    if (scrollAmount > 0) {
        if (pMap_->isScrollMaxLimitHitOnTx(mpt)) {
            // we hit the lower right border of the map
            // so we scroll down until the lower corner
            int newOriginX = viewportOriginPt_.x - 2*scrollAmount;
            mpt = pMap_->screenToTilePoint(newOriginX, newWorldY);

            if (pMap_->isScrollMaxLimitHitOnTy(mpt) || pMap_->isScrollMaxLimitHitOnTx(mpt)) {
                return;
            } else {
                viewportOriginPt_.x = newOriginX;
                viewportOriginPt_.y = newWorldY;
            }
        } else if (pMap_->isScrollMaxLimitHitOnTy(mpt)) {
            // we hit the lower left border of the map
            // so we scroll down until the lower corner
            int newOriginX = viewportOriginPt_.x + 2*scrollAmount;
            mpt = pMap_->screenToTilePoint(newOriginX, newWorldY);

            if (pMap_->isScrollMaxLimitHitOnTy(mpt) || pMap_->isScrollMaxLimitHitOnTx(mpt)) {
                return;
            } else {
                viewportOriginPt_.x = newOriginX;
                viewportOriginPt_.y = newWorldY;
            }
        } else {
            viewportOriginPt_.y = newWorldY;
        }

    } else { // Scroll up
        if (pMap_->isScrollMinLimitHitOnTx(mpt)) {
            // we hit the west border of the map
            // so we scroll towards the south border
            int newOriginX = viewportOriginPt_.x - 2*scrollAmount;
            mpt = pMap_->screenToTilePoint(newOriginX, newWorldY);

            if (pMap_->isScrollMinLimitHitOnTy(mpt) || pMap_->isScrollMinLimitHitOnTx(mpt)) {
                return;
            } else {
                viewportOriginPt_.x = newOriginX;
                viewportOriginPt_.y = newWorldY;
            }
        } else if (pMap_->isScrollMinLimitHitOnTy(mpt)) {
            // we hit the upper left border of the map
            // so we scroll up until the upper corner
            int newOriginX = viewportOriginPt_.x + 2*scrollAmount;
            mpt = pMap_->screenToTilePoint(newOriginX, newWorldY);

            if (pMap_->isScrollMinLimitHitOnTy(mpt) || pMap_->isScrollMinLimitHitOnTx(mpt)) {
                return;
            } else {
                viewportOriginPt_.x = newOriginX;
                viewportOriginPt_.y = newWorldY;
            }
        } else {
            viewportOriginPt_.y = newWorldY;
        }
    }
}

fs_knl::TilePoint EditorMapRenderer::getTilePointFromMouse(const Point2D &mousePt) {
    return pMap_->screenToTilePoint(viewportOriginPt_.x + mousePt.x - kGameplayPanelWidth,
                    viewportOriginPt_.y + mousePt.y);
}

/*!
 * @brief 
 * @param mousePt 
 * @param mapObject 
 * @param padTopLeft 
 * @param padSize 
 * @return 
 */
bool EditorMapRenderer::isMouseHovering(const Point2D &mousePt, const fs_knl::MapObject &mapObject, const Point2D &padTopLeft, const Point2D &padSize) {
    Point2D scPt;
    pMap_->tileToScreenPoint(mapObject.position(), &scPt);

    Point2D topLeftPt = scPt.add(
                                kGameplayPanelWidth - viewportOriginPt_.x - padTopLeft.x,
                                -viewportOriginPt_.y - padTopLeft.y);
    
    return (mousePt.x >= topLeftPt.x && 
            mousePt.y >= topLeftPt.y &&
            mousePt.x < (topLeftPt.x + padSize.x) &&
            mousePt.y < (topLeftPt.y + padSize.y));
}

/*!
 * @brief Draw a contour around the tile on the map to clearly see it
 * @param tilePoint 
 * @param color 
 */
void EditorMapRenderer::drawTileContour(const fs_knl::TilePoint &tilePoint, fs_eng::FSColor color) {
    Point2D tileTop;
    
    pMap_->tileToScreenPoint(tilePoint, &tileTop);
    tileTop = tileTop.add(-viewportOriginPt_.x + kGameplayPanelWidth, -viewportOriginPt_.y - tilePoint.tz * fs_eng::Tile::kSubTileHeight);
    
    g_System.drawLine(tileTop,
                        tileTop.add(fs_eng::Tile::kSubTileWidth, fs_eng::Tile::kSubTileHeight ),
                        color);

    g_System.drawLine(tileTop,
                        tileTop.add(-fs_eng::Tile::kSubTileWidth, fs_eng::Tile::kSubTileHeight),
                        color);

    g_System.drawLine(tileTop.add(0, fs_eng::Tile::kSubTileHeight*2),
                        tileTop.add(fs_eng::Tile::kSubTileWidth, fs_eng::Tile::kSubTileHeight),
                        color);
    
    g_System.drawLine(tileTop.add(0, fs_eng::Tile::kSubTileHeight*2),
                        tileTop.add(-fs_eng::Tile::kSubTileWidth, fs_eng::Tile::kSubTileHeight),
                        color);

    tileTop = tileTop.add(0, fs_eng::Tile::kSubTileHeight);
    g_System.drawLine(tileTop.add(0, fs_eng::Tile::kSubTileHeight*2),
                        tileTop.add(fs_eng::Tile::kSubTileWidth, fs_eng::Tile::kSubTileHeight),
                        color);
    
    g_System.drawLine(tileTop.add(0, fs_eng::Tile::kSubTileHeight*2),
                        tileTop.add(-fs_eng::Tile::kSubTileWidth, fs_eng::Tile::kSubTileHeight),
                        color);
}