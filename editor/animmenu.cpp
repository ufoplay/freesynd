/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
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

#include "animmenu.h"

#include <format>

#include "fs-engine/menus/menumanager.h"
#include "fs-engine/system/system.h"
#include "fs-engine/gfx/animationmanager.h"

#include "editormenuid.h"
#include "editorcontroller.h"

using fs_eng::MenuManager;
using fs_eng::Menu;
using fs_eng::FontManager;

AnimMenu::AnimMenu(MenuManager * m)
        : Menu(m, fs_edit_menus::kMenuIdFont, fs_edit_menus::kMenuIdMain, true) {
    isCachable_ = false;
    cursorOnShow_ = kMenuCursor;
    animId_ = 0;
    frameId_ = 0;

    addStatic(0, 40, fs_eng::kScreenWidth, "ANIMATIONS - TILES", FontManager::SIZE_4, false);
    // Accept button
    addOption(17, 347, 128, 25, "BACK", FontManager::SIZE_2, fs_edit_menus::kMenuIdMain);

    // Animation id
    addStatic(50, 100, "ANIMATION:", FontManager::SIZE_2, true);
    pAnimIdTF_ = addTextField(60, 245, 40, 21, FontManager::SIZE_2, 4, false, true);
    pAnimIdTF_->setText("0");
    txtTotalAnimations_ = g_AnimMgr.getNumAnims();
    std::string s = std::format("/{}", txtTotalAnimations_);
    addStatic(130, 248, s.c_str(), FontManager::SIZE_2, true);

    // frame id
    addStatic(50, 280, "FRAME:", FontManager::SIZE_2, true);
    txtFrameId_ = addStatic(145, 280, "0", FontManager::SIZE_2, true);
    
    txtTotalFrames_ = addStatic(160, 280, "", FontManager::SIZE_2, true);
    updateTotalFrames();

    // Tile id
    tileId_ = 0;
    addStatic(390, 100, "TILE:", FontManager::SIZE_2, true);
    pTileIdTF_ = addTextField(450, 245, 40, 21, FontManager::SIZE_2, 3, false, true);
    pTileIdTF_->setText("0");
    std::string tileCount = std::format("/{}", fs_eng::TileManager::kNumOfTiles - 1);
    addStatic(500, 248, tileCount.c_str(), FontManager::SIZE_2, true);
}

void AnimMenu::handleRender() {
    // Rectangle around anim textfield
    g_System.drawRect({58, 245}, 70, 23, menu_manager_->kMenuColorLightGreen);
    // Draw rect for animation
    g_System.drawFillRect({50, 130}, 150, 100, menu_manager_->kMenuColorWhite);
    Point2D pos = {100, 200};
    // Draw frame animation
    g_AnimMgr.drawFrame(animId_, frameId_, pos);

    // Rectangle around tile textfield
    g_System.drawRect({448, 245}, 50, 23, menu_manager_->kMenuColorLightGreen);
    // Draw rect for tile preview
    g_System.drawFillRect({390, 130}, 150, 100, menu_manager_->kMenuColorWhite);
    // Draw current tile
    fs_eng::Tile *pTile = g_editorCtrl.tileManager().getTile(tileId_);
    if (pTile != nullptr) {
        g_editorCtrl.tileManager().drawTile(pTile, 440, 160);
    }
}

void AnimMenu::handleLeave() {
    g_System.hideCursor();
}

void AnimMenu::setAnimIdText(uint16_t animId) {
    std::string str = std::to_string(animId);
    pAnimIdTF_->setText(str.c_str());
}

void AnimMenu::setTileIdText(uint8_t tileId) {
    std::string str = std::to_string(tileId);
    pTileIdTF_->setText(str.c_str());
}

void AnimMenu::changeFrameId(int newFrameId) {
    frameId_ = newFrameId;
    std::string s = std::format("{}", frameId_);
    getStatic(txtFrameId_)->setText(s.c_str());
}

void AnimMenu::updateTotalFrames() {
    std::string s = std::format("/{}", g_AnimMgr.lastFrame(animId_));
    getStatic(txtTotalFrames_)->setText(s.c_str());
}

bool AnimMenu::handleUnMappedKey(const fs_eng::FS_Key key) {
    fs_eng::TextField *pTextField = getCapturingInput();
    if (key.keyCode == fs_eng::kKeyCode_Up && pTextField != pAnimIdTF_) {
        // We don't want to scroll up while editing the field
        if (animId_ < g_AnimMgr.getNumAnims() - 1) {
            setAnimIdText(++animId_);
            changeFrameId(0);
            updateTotalFrames();
        }
        return true;
    } else if (key.keyCode == fs_eng::kKeyCode_PageUp && pTextField != pTileIdTF_) {
        if (tileId_ < static_cast<uint8_t>(fs_eng::TileManager::kNumOfTiles - 1)) {
            setTileIdText(++tileId_);
        }
        return true;
    } else if (key.keyCode == fs_eng::kKeyCode_Down && pTextField != pAnimIdTF_) {
        // We don't want to scroll down while editing the field
        if (animId_ > 0) {
            setAnimIdText(--animId_);
            changeFrameId(0);
            updateTotalFrames();
        }
        return true;
    } else if (key.keyCode == fs_eng::kKeyCode_PageDown && pTextField != pTileIdTF_) {
        if (tileId_ > 0) {
            setTileIdText(--tileId_);
        }
        return true;
    } else if (key.keyCode == fs_eng::kKeyCode_Right) {
        changeFrameId(frameId_ + 1);
        if (frameId_ > g_AnimMgr.lastFrame(animId_)) {
            changeFrameId(0);
        }
        return true;
    } else if (key.keyCode == fs_eng::kKeyCode_Left) {
        changeFrameId(frameId_ - 1);
        if (frameId_ < 0) {
            changeFrameId(g_AnimMgr.lastFrame(animId_));
        }
        return true;
    } else if (key.keyCode == fs_eng::kKeyCode_Return) {
        if (pTextField == pAnimIdTF_) {
            try {
                uint16_t newAnimId = static_cast<uint16_t> (std::stoi(pTextField->getText()));
                if (newAnimId < txtTotalAnimations_) {
                    animId_ = newAnimId;
                    changeFrameId(0);
                    updateTotalFrames();
                    captureInputBy(nullptr);
                }
            } catch (...) {
                // don't do anything
            }
        } else if (pTextField == pTileIdTF_) {
            try {
                int val = std::stoi(pTextField->getText());
                if (val >= 0 && val < fs_eng::TileManager::kNumOfTiles) {
                    tileId_ = static_cast<uint8_t>(val);
                    captureInputBy(nullptr);
                }
            } catch (...) {
                // don't do anything
            }
        }
        return true;
    } else if (key.keyCode == fs_eng::kKeyCode_Escape) {
        if (pTextField == pAnimIdTF_) {
            setAnimIdText(animId_);
            captureInputBy(nullptr);
            return true;
        } else if (pTextField == pTileIdTF_) {
            setTileIdText(tileId_);
            captureInputBy(nullptr);
            return true;
        }
    }

    return false;
}

