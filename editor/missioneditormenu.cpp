/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
 *   Copyright (C) 2015, 2024-2025  Benoit Blancard <benblan@users.sourceforge.net>
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

#include "missioneditormenu.h"

#include "fs-engine/gfx/animationmanager.h"
#include "fs-kernel/model/ped.h"
#include "fs-kernel/model/vehicle.h"
#include "fs-kernel/model/static.h"
#include "editormenuid.h"
#include "editorcontroller.h"

using fs_eng::MenuManager;
using fs_eng::Menu;
using fs_eng::FontManager;

// The number of pixel of a scroll
const int kScrollStep = 16;

MissionEditorMenu::MissionEditorMenu(MenuManager * m):
    Menu(m, fs_edit_menus::kMenuIdMissionEditor, fs_edit_menus::kMenuIdMain),
    targetHovered_(nullptr), targetSelected_(nullptr), currentTile_(nullptr) {
    isCachable_ = false;
    cursorOnShow_ = kGameplayCursor; 
    currentTilePos_.reset();
}

MissionEditorMenu::~MissionEditorMenu() {
   
}

bool MissionEditorMenu::handleBeforeShow() {
    int missionId = g_editorCtrl.getMissionResultList().back();
    mission_ = g_missionCtrl.loadMission(missionId, 2);

    initWorldCoords();

    missionPalette_ = mission_->map()->getTileManager()->getPalette();
    g_AnimMgr.setPalette(missionPalette_);

    maxZDesc_ = std::format("Z = {}/{}", mission_->map()->maxTz(), mission_->map()->maxTz());

    menu_manager_->resetSinceMouseDown();

    return true;
}

void MissionEditorMenu::handleRender() {
    mapRenderer_.render();
    g_System.drawFillRect({0,0}, 129, fs_eng::kScreenHeight, menu_manager_->kMenuColorBlack);
    drawCurrentTileSelector();
}

void MissionEditorMenu::handleLeave() {
    g_missionCtrl.destroyMission();
}

/*!
 * Initialize the screen position centered on the squad leader.
 */
void MissionEditorMenu::initWorldCoords() {
    fs_knl::TilePoint center;

    // get the first PedInstance position on the map
    for (size_t i=0; i < mission_->numPeds(); i++) {
        if (mission_->ped(i) != nullptr) {
            fs_knl::PedInstance *pPed = mission_->ped(i);
            center.initFrom({pPed->tileX(),
                                pPed->tileY(),
                                mission_->mmax_z_ + 1,
                                0, 0});
            break;
        }
    }
    
    Point2D start;
    mission_->map()->tileToScreenPoint(center, &start);
    start.x -= (fs_eng::kScreenWidth - 129) / 2;
    start.y -= fs_eng::kScreenHeight / 2;

    if (start.x < 0)
        start.x = 0;

    if (start.y < 0)
        start.y = 0;

    // Check if the position is within map borders
    fs_knl::TilePoint mpt = mission_->map()->screenToTilePoint(start.x, start.y);

    mission_->map()->clipToScrollLimits(mpt);

    // recalculating new screen coords
    fs_knl::TilePoint newPoint(mpt.tx,
                                mpt.ty,
                                mission_->mmax_z_ + 1, 
                                0, 0);
   

    mapRenderer_.init(mission_, newPoint);
}

bool MissionEditorMenu::handleUnMappedKey(const fs_eng::FS_Key key) {
    bool consumed = true;

    if (key.keyCode == fs_eng::kKeyCode_Left) { // Scroll the map to the left
         if (g_System.isKeyModStatePressed(fs_eng::KMD_ALT)) {
            if (currentTile_ && currentTilePos_.ty < mission_->map()->maxTy()) { // move selected tile
                currentTilePos_.ty++;
                selectCurrentTile(currentTilePos_);
            }
        } else {
            scroll_.x = -kScrollStep;
        }
    } else if (key.keyCode == fs_eng::kKeyCode_Right) { // Scroll the map to the right
        if (g_System.isKeyModStatePressed(fs_eng::KMD_ALT)) {
            if (currentTile_ && currentTilePos_.ty > 0) { // move selected tile
                currentTilePos_.ty--;
                selectCurrentTile(currentTilePos_);
            }
        }  else {
            scroll_.x = kScrollStep;
        }
    } else if (key.keyCode == fs_eng::kKeyCode_Up) { 
        if (g_System.isKeyModStatePressed(fs_eng::KMD_CTRL)) {
            if (currentTile_ && currentTilePos_.tz < mission_->map()->maxTz()) { // select tile above current
                currentTilePos_.tz++;
                selectCurrentTile(currentTilePos_);
            }
        } else if (g_System.isKeyModStatePressed(fs_eng::KMD_ALT)) {
            if (currentTile_ && currentTilePos_.tx > 0) { // move selected tile
                currentTilePos_.tx--;
                selectCurrentTile(currentTilePos_);
            }
        } else { // Scroll the map to the top
            scroll_.y = -kScrollStep;
        }
    } else if (key.keyCode == fs_eng::kKeyCode_Down) { 
        if (g_System.isKeyModStatePressed(fs_eng::KMD_CTRL)) {
            if (currentTile_ && currentTilePos_.tz > 0) { // select tile below current
                currentTilePos_.tz--;
                selectCurrentTile(currentTilePos_);
            }
        } else if (g_System.isKeyModStatePressed(fs_eng::KMD_ALT)) {
            if (currentTile_ && currentTilePos_.tx < mission_->map()->maxTx()) { // select tile below current
                currentTilePos_.tx++;
                selectCurrentTile(currentTilePos_);
            }
        } else { // Scroll the map to the bottom
            scroll_.y = kScrollStep;
        }
    } else if (key.keyCode == fs_eng::kKeyCode_PageUp) { // Increase max Z for drawing
        maxZDesc_ = std::format("Z = {}/{}", mapRenderer_.incrMaxTztoDraw(), mission_->map()->maxTz());
    } else if (key.keyCode == fs_eng::kKeyCode_PageDown) { // Decrease max Z for drawing
        maxZDesc_ = std::format("Z = {}/{}", mapRenderer_.decrMaxTztoDraw(), mission_->map()->maxTz());
    } else if (key.keyCode == fs_eng::kKeyCode_Home) { // Decrease max Z to minimum
        maxZDesc_ = std::format("Z = {}/{}", mapRenderer_.setMaxTztoDrawToMin(), mission_->map()->maxTz());
    } else if (key.keyCode == fs_eng::kKeyCode_End) { // Decrease max Z to minimum
        maxZDesc_ = std::format("Z = {}/{}", mapRenderer_.setMaxTztoDrawToMax(), mission_->map()->maxTz());
    } else {
        consumed = false;
    }

    return consumed;
}

int MissionEditorMenu::isMousePositionScrollonX(Point2D point) {
    if (point.x < 5) {
        return -1;
    } else if (point.x > fs_eng::kScreenWidth - 5) {
        return 1;
    }

    return 0;
}

int MissionEditorMenu::isMousePositionScrollonY(Point2D point) {
    if (point.y < 5) {
        return -1;
    } else if (point.y > fs_eng::kScreenHeight - 5) {
        return 1;
    }

    return 0;
}

bool MissionEditorMenu::handleTick([[maybe_unused]] uint32_t elapsed) {
    Point2D mousePos;
    g_System.getMousePos(mousePos);
    // Scroll the map
    if (scroll_.x != 0) {
        mapRenderer_.scrollOnX(scroll_.x);
        scroll_.x = isMousePositionScrollonX(mousePos) * kScrollStep;
    }

    if (scroll_.y != 0) {
        mapRenderer_.scrollOnY(scroll_.y);
        scroll_.y = isMousePositionScrollonY(mousePos) * kScrollStep;
    }

    updateCursorFromTarget(mousePos);

    return true;
}

bool MissionEditorMenu::handleMouseDown(Point2D point, int button)
{
    if (point.x > 129) {
        handleClickOnMap(point, button);
    }
    return true;
}

void MissionEditorMenu::handleMouseMotion(Point2D point, [[maybe_unused]] uint32_t state) {
    scroll_.x = isMousePositionScrollonX(point) * kScrollStep;
    scroll_.y = isMousePositionScrollonY(point) * kScrollStep;
}

void MissionEditorMenu::handleClickOnMap(Point2D point, [[maybe_unused]] int button) {
    if (targetHovered_) {
        targetDesc_ = std::format("{} ({})", targetHovered_->natureName(), targetHovered_->id());
        targetLocDescXYZ_ = std::format("At {}, {}, {}", 
                                    targetHovered_->position().tx, targetHovered_->position().ty, targetHovered_->position().tz);
        targetLocDescOXYZ_ = std::format("- {}, {}, {}", 
                                    targetHovered_->position().ox, targetHovered_->position().oy, targetHovered_->position().oz);
        selectCurrentTile(targetHovered_->position());
    } else {
        /*fs_knl::TilePoint mapPt = mission_->map()->screenToTilePoint(displayOriginPt_.x + point.x - 129,
                    displayOriginPt_.y + point.y);*/
        fs_knl::TilePoint mapPt = mapRenderer_.getTilePointFromMouse(point);
        printf("Base tile : %d, %d, %d, %d, %d, %d\n", mapPt.tx, mapPt.ty, mapPt.tz, mapPt.ox, mapPt.oy, mapPt.oz);

        if (!mission_->findWalkableTileFromBase(mapPt)) {
            printf("Did not found walkable tile\n");
        }
        selectCurrentTile(mapPt);
    }

    return;
}

/*!
 * @brief Set the selected tile type with a readable name
 * @param tileType 
 */
void MissionEditorMenu::setTileTypeName(fs_eng::Tile::EType tileType) {
    std::string typeAsStr;
    switch (tileType) {
    case fs_eng::Tile::kNone:
        typeAsStr = "None";
        break;
    case fs_eng::Tile::kSlopeSN:
        typeAsStr = "SlopeSN";
        break;
    case fs_eng::Tile::kSlopeNS:
        typeAsStr = "SlopeNS";
        break;
    case fs_eng::Tile::kSlopeEW:
        typeAsStr = "SlopeEW";
        break;
    case fs_eng::Tile::kSlopeWE:
        typeAsStr = "SlopeWE";
        break;
    case fs_eng::Tile::kGround:
        typeAsStr = "Ground";
        break;
    case fs_eng::Tile::kRoadSideEW:
        typeAsStr = "RoadSideEW";
        break;
    case fs_eng::Tile::kRoadSideWE:
        typeAsStr = "RoadSideWE";
        break;
    case fs_eng::Tile::kRoadSideSN:
        typeAsStr = "RoadSideSN";
        break;
    case fs_eng::Tile::kRoadSideNS:
        typeAsStr = "RoadSideNS";
        break;
    case fs_eng::Tile::kWall:
        typeAsStr = "Wall";
        break;
    case fs_eng::Tile::kRoadCurve:
        typeAsStr = "RoadCurve";
        break;
    case fs_eng::Tile::kHandrailLight:
        typeAsStr = "HandrailLight";
        break;
    case fs_eng::Tile::kRoof:
        typeAsStr = "Roof";
        break;
    case fs_eng::Tile::kRoadPedCross:
        typeAsStr = "RoadPedCross";
        break;
    case fs_eng::Tile::kRoadMark:
        typeAsStr = "RoadMark";
        break;
    case fs_eng::Tile::kTrainStop:
        typeAsStr = "TrainStop";
        break;
    default:
        typeAsStr = "Undefined";
        break;
    }
    tileTypeDesc_ = std::format("is {}", typeAsStr);
}

void MissionEditorMenu::selectCurrentTile(const fs_knl::TilePoint &tilePt) {
    currentTile_ = mission_->map()->getTileAt(tilePt);

    currentTilePos_.tx = tilePt.tx;
    currentTilePos_.ty = tilePt.ty;
    currentTilePos_.tz = tilePt.tz;

    tileDesc_ = std::format("Tile {}", currentTile_->id());
    setTileTypeName(currentTile_->type());
    locationDesc_ = std::format("At {}, {}, {}", currentTilePos_.tx, currentTilePos_.ty, currentTilePos_.tz);
}

void MissionEditorMenu::updateCursorFromTarget(Point2D point) {
    targetHovered_ = nullptr;
    if (point.x > 128) {
        for (size_t i = mission_->getSquad()->size(); mission_ && i < mission_->numPeds(); ++i) {
            fs_knl::PedInstance *p = mission_->ped(i);
            if (p->isAlive() && p->isDrawable()) {
                Point2D topLeftPt = { 10,
                    (1 + p->tileZ()) * fs_eng::Tile::kTileHeight/3 - 
                    (p->offZ() * fs_eng::Tile::kTileHeight/3) / 128};
            
                if (mapRenderer_.isMouseHovering(point, *p, topLeftPt, {21, 34})) {
                    // mouse pointer is on the object, so it's the new target
                    targetHovered_ = p;
                    break;
                }
            }
        }

        if (targetHovered_ == nullptr) {
            for (size_t i = 0; mission_ && i < mission_->numVehicles(); ++i) {
                fs_knl::Vehicle *v = mission_->vehicle(i);
                // TrainHead cannot be selected to prevent player from putting agents in it
                if (v->isAlive() && v->getType() != fs_knl::Vehicle::kVehicleTypeTrainHead) {
                    Point2D topLeftPt = { 20, 10 + v->tileZ() * fs_eng::Tile::kTileHeight/3};
            
                    if (mapRenderer_.isMouseHovering(point, *v, topLeftPt, {40, 32})) {
                        // mouse pointer is on the object, so it's the new target
                        targetHovered_ = v;
                        break;
                    }
                }
            }
        }

        if (targetHovered_ == nullptr) {
            for (size_t i = 0; mission_ && i < mission_->numWeaponsOnGround(); ++i) {
                fs_knl::WeaponInstance *w = mission_->weaponOnGround(i);

                if (w->isDrawable()) {
                    Point2D topLeftPt = { 10, -4 + w->tileZ() * fs_eng::Tile::kTileHeight/3
                        + (w->offZ() * fs_eng::Tile::kTileHeight/3) / 128};
            
                    if (mapRenderer_.isMouseHovering(point, *w, topLeftPt, {20, 15})) {
                        // mouse pointer is on the object, so it's the new target
                        targetHovered_ = w;
                        break;
                    }
                }
            }
        }

        if (targetHovered_ == nullptr) {
            for (size_t i = 0; mission_ && i < mission_->numStatics(); ++i) {
                fs_knl::Static *s = mission_->statics(i);

                if (s->isDrawable()) {
                    Point2D topLeftPt = { 10, -4 + s->tileZ() * fs_eng::Tile::kTileHeight/3
                        + (s->offZ() * fs_eng::Tile::kTileHeight/3) / 128};
            
                    if (mapRenderer_.isMouseHovering(point, *s, topLeftPt, {20, 15})) {
                        // mouse pointer is on the object, so it's the new target
                        targetHovered_ = s;
                        break;
                    }
                }
            }
        }
    }

    if (targetHovered_) {
        g_System.useTargetCursor();
    } else if (point.x > 128) {
            g_System.usePointerCursor();
    } else {
            g_System.usePointerYellowCursor();
    }
}

void MissionEditorMenu::drawCurrentTileSelector() {
    if (currentTile_) {
        mapRenderer_.drawTileContour(currentTilePos_, menu_manager_->kMenuColorYellow);

        // Draw target information
        gameFont()->drawText(10, 140, targetDesc_, menu_manager_->kMenuColorLightGreen);
        gameFont()->drawText(10, 155, targetLocDescXYZ_, menu_manager_->kMenuColorLightGreen);
        gameFont()->drawText(10, 170, targetLocDescOXYZ_, menu_manager_->kMenuColorLightGreen);
        
        // Then draw the tile on the left side to better isolate it
        mission_->map()->getTileManager()->drawTile(currentTile_, 33, 285);
        gameFont()->drawText(10, 335, tileDesc_, menu_manager_->kMenuColorLightGreen);
        gameFont()->drawText(10, 350, tileTypeDesc_, menu_manager_->kMenuColorLightGreen);
        gameFont()->drawText(10, 365, locationDesc_, menu_manager_->kMenuColorLightGreen);
    }

    gameFont()->drawText(10, 380, maxZDesc_, menu_manager_->kMenuColorLightGreen);
}
