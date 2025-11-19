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

#include "editorapp.h"
#include "editormenuid.h"

using fs_eng::MenuManager;
using fs_eng::Menu;
using fs_eng::FontManager;

MissionEditorMenu::MissionEditorMenu(MenuManager * m):
    Menu(m, fs_edit_menus::kMenuIdMissionEditor, fs_edit_menus::kMenuIdMain)
{
    isCachable_ = false;
    cursorOnShow_ = kMenuCursor;    
}

MissionEditorMenu::~MissionEditorMenu() {
   
}

bool MissionEditorMenu::handleBeforeShow() {
    int missionId = g_editorCtrl.getMissionResultList().back();

    printf("Mission id  = %d\n", missionId);

    return true;
}

void MissionEditorMenu::handleRender() {}

void MissionEditorMenu::handleLeave() {}
