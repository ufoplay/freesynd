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
    target_(nullptr)
{
    isCachable_ = false;
    cursorOnShow_ = kGameplayCursor; 
}

MissionEditorMenu::~MissionEditorMenu() {
   
}

bool MissionEditorMenu::handleBeforeShow() {
    int missionId = g_editorCtrl.getMissionResultList().back();
    mission_ = g_missionCtrl.loadMission(missionId, 2);

    initWorldCoords();

    missionPalette_ = mission_->get_map()->getTileManager()->getPalette();
    g_AnimMgr.setPalette(missionPalette_);

    map_renderer_.init(mission_);

    menu_manager_->resetSinceMouseDown();

    return true;
}

void MissionEditorMenu::handleRender() {
    map_renderer_.render(displayOriginPt_);
    g_System.drawFillRect({0,0}, 129, fs_eng::kScreenHeight, menu_manager_->kMenuColorBlack);
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
    mission_->get_map()->tileToScreenPoint(center, &start);
    start.x -= (fs_eng::kScreenWidth - 129) / 2;
    start.y -= fs_eng::kScreenHeight / 2;

    if (start.x < 0)
        start.x = 0;

    if (start.y < 0)
        start.y = 0;

    // Check if the position is within map borders
    fs_knl::TilePoint mpt = mission_->get_map()->screenToTilePoint(start.x, start.y);

    if (mpt.tx < mission_->minX())
        mpt.tx = mission_->minX();

    if (mpt.ty < mission_->minY())
        mpt.ty = mission_->minY();

    if (mpt.tx > mission_->maxX())
        mpt.tx = mission_->maxX();

    if (mpt.ty > mission_->maxY())
        mpt.ty = mission_->maxY();

    // recalculating new screen coords
    fs_knl::TilePoint newPoint(mpt.tx,
                                mpt.ty,
                                mission_->mmax_z_ + 1, 
                                0, 0);
    Point2D msp;
    mission_->get_map()->tileToScreenPoint(newPoint, &msp);
    displayOriginPt_.x = msp.x;
    displayOriginPt_.y = msp.y;
}

bool MissionEditorMenu::handleUnMappedKey(const fs_eng::FS_Key key) {
    bool consumed = true;

    if (key.keyCode == fs_eng::kKeyCode_Left) { // Scroll the map to the left
        scroll_.x = -kScrollStep;
    } else if (key.keyCode == fs_eng::kKeyCode_Right) { // Scroll the map to the right
        scroll_.x = kScrollStep;
    } else if (key.keyCode == fs_eng::kKeyCode_Up) { // Scroll the map to the top
        scroll_.y = -kScrollStep;
    } else if (key.keyCode == fs_eng::kKeyCode_Down) { // Scroll the map to the bottom
        scroll_.y = kScrollStep;
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

/*!
 * Scroll the map horizontally.
 * Each map has a min and max value for the world origin coords and this
 * method moves that point between those limits. If scrolling hits the
 * map border, the scrolling is made along that border.
 * \return True is a scroll is made
 */
bool MissionEditorMenu::scrollOnX(Point2D mousePos) {
    bool change = false;

    int newOriginX = displayOriginPt_.x + scroll_.x;

    fs_knl::TilePoint mpt = mission_->get_map()->screenToTilePoint(newOriginX, displayOriginPt_.y);

    // Scroll to the right
    if (scroll_.x > 0) {
        if (mpt.ty < mission_->minY()) {
            // we hit the upper right border of the map
            // so we scroll down until the far right corner
            int newWorldY = displayOriginPt_.y + kScrollStep;
            newOriginX += kScrollStep;
            mpt = mission_->get_map()->screenToTilePoint(newOriginX, newWorldY);

            if (mpt.ty < mission_->minY() || mpt.tx > mission_->maxX()) {
                // We hit the corner so don't scroll
                return false;
            } else {
                displayOriginPt_.x = newOriginX;
                displayOriginPt_.y = newWorldY;
                change = true;
            }
        } else if (mpt.tx > mission_->maxX()) {
            // we hit the lower right border of the map
            // so we scroll up until the far right corner
            int newWorldY = displayOriginPt_.y - kScrollStep;
            newOriginX += kScrollStep;
            mpt = mission_->get_map()->screenToTilePoint(newOriginX, newWorldY);

            if (mpt.ty < mission_->minY() || mpt.tx > mission_->maxX()) {
                return false;
            } else {
                displayOriginPt_.x = newOriginX;
                displayOriginPt_.y = newWorldY;
                change = true;
            }
        } else {
            // This is a regular right scroll
            displayOriginPt_.x = newOriginX;
            change = true;
        }

    } else { // Scroll to the left
        if (mpt.tx < mission_->minX()) {
            // we hit the upper left border of the map
            // so we scroll down until the far left corner
            int newWorldY = displayOriginPt_.y + kScrollStep;
            newOriginX -= kScrollStep;
            mpt = mission_->get_map()->screenToTilePoint(newOriginX, newWorldY);

            if (mpt.tx < mission_->minX() || mpt.ty > mission_->maxY()) {
                return false;
            } else {
                displayOriginPt_.x = newOriginX;
                displayOriginPt_.y = newWorldY;
                change = true;
            }
        } else if (mpt.ty > mission_->maxY()) {
            // we hit the lower left border of the map
            // so we scroll up until the far left corner
            int newWorldY = displayOriginPt_.y - kScrollStep;
            newOriginX -= kScrollStep;
            mpt = mission_->get_map()->screenToTilePoint(newOriginX, newWorldY);

            if (mpt.tx < mission_->minX() || mpt.ty > mission_->maxY()) {
                return false;
            } else {
                displayOriginPt_.x = newOriginX;
                displayOriginPt_.y = newWorldY;
                change = true;
            }
        } else {
            displayOriginPt_.x = newOriginX;
            change = true;
        }
    }

    if (!isMousePositionScrollonX(mousePos)) {
        scroll_.x = 0;
    }

    return change;
}

/*!
 * Scroll the map vertically.
 * Each map has a min and max value for the world origin coords and this
 * method moves that point between those limits. If scrolling hits the
 * map border, the scrolling is made along that border.
 * \return True is a scroll is made
 */
bool MissionEditorMenu::scrollOnY(Point2D mousePos) {
    bool change = false;

    int newWorldY = displayOriginPt_.y + scroll_.y;

    fs_knl::TilePoint mpt = mission_->get_map()->screenToTilePoint(displayOriginPt_.x, newWorldY);

    // Scroll down
    if (scroll_.y > 0) {
        if (mpt.tx > mission_->maxX()) {
            // we hit the lower right border of the map
            // so we scroll down until the lower corner
            int newOriginX = displayOriginPt_.x - 2*kScrollStep;
            mpt = mission_->get_map()->screenToTilePoint(newOriginX, newWorldY);

            if (mpt.ty > mission_->maxY() || mpt.tx > mission_->maxX()) {
                return false;
            } else {
                displayOriginPt_.x = newOriginX;
                displayOriginPt_.y = newWorldY;
                change = true;
            }
        } else if (mpt.ty > mission_->maxY()) {
            // we hit the lower left border of the map
            // so we scroll down until the lower corner
            int newOriginX = displayOriginPt_.x + 2*kScrollStep;
            mpt = mission_->get_map()->screenToTilePoint(newOriginX, newWorldY);

            if (mpt.ty > mission_->maxY() || mpt.tx > mission_->maxX()) {
                return false;
            } else {
                displayOriginPt_.x = newOriginX;
                displayOriginPt_.y = newWorldY;
                change = true;
            }
        } else {
            displayOriginPt_.y = newWorldY;
            change = true;
        }

    } else { // Scroll up
        if (mpt.tx < mission_->minX()) {
            // we hit the upper right border of the map
            // so we scroll up until the upper corner
            int newOriginX = displayOriginPt_.x + 2*kScrollStep;
            mpt = mission_->get_map()->screenToTilePoint(newOriginX, newWorldY);

            if (mpt.ty < mission_->minY() || mpt.tx < mission_->minX()) {
                return false;
            } else {
                displayOriginPt_.x = newOriginX;
                displayOriginPt_.y = newWorldY;
                change = true;
            }
        } else if (mpt.ty < mission_->minY()) {
            // we hit the upper left border of the map
            // so we scroll up until the upper corner
            int newOriginX = displayOriginPt_.x - 2*kScrollStep;
            mpt = mission_->get_map()->screenToTilePoint(newOriginX, newWorldY);

            if (mpt.ty < mission_->minY() || mpt.tx < mission_->minX()) {
                return false;
            } else {
                displayOriginPt_.x = newOriginX;
                displayOriginPt_.y = newWorldY;
                change = true;
            }
        } else {
            displayOriginPt_.y = newWorldY;
            change = true;
        }
    }

    if (!isMousePositionScrollonY(mousePos)) {
        scroll_.y = 0;
    }

    return change;
}

bool MissionEditorMenu::handleTick(uint32_t elapsed) {
    bool change = false;
    Point2D mousePos;
    g_System.getMousePos(mousePos);
    // Scroll the map
    if (scroll_.x != 0) {
        change = scrollOnX(mousePos);
    }

    if (scroll_.y != 0) {
        change = scrollOnY(mousePos);
    }

    updateTarget(mousePos);

    if (target_) {
        g_System.useTargetCursor();
    } else if (mousePos.x > 128) {
            g_System.usePointerCursor();
    } else {
            g_System.usePointerYellowCursor();
    }

    return true;
}

bool MissionEditorMenu::handleMouseDown(Point2D point, int button)
{
    return true;
}

void MissionEditorMenu::handleMouseMotion(Point2D point, [[maybe_unused]] uint32_t state) {
    scroll_.x = isMousePositionScrollonX(point) * kScrollStep;
    scroll_.y = isMousePositionScrollonY(point) * kScrollStep;
}

void MissionEditorMenu::handleMouseUp([[maybe_unused]] Point2D point, int button)
{}

void MissionEditorMenu::handleClickOnMap(Point2D point, int button) {}

void MissionEditorMenu::updateTarget(Point2D point) {
    target_ = nullptr;
    if (point.x > 128) {
        for (size_t i = mission_->getSquad()->size(); mission_ && i < mission_->numPeds(); ++i) {
            fs_knl::PedInstance *p = mission_->ped(i);
            if (p->isAlive() && p->isDrawable()) {
                Point2D scPt;
                mission_->get_map()->tileToScreenPoint(p->position(), &scPt);
                int px = scPt.x - 10;
                int py = scPt.y - (1 + p->tileZ()) * fs_eng::Tile::kTileHeight/3
                    - (p->offZ() * fs_eng::Tile::kTileHeight/3) / 128;

                if (point.x - 129 + displayOriginPt_.x >= px && point.y + displayOriginPt_.y >= py &&
                    point.x - 129 + displayOriginPt_.x < px + 21 && point.y + displayOriginPt_.y < py + 34)
                {
                    // mouse pointer is on the object, so it's the new target
                    target_ = p;
                    break;
                }
            }
        }

        if (target_ == nullptr) {
            for (size_t i = 0; mission_ && i < mission_->numVehicles(); ++i) {
                fs_knl::Vehicle *v = mission_->vehicle(i);
                // TrainHead cannot be selected to prevent player from putting agents in it
                if (v->isAlive() && v->getType() != fs_knl::Vehicle::kVehicleTypeTrainHead) {
                    Point2D scPt;
                    mission_->get_map()->tileToScreenPoint(v->position(), &scPt);
                    int px = scPt.x - 20;
                    int py = scPt.y - 10 - v->tileZ() * fs_eng::Tile::kTileHeight/3;

                    if (point.x - 129 + displayOriginPt_.x >= px && point.y + displayOriginPt_.y >= py &&
                        point.x - 129 + displayOriginPt_.x < px + 40 && point.y + displayOriginPt_.y < py + 32)
                    {
                        target_ = v;
                        break;
                    }
                }
            }
        }

        if (target_ == nullptr) {
            for (size_t i = 0; mission_ && i < mission_->numWeaponsOnGround(); ++i) {
                fs_knl::WeaponInstance *w = mission_->weaponOnGround(i);

                if (w->isDrawable()) {
                    Point2D scPt;
                    mission_->get_map()->tileToScreenPoint(w->position(), &scPt);
                    int px = scPt.x - 10;
                    int py = scPt.y + 4 - w->tileZ() * fs_eng::Tile::kTileHeight/3
                        - (w->offZ() * fs_eng::Tile::kTileHeight/3) / 128;

                    if (point.x - 129 + displayOriginPt_.x >= px && point.y + displayOriginPt_.y >= py &&
                        point.x - 129 + displayOriginPt_.x < px + 20 && point.y + displayOriginPt_.y < py + 15)
                    {
                        target_ = w;
                        break;
                    }
                }
            }
        }

        if (target_ == nullptr) {
            for (size_t i = 0; mission_ && i < mission_->numStatics(); ++i) {
                fs_knl::Static *s = mission_->statics(i);

                if (s->isDrawable()) {
                    Point2D scPt;
                    mission_->get_map()->tileToScreenPoint(s->position(), &scPt);
                    int px = scPt.x - 10;
                    int py = scPt.y + 4 - s->tileZ() * fs_eng::Tile::kTileHeight/3
                        - (s->offZ() * fs_eng::Tile::kTileHeight/3) / 128;

                    if (point.x - 129 + displayOriginPt_.x >= px && point.y + displayOriginPt_.y >= py &&
                        point.x - 129 + displayOriginPt_.x < px + 20 && point.y + displayOriginPt_.y < py + 15)
                    {
                        target_ = s;
                        break;
                    }
                }
            }
        }
    }
}
