/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
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

#ifndef EDITOR_MISSIONEDITORMENU_H_
#define EDITOR_MISSIONEDITORMENU_H_

#include "fs-engine/menus/menu.h"
#include "fs-engine/menus/menumanager.h"
#include "maprenderer.h"

/*!
 * The mission editor menu allows the display of a mission and map.
 */
class MissionEditorMenu : public fs_eng::Menu {
public:
    MissionEditorMenu(fs_eng::MenuManager *m);
    virtual ~MissionEditorMenu();

    bool handleBeforeShow() override;
    void handleRender() override;
    void handleLeave() override;

    bool handleTick(uint32_t elapsed) override;

protected:
    void initWorldCoords();

    bool handleUnMappedKey(const fs_eng::FS_Key key) override;

    void handleMouseMotion(Point2D point, uint32_t state) override;
    bool handleMouseDown(Point2D point, int button) override;
    void handleMouseUp(Point2D point, int button) override;

    //! Handles the user's click on the map
    void handleClickOnMap(Point2D point, int button);

    //! Scroll the map horizontally.
    bool scrollOnX(Point2D point);
    //! Scroll the map vertically.
    bool scrollOnY(Point2D point);
    int isMousePositionScrollonX(Point2D point);
    int isMousePositionScrollonY(Point2D point);

protected:
    fs_knl::Mission *mission_;
    /*! This is a projection in 2D of a point on a the map. This point represents the top
     * left corner of the screen and it moves inside the map's borders.*/
    Point2D displayOriginPt_;
    Point2D scroll_;
    /*! This renderer is in charge of drawing the map.*/
    MapRenderer map_renderer_;
    //! The palette of colors used for this mission
    fs_eng::Palette missionPalette_;
};

#endif // EDITOR_MISSIONEDITORMENU_H_
