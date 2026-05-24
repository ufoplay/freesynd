/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
 *   Copyright (C) 2006  Tarjei Knapstad <tarjei.knapstad@gmail.com>
 *   Copyright (C) 2010  Bohdan Stelmakh <chamel@users.sourceforge.net>
 *   Copyright (C) 2010, 2024-2026  Benoit Blancard <benblan@users.sourceforge.net>
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

#include "fs-kernel/model/mission.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <string>
#include <random>
#include <algorithm>

#include "tracy/Tracy.hpp"

#include "fs-utils/log/log.h"
#include "fs-engine/events/event.h"
#include "fs-engine/gfx/tile.h"
#include "fs-kernel/model/shot.h"
#include "fs-kernel/model/objectivedesc.h"
#include "fs-kernel/model/vehicle.h"
#include "fs-kernel/model/squad.h"

namespace {
    // Générateur de nombres aléatoires thread-safe
    thread_local std::mt19937 rng(std::random_device{}());
}

namespace fs_knl {

const uint8_t Mission::kBMaskBlockerTargetInRange = 0x01;
const uint8_t Mission::kBMaskBlockerTargetOutOfMap = 0x20;
const uint8_t Mission::kBMaskBlockerTargetObjectUpdated = 0x02;
const uint8_t Mission::kBMaskBlockerTargetPosUpdated = 0x04;
const uint8_t Mission::kBMaskBlockerBlockedByTile = 0x10;

/*!
 * Initialize the statistics.
 * \param nbAgents Number of agents for the mission
 */
void MissionStats::init(size_t nbAgents) {
    nbAgents_ = nbAgents;
    missionDuration_ = 0;
    nbAgentCaptured_ = 0;
    enemyKilled_ = 0;
    criminalKilled_ = 0;
    civilKilled_ = 0;
    policeKilled_ = 0;
    guardKilled_ = 0;
    convinced_ = 0;
    nbOfShots_ = 0;
    nbOfHits_ = 0;
}

/*!
 * Mission constructor
 * @param map_infos 
 * @param pMap 
 */
Mission::Mission() :
        cur_objective_(0),
        p_map_(nullptr), p_minimap_(nullptr),
        squad_(std::make_unique<Squad>()) {
    status_ = kMissionStatusRunning;
    mtsurfaces_ = NULL;
    mdpoints_ = NULL;
    mdpoints_cp_ = NULL;
}

Mission::~Mission()
{
    for (unsigned int i = 0; i < vehicles_.size(); i++)
        delete vehicles_[i];
    for (unsigned int i = 0; i < peds_.size(); i++)
        delete peds_[i];
    for (unsigned int i = 0; i < weaponsOnGround_.size(); i++)
        delete weaponsOnGround_[i];
    
    sfx_objects_.clear();
    projectileShots_.clear();
    statics_.clear();

    for (unsigned int i = 0; i < objectives_.size(); i++)
        delete objectives_[i];
    armedPedsVec_.clear();
    clrNavigationGraph();

    if (p_minimap_) {
        delete p_minimap_;
    }
}

/*!
 * Sets the given map for the mission.
 * If p_map is not null, creates a minimap from it.
 * @param map_infos 
 * @param pMap The map to set.
 * @return True if everything is ok
 */
bool Mission::init(Map *pMap) {
    if (!pMap) {
        FSERR(Log::k_FLG_GAME, "Mission", "init", ("Map is null"));
        return false;
    }

    p_map_ = pMap;
    p_map_->mapDimensions(&mmax_x_, &mmax_y_, &mmax_z_);
    mmax_m_xy = mmax_x_ * mmax_y_;

    if (p_minimap_) {
        delete p_minimap_;
    }
    // TODO : change with new init method
    p_minimap_ = new MiniMap();

    return p_minimap_->init(p_map_);
}

/*!
 * Add the given Static to the list of objects to animate
 * @param aStatic 
 */
void Mission::addStatic(std::unique_ptr<Static> aStatic) { 
    statics_.push_back(std::move(aStatic));
}

/*!
 * Adds the given ProjectileShot to the list of animated shots.
 * @param shot The projectile to add
 */
void Mission::addProjectileShot(std::unique_ptr<ProjectileShot> shot) {
    projectileShots_.push_back(std::move(shot));
}

/*!
 * Add an new instance of SfxObject.
 * If object is visible, start its animation.
 * @param so The SfxObject to manage
 */
void Mission::addSfxObject(std::unique_ptr<SFXObject> so) {
    if (so->isDrawable()) {
        so->playMainAnimation();
    }
    sfx_objects_.push_back(std::move(so));
}

/*!
 * Removes given ped from the list of armed peds.
 * \param pPed The ped to remove
 */
void Mission::removeArmedPed(PedInstance *pPed) {
    std::erase_if(armedPedsVec_, [pPed](PedInstance* ped) {
        return ped == pPed;
    });
}

/*!
 * Sets the given message with the current objective label.
 */
void Mission::objectiveMsg(std::string& msg) {
    if (cur_objective_ < objectives_.size()) {
        msg = objectives_[cur_objective_]->msg;
    } else {
        msg = "";
    }
}

/**
 * Iterates over the given weapon list and identifies the two weapons
 * with the highest rank values.
 *
 * @param weapons List of available weapons to select from.
 * @return A pair of indices {bestWeaponIndex, secondBestWeaponIndex}.
 *         If no weapons are available, indices are set to -1.
 */
std::pair<int, int> Mission::findTopTwoWeapons(const std::vector<Weapon*>& weapons) {
    int indexBest = -1, indexSecond = -1;
    int rankBest = -1, rankSecond = -1;

    for (size_t i = 0; i < weapons.size(); ++i) {
        int weaponRank = weapons[i]->rank();
        if (weaponRank > rankBest) {
            rankSecond = rankBest;
            indexSecond = indexBest;
            rankBest = weaponRank;
            indexBest = static_cast<int>(i);
        } else if (weaponRank > rankSecond) {
            rankSecond = weaponRank;
            indexSecond = static_cast<int>(i);
        }
    }
    return {indexBest, indexSecond};
}

/**
 * For each enemy agent without any weapon, this function assigns either the best-ranked
 * or second-best-ranked weapon (randomly with a small chance) and optionally a time bomb
 * if one is available.
 *
 * @param weapons List of available weapons.
 * @param bomb Pointer to a bomb weapon (can be nullptr if not available).
 */
void Mission::assignWeaponsToEnemyAgents(const std::vector<Weapon*>& weapons, Weapon* bomb) {
    std::uniform_int_distribution<int> distribution(0, 255);

    auto [indexBest, indexSecond] = findTopTwoWeapons(weapons);

    for (size_t i = squad_->size(); i < peds_.size(); ++i) {
        auto* ped = peds_[i];
        if (ped->objGroupDef() == PedInstance::og_dmAgent && ped->numWeapons() == 0) {
            int indexToGive = indexBest;

            if (indexSecond != -1 && distribution(rng) > 200) {
                indexToGive = indexSecond;
            }

            if (indexToGive != -1 && static_cast<size_t>(indexToGive) < weapons.size()) {
                auto* weaponInstance = WeaponInstance::createInstance(weapons[indexToGive]);
                ped->addWeapon(weaponInstance);
                weaponInstance->setOwner(ped);
            }

            if (bomb) {
                auto* bombInstance = WeaponInstance::createInstance(bomb);
                ped->addWeapon(bombInstance);
                bombInstance->setOwner(ped);
            }
        }
    }
}

/**
 * Initializes mission statistics, prepares available weapons,
 * and assigns weapons to enemy agents.
 *
 * @param weaponMgr WeaponManager instance providing access to available weapons.
 */
void Mission::start(WeaponManager& weaponMgr) {
    // TODO: consider weight of weapons when adding?
    // TODO: check whether enemy agents weapons are equal to best two

    LOG(Log::k_FLG_GAME, "Mission", "start()", ("Start mission"));
    // Reset mission statistics
    stats_.init(squad_->size());

    cur_objective_ = 0;

    // Get available weapons
    std::vector<Weapon*> availableWeapons;
    weaponMgr.getAvailable(kDmgTypeBullet, availableWeapons);

    // see if bomb is available
    Weapon* bomb = weaponMgr.getAvailable(Weapon::TimeBomb);

    // Assign best weapons to enemy agents
    assignWeaponsToEnemyAgents(availableWeapons, bomb);
}


/*!
 * Run animate method for each managed object.
 * @param elapsed 
 * @param diff 
 */
void Mission::handleTick(uint32_t elapsed, uint32_t diff) {
    ZoneScoped;
    
    buildDynamicSpatialGrid();

    for (auto it = sfx_objects_.begin(); it != sfx_objects_.end(); ) {
        auto& sfx = *it;

        sfx->animate(diff);

        if (sfx->sfxLifeOver()) {
            it = sfx_objects_.erase(it);
        } else {
            ++it;
        }
    }

    for (fs_knl::PedInstance *pPed : peds_) {
        pPed->animate(diff);
    }

    for (fs_knl::Vehicle *pVehicle : vehicles_) {
        pVehicle->animate(diff);
    }

    for (fs_knl::WeaponInstance *pWeapon : weaponsOnGround_) {
        pWeapon->animate(diff);
    }

    for (auto & pStatics : statics_) {
        pStatics->animate(elapsed);
    }

    for (auto it = projectileShots_.begin(); it != projectileShots_.end(); ) {
        auto& projectileShot = *it;

        projectileShot->animate(diff, this);

        if (projectileShot->isLifeOver()) {
            // TODO Delete object 
            it = projectileShots_.erase(it);
        } else {
            ++it;
        }
    }
}

/*!
 * Checks if objectives are completed or failed and updates
 * mission status.
 */
void Mission::checkObjectives() {
    // We only check the current objective
    if (cur_objective_ < objectives_.size()) {
        ObjectiveDesc * pObj = objectives_[cur_objective_];

        // If it's the first time the objective is checked,
        // declares it started
        if (pObj->status == kNotStarted) {
            LOG(Log::k_FLG_GAME, "Mission", "checkObjectives()", ("Start objective : %d", cur_objective_));
            // An objective has just started, warn all listeners
            pObj->start();
        }

        // Checks if the objective is completed
        pObj->evaluate(this);

        if (pObj->isTerminated()) {
            if (pObj->status == kFailed) {
                endWithStatus(kMissionStatusFailed);
            } else {
                // Objective is completed -> go to next one
                cur_objective_++;
                if (cur_objective_ >= objectives_.size()) {
                    // the last objective has been completed : mission succeeded
                    endWithStatus(kMissionStatusCompleted);
                }
            }
        }
    }
}

/*!
 * Ends the mission with the given status.
 * \param status The ending status
 */
void Mission::endWithStatus(Status status) {
    status_ = status;
    EventManager::fire<MissionEndedEvent>(status);

    updateStats();
}


/** \brief
 *
 * \return void
 *
 */
void Mission::updateStats() {
    LOG(Log::k_FLG_GAME, "Mission", "updateStats()", ("calculate statistics for mission"));
    for (size_t i = squad_->size(); i < peds_.size(); i++) {
        PedInstance *p = peds_[i];
        // TODO: influence country happiness with number of killed overall
        // civilians+police, more killed less happy
        // TODO: add money per every persuaded non-agent ped
        if (p->isDead()) {
            switch (p->type()) {
                case PedInstance::kPedTypeAgent:
                    stats_.incrEnemyKilled();
                    break;
                case PedInstance::kPedTypeCriminal:
                    stats_.incrCriminalKilled();
                    break;
                case PedInstance::kPedTypeCivilian:
                    stats_.incrCivilKilled();
                    break;
                case PedInstance::kPedTypeGuard:
                    stats_.incrGuardKilled();
                    break;
                case PedInstance::kPedTypePolice:
                    stats_.incrPoliceKilled();
                    break;
            }
        } else if (p->isPersuaded()) {
            if (p->objGroupDef() == PedInstance::og_dmAgent) {
                stats_.incrAgentCaptured();
            } else {
                stats_.incrConvinced();
            }
        }
    }
}

/*!
 * Add the given weapon to the list of weapons on the ground.
 * Start the animation of the weapon
 * @param pWeapon Weapon to drop on ground
 */
void Mission::addWeaponToGround(WeaponInstance * pWeapon)
{
    for (unsigned int i = 0; i < weaponsOnGround_.size(); i++) {
        // TODO : check if == operator is used correctly (see  == in WeaponInstance)
        if (weaponsOnGround_[i] == pWeapon)
            return;
    }
    weaponsOnGround_.push_back(pWeapon);
    pWeapon->playOnGroundAnimation();
}

/*!
 * Remove the weapon from the list of weapons on the ground.
 * @param pWeapon 
 */
void Mission::removeWeaponOnGround(WeaponInstance *pWeapon) {
    for (unsigned int i = 0; i < weaponsOnGround_.size(); i++) {
        if (weaponsOnGround_[i] == pWeapon) {
            weaponsOnGround_.erase(weaponsOnGround_.begin() + i);
        }
    }
    pWeapon->resetAnimation();
}

/*!
 * This function iterates over the objects of the requested type (pedestrians or vehicles) starting
 * from the provided search index and looks for an object located at the specified tile coordinates.
 * If a matching object is found, it is returned and the search index is updated
 * so that subsequent calls can continue searching from the next object.
 *
 * @param tilex X coordinate of the tile.
 * @param tiley Y coordinate of the tile.
 * @param tilez Z level (altitude) of the tile.
 * @param nature The nature/type of the object to search for (pedestrian or vehicle).
 * @param searchIndex [in,out] Pointer to the current search index; updated if a matching object is found.
 * @return Pointer to the found object, or nullptr if no matching object is found.
 *
 * @note
 * - Dead pedestrians are included in the search to prevent logic issues (e.g., doors closing over a corpse).
 * - If an undefined nature is provided, an error is logged and the function returns nullptr.
 *
 * @see MapObject::ObjectNature
 */
MapObject * Mission::findObjectWithNatureAtPos(int tilex, int tiley, int tilez,
                            MapObject::ObjectNature nature, size_t *searchIndex) {

    const TilePoint position{tilex, tiley, tilez};

    // lambda to search for an object in a generic container that has same position
    auto findIn = [&](auto& container) -> MapObject* {
        for (size_t i = *searchIndex; i < container.size(); ++i) {
            if (container[i]->sameTile(position)) {
                *searchIndex = i + 1;
                return container[i];
            }
        }
        return nullptr;
    };

    switch (nature) {
        case MapObject::kNaturePed:
            return findIn(peds_);
        case MapObject::kNatureVehicle:
            return findIn(vehicles_);
        default:
            FSERR(Log::k_FLG_GAME, "Mission", "findObjectWithNatureAtPos", 
                    ("Undefined nature %i\n", static_cast<int>(nature)));
            return nullptr;
    }
}

// Surface walkable
bool Mission::sWalkable(SurfaceType thisTile, SurfaceType upperTile) {

    return (
            // checking surface
            ((thisTile == SurfaceType::kGround ||
                thisTile == SurfaceType::kRoad ||
            (thisTile >= SurfaceType::kRoof && thisTile <= SurfaceType::kRoadPedCross)
            || (thisTile == SurfaceType::kTrainPlatformNS || thisTile == SurfaceType::kTrainPlatformEW)))
            // or checking stairs
            || ((thisTile > SurfaceType::kEmpty && thisTile < SurfaceType::kGround))
        ) && (upperTile == SurfaceType::kEmpty || upperTile == SurfaceType::kTrainStop);
}

bool Mission::isSurface(SurfaceType thisTile) {
    return thisTile == SurfaceType::kGround ||
            thisTile == SurfaceType::kRoad ||
            (thisTile >= SurfaceType::kRoof && thisTile <= SurfaceType::kRoadPedCross)
        || (thisTile == SurfaceType::kTrainPlatformNS || thisTile == SurfaceType::kTrainPlatformEW);
}

/*!
 * Constructs the navigation data for the entire 3-D tile map in three phases:
 *
 * **Phase 1 – Surface initialisation** (`initSurface`)\n
 * Allocates the `mdpoints_` and `mtsurfaces_` arrays and copies the raw
 * walkdata from the map into `mtsurfaces_`, then patches tiles that overlap
 * large doors so they are treated as passable.
 *
 * **Phase 2 – Flood fill** (`floodFillFromSeed` / `classifyTile`)\n
 * For each live pedestrian whose starting tile has not yet been classified, a
 * flood fill is seeded from that tile.  For every visited tile the fill records:
 * - whether the tile is walkable (`kWalkable`) or not (`kNonWalkable`),
 *   and whether it is safe ground (not a road or railway) via `kSafeWalk`;
 * - reachable horizontal neighbours at the same level (`FloodNode::dirsSame`),
 *   one level up (`dirsAbove`), and one level down (`dirsBelow`), each as an 8-direction
 *   bitmask.
 *
 * Tiles whose surface type redirects traversal (type `0x00`/`0x10` fall back to
 * the tile below; types `0x11`/`0x12` are treated as the tile above) are
 * resolved before classification.  Tiles never reachable from any pedestrian
 * start position remain `kNone`.
 *
 * **Phase 3 – Static spatial grid**\n
 * Populates `staticSpatialGrid_` with all static map objects for
 * grid-accelerated collision and line-of-sight queries.  Done only once because
 * statics never move.
 *
 * @return true (always; kept for historical compatibility).
 */
bool Mission::buildNavigationGraph() {
    LOG(Log::k_FLG_GAME, "Mission", "buildNavigationGraph", ("Starting build of navigation graph"));

    clrNavigationGraph();
    int gridSize = mmax_x_ * mmax_y_ * mmax_z_;

    mtsurfaces_ = new SurfaceType[gridSize];
    mdpoints_ = new FloodNode[gridSize];
    mdpoints_cp_ = new FloodNode[gridSize];
    
    buildSurfaces();

    for (PedInstance *pPed : peds_) {
        if (pPed->tileZ() >= mmax_z_ || pPed->tileZ() < 0 || pPed->isDead()) {
            // TODO : check on all maps those peds correct position
            LOG(Log::k_FLG_GAME, "Mission", "buildNavigationGraph", ("!! Ped %d has tz (%d) superior to maxtz %d", pPed->id(), pPed->tileZ(), mmax_z_));
            pPed->setTileZ(mmax_z_ - 1);
            continue;
        }

        if (mdpoints_[getTileIndex(pPed->position())].flags == kNone) {
            floodFillFromSeed(pPed->position());
        }
    }

    // Build static spatial grid (statics never move so this is done only once).
    staticSpatialGrid_.assign(gridSize, {});
    for (const auto& s : statics_) {
        insertIntoSpatialGrid(staticSpatialGrid_, s.get());
    }

    LOG(Log::k_FLG_GAME, "Mission", "buildNavigationGraph", ("End of build of navigation graph"));

    return true;
}

void Mission::clrNavigationGraph() {

    if(mtsurfaces_ != NULL) {
        delete[] mtsurfaces_;
        mtsurfaces_ = NULL;
    }
    if(mdpoints_ != NULL) {
        delete[] mdpoints_;
        mdpoints_ = NULL;
    }
    if(mdpoints_cp_ != NULL) {
        delete[] mdpoints_cp_;
        mdpoints_cp_ = NULL;
    }
    staticSpatialGrid_.clear();
    dynamicSpatialGrid_.clear();
}

void Mission::buildSurfaces() {
    int mmax_m_all = mmax_x_ * mmax_y_ * mmax_z_;
    
    for (int ix = 0; ix < mmax_x_; ++ix) {
        for (int iy = 0; iy < mmax_y_; ++iy) {
            for (int iz = 0; iz < mmax_z_; ++iz) {
                mtsurfaces_[ix + iy * mmax_x_ + iz * mmax_m_xy] =
                    p_map_->getSurfaceTypeForTile(ix, iy, iz);
            }
        }
    }

    // to make surfaces where large doors are located walkable
    for (const auto & s : statics_) {
        if (s->type() == Static::smt_LargeDoor) {
            int indx = getTileIndex(s->position());
            mtsurfaces_[indx] = SurfaceType::kEmpty;
            if (s->orientation() == Static::kStaticOrientationNS) {
                if (indx - 1 >= 0)
                    mtsurfaces_[indx - 1] = SurfaceType::kEmpty;
                if (indx + 1 < mmax_m_all)
                    mtsurfaces_[indx + 1] = SurfaceType::kEmpty;
            } else if (s->orientation() == Static::kStaticOrientationEW) {
                if (indx - mmax_x_ >= 0)
                    mtsurfaces_[indx - mmax_x_] = SurfaceType::kEmpty;
                if (indx + mmax_x_ < mmax_m_all)
                    mtsurfaces_[indx + mmax_x_] = SurfaceType::kEmpty;
            }
        }
    }
}

/*!
 * Called if the tile at seedPt is not already classified.
 * Then it seeds the flood fill queue, iterates over reachable tiles,
 * resolves surface redirections (0x00/0x10 → lower level, 0x11/0x12 →
 * upper level), and calls classifyTile() for each resolved node.
 * @param seedPt         raw tile coordinate
 * @note Surface type `0x0D` is not handled correctly (see in-source TODO).
 */
void Mission::floodFillFromSeed(const TilePoint &seedPt) {
    int mmax_m_all = mmax_x_ * mmax_y_ * mmax_z_;
    std::vector<WorldPoint> vtodefine;
    mdpoints_[getTileIndex(seedPt)].flags = kPending;
    WorldPoint seedWPt;
    seedWPt.x = seedPt.tx;
    seedWPt.y = seedPt.ty;
    seedWPt.z = seedPt.tz;
    vtodefine.push_back(seedWPt);

    // TODO: tiles walkdata type 0x0D are quiet special, and they
    // are not handled correctly, these correction and adjustings
    // can create additional speed drain, as such I didn't
    // implemented them as needed. To make it possible a patch
    // required to walkdata and a lot of changes which I don't
    // want to do.
    // 0x10 appear above walking tile where train stops

    do {
        WorldPoint stodef = vtodefine.back();
        vtodefine.pop_back();
        int x = stodef.x;
        int y = stodef.y * mmax_x_;
        int z = stodef.z * mmax_m_xy;
        //if (x == 50 && y / mmax_x_ == 27 && z / mmax_m_xy == 2)
            //x = 50;
        SurfaceType this_s = mtsurfaces_[x + y + z];
        SurfaceType upper_s = SurfaceType::kEmpty;
        FloodNode *cfp = &(mdpoints_[x + y + z]);
        int zm = z - mmax_m_xy;
        // if current is 0x00 or 0x10 tile we will use lower tile
        // to define it
        if (this_s == SurfaceType::kEmpty || this_s == SurfaceType::kTrainStop) {
            if (zm < 0) {
                cfp->flags = kNonWalkable;
                continue;
            }
            z = zm;
            zm -= mmax_m_xy;
            upper_s = this_s;
            this_s = mtsurfaces_[x + y + z];
            if (!sWalkable(this_s, upper_s))
                continue;
        } else if (this_s == SurfaceType::kTrainPlatformNS || this_s == SurfaceType::kTrainPlatformEW) {
            int zp_tmp = z + mmax_m_xy;
            if (zp_tmp < mmax_m_all) {
                // we are defining tile above current
                cfp = &(mdpoints_[x + y + zp_tmp]);
            } else
                cfp->flags = kNonWalkable;
        }
        int xm = x - 1;
        int ym = y - mmax_x_;
        int xp = x + 1;
        int yp = y + mmax_x_;
        int zp = z + mmax_m_xy;
        if (zp < mmax_m_all) {
            upper_s = mtsurfaces_[x + y + zp];
            if(!sWalkable(this_s, upper_s)) {
                cfp->flags = kNonWalkable;
                continue;
            }
        } else {
            cfp->flags = kNonWalkable;
            continue;
        }
        classifyTile(x, y, z,
                     xm, xp, ym, yp, zm, zp,
                     this_s, mmax_m_all, cfp, vtodefine);
    } while (vtodefine.size());
}

void Mission::enqueueIfUndefined(FloodNode *fp, int x, int y, int z,
                                  std::vector<WorldPoint>& queue) {
    if (fp->flags == kNone) {
        fp->flags = kPending;
        WorldPoint pt;
        pt.x = x;
        pt.y = y / mmax_x_;
        pt.z = z / mmax_m_xy;
        queue.push_back(pt);
    }
}

/**
 * Sets cfp->flags and the direction bitmasks (dirsSame/dirsAbove/dirsBelow) for
 * the tile at (x, y, z) and enqueues any reachable neighbours.
 * All coordinate parameters use the stride-multiplied convention:
 * y = tyRaw*mmax_x_, z = tzRaw*mmax_m_xy.
 * @param x     stride-multiplied x (raw tile index)
 * @param y     stride-multiplied y
 * @param z     stride-multiplied z
 * @param xm    x - 1
 * @param xp    x + 1
 * @param ym    y - mmax_x_
 * @param yp    y + mmax_x_
 * @param zm    z - mmax_m_xy
 * @param zp    z + mmax_m_xy
 * @param this_s surface type of the current tile (already resolved)
 * @param mmax_m_all total number of tiles (mmax_x_ * mmax_y_ * mmax_z_)
 * @param cfp   flood-point descriptor to fill in (output)
 * @param vtodefine flood-fill queue (modified in place)
 */
void Mission::classifyTile(int x, int y, int z,
                           int xm, int xp, int ym, int yp, int zm, int zp,
                           SurfaceType this_s, int mmax_m_all,
                           FloodNode* cfp, std::vector<WorldPoint>& vtodefine) {
    SurfaceType upper_s = SurfaceType::kEmpty;
    FloodNode* nxtfp = nullptr;
    unsigned char sdirm = 0x00;
    unsigned char sdirh = 0x00;
    unsigned char sdirl = 0x00;
    unsigned char sdirmr = 0x00;

    switch (this_s) {
        case SurfaceType::kEmpty:
            cfp->flags = kNonWalkable;
            break;
        case SurfaceType::kSlopeSN:
            cfp->flags = kWalkable;
            cfp->flags |= kSafeWalk;
            if (zm >= 0) {
                mdpoints_[x + y + zm].flags = kNonWalkable;
                if (yp < mmax_m_xy) {
                    this_s = mtsurfaces_[x + yp + zm];
                    upper_s = mtsurfaces_[x + yp + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        sdirm |= 0x01;
                        nxtfp = &(mdpoints_[x + yp + z]);
                        enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                    } else if (this_s == SurfaceType::kSlopeSN) {
                        nxtfp = &(mdpoints_[x + yp + zm]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x01;
                            nxtfp = &(mdpoints_[x + yp + zm]);
                            enqueueIfUndefined(nxtfp, x, yp, zm, vtodefine);
                        } else
                            nxtfp->flags = kNonWalkable;
                    }
                }
                if (xm >= 0) {
                    this_s = mtsurfaces_[xm + y + zm];
                    upper_s = mtsurfaces_[xm + y + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[xm + y + z]);
                        sdirm |= 0x40;
                        enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                    } else if (SurfaceUtils::isStairs(this_s)) {
                        nxtfp = &(mdpoints_[xm + y + zm]);
                        enqueueIfUndefined(nxtfp, xm, y, zm, vtodefine);
                    }
                }
                if (xp < mmax_x_) {
                    this_s = mtsurfaces_[xp + y + zm];
                    upper_s = mtsurfaces_[xp + y + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[xp + y + z]);
                        sdirm |= 0x04;
                        enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                    } else if (SurfaceUtils::isStairs(this_s)) {
                        nxtfp = &(mdpoints_[xp + y + zm]);
                        enqueueIfUndefined(nxtfp, xp, y, zm, vtodefine);
                    }
                }
            }

            if (ym >= 0) {
                nxtfp = &(mdpoints_[x + ym + zp]);
                this_s = mtsurfaces_[x + ym + z];
                upper_s = mtsurfaces_[x + ym + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    sdirh |= 0x10;
                    enqueueIfUndefined(nxtfp, x, ym, zp, vtodefine);
                } else if(upper_s == SurfaceType::kSlopeSN && (zp + mmax_m_xy) < mmax_m_all) {
                    if(sWalkable(upper_s, mtsurfaces_[
                        x + ym + (zp + mmax_m_xy)]))
                    {
                        sdirh |= 0x10;
                        enqueueIfUndefined(nxtfp, x, ym, zp, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }

            if (xm >= 0) {
                this_s = mtsurfaces_[xm + y + z];
                upper_s = mtsurfaces_[xm + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    nxtfp = &(mdpoints_[xm + y + zp]);
                    sdirh |= 0x40;
                    enqueueIfUndefined(nxtfp, xm, y, zp, vtodefine);
                } else if (this_s == SurfaceType::kSlopeSN) {
                    nxtfp = &(mdpoints_[xm + y + z]);
                    if (sWalkable(this_s, upper_s)) {
                        sdirm |= 0x40;
                        enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }

            if (xp < mmax_x_) {
                this_s = mtsurfaces_[xp + y + z];
                upper_s = mtsurfaces_[xp + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    nxtfp = &(mdpoints_[xp + y + zp]);
                    sdirh |= 0x04;
                    enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                } else if (this_s == SurfaceType::kSlopeSN) {
                    nxtfp = &(mdpoints_[xp + y + z]);
                    if (sWalkable(this_s, upper_s)) {
                        sdirm |= 0x04;
                        enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }
            cfp->dirsSame = sdirm;
            cfp->dirsAbove = sdirh;
            cfp->dirsBelow = sdirl;

            break;
        case SurfaceType::kSlopeNS:
            cfp->flags = kWalkable;
            cfp->flags |= kSafeWalk;
            if (zm >= 0) {
                mdpoints_[x + y + zm].flags = kNonWalkable;
                if (ym >= 0) {
                    this_s = mtsurfaces_[x + ym + zm];
                    upper_s = mtsurfaces_[x + ym + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[x + ym + z]);
                        sdirm |= 0x10;
                        enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                    } else if (this_s == SurfaceType::kSlopeNS) {
                        nxtfp = &(mdpoints_[x + ym + zm]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x10;
                            enqueueIfUndefined(nxtfp, x, ym, zm, vtodefine);
                        } else
                            nxtfp->flags = kNonWalkable;
                    }
                }
                if (xm >= 0) {
                    this_s = mtsurfaces_[xm + y + zm];
                    upper_s = mtsurfaces_[xm + y + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[xm + y + z]);
                        sdirm |= 0x40;
                        enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                    } else if (SurfaceUtils::isStairs(this_s)) {
                        nxtfp = &(mdpoints_[xm + y + zm]);
                        enqueueIfUndefined(nxtfp, xm, y, zm, vtodefine);
                    }
                }
                if (xp < mmax_x_) {
                    this_s = mtsurfaces_[xp + y + zm];
                    upper_s = mtsurfaces_[xp + y + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[xp + y + z]);
                        sdirm |= 0x04;
                        enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                    } else if (SurfaceUtils::isStairs(this_s)) {
                        nxtfp = &(mdpoints_[xp + y + zm]);
                        enqueueIfUndefined(nxtfp, xp, y, zm, vtodefine);
                    }
                }
            }

            if (yp < mmax_m_xy) {
                nxtfp = &(mdpoints_[x + yp + zp]);
                this_s = mtsurfaces_[x + yp + z];
                upper_s = mtsurfaces_[x + yp + zp];
                if(isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    sdirh |= 0x01;
                    enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                } else if(upper_s == SurfaceType::kSlopeNS && (zp + mmax_m_xy) < mmax_m_all) {
                    if(sWalkable(upper_s,  mtsurfaces_[
                        x + yp + (zp + mmax_m_xy)]))
                    {
                        sdirh |= 0x01;
                        enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }

            if (xm >= 0) {
                this_s = mtsurfaces_[xm + y + z];
                upper_s = mtsurfaces_[xm + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    nxtfp = &(mdpoints_[xm + y + zp]);
                    sdirh |= 0x40;
                    enqueueIfUndefined(nxtfp, xm, y, zp, vtodefine);
                } else if (this_s == SurfaceType::kSlopeNS) {
                    nxtfp = &(mdpoints_[xm + y + z]);
                    if (sWalkable(this_s, upper_s)) {
                        sdirm |= 0x40;
                        enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }

            if (xp < mmax_x_) {
                this_s = mtsurfaces_[xp + y + z];
                upper_s = mtsurfaces_[xp + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    nxtfp = &(mdpoints_[xp + y + zp]);
                    sdirh |= 0x04;
                    enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                } else if (this_s == SurfaceType::kSlopeNS) {
                    nxtfp = &(mdpoints_[xp + y + z]);
                    if (sWalkable(this_s, upper_s)) {
                        sdirm |= 0x04;
                        enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }
            cfp->dirsSame = sdirm;
            cfp->dirsAbove = sdirh;
            cfp->dirsBelow = sdirl;

            break;
        case SurfaceType::kSlopeEW:
            cfp->flags = kWalkable;
            cfp->flags |= kSafeWalk;
            if (zm >= 0) {
                mdpoints_[x + y + zm].flags = kNonWalkable;
                if (xm >= 0) {
                    this_s = mtsurfaces_[xm + y + zm];
                    upper_s = mtsurfaces_[xm + y + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[xm + y + z]);
                        sdirm |= 0x40;
                        enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                    } else if (this_s == SurfaceType::kSlopeEW) {
                        nxtfp = &(mdpoints_[xm + y + zm]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x40;
                            enqueueIfUndefined(nxtfp, xm, y, zm, vtodefine);
                        } else
                            nxtfp->flags = kNonWalkable;
                    }
                }
                if (ym >= 0) {
                    this_s = mtsurfaces_[x + ym + zm];
                    upper_s = mtsurfaces_[x + ym + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[x + ym + z]);
                        sdirm |= 0x10;
                        enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                    } else if (SurfaceUtils::isStairs(this_s)) {
                        nxtfp = &(mdpoints_[x + ym + zm]);
                        enqueueIfUndefined(nxtfp, x, ym, zm, vtodefine);
                    }
                }
                if (yp < mmax_m_xy) {
                    this_s = mtsurfaces_[x + yp + zm];
                    upper_s = mtsurfaces_[x + yp + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[x + yp + z]);
                        sdirm |= 0x01;
                        enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                    } else if (SurfaceUtils::isStairs(this_s)) {
                        nxtfp = &(mdpoints_[x + yp + zm]);
                        enqueueIfUndefined(nxtfp, x, yp, zm, vtodefine);
                    }
                }
            }

            if (xp < mmax_x_) {
                nxtfp = &(mdpoints_[xp + y + zp]);
                this_s = mtsurfaces_[xp + y + z];
                upper_s = mtsurfaces_[xp + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    sdirh |= 0x04;
                    enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                } else if(upper_s == SurfaceType::kSlopeEW && (zp + mmax_m_xy) < mmax_m_all) {
                    if(sWalkable(upper_s,
                        mtsurfaces_[xp + y + (zp + mmax_m_xy)]))
                    {
                        sdirh |= 0x04;
                        enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }

            if (ym >= 0) {
                this_s = mtsurfaces_[x + ym + z];
                upper_s = mtsurfaces_[x + ym + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    nxtfp = &(mdpoints_[x + ym + zp]);
                    sdirh |= 0x10;
                    enqueueIfUndefined(nxtfp, x, ym, zp, vtodefine);
                } else if (this_s == SurfaceType::kSlopeEW) {
                    nxtfp = &(mdpoints_[x + ym + z]);
                    if (sWalkable(this_s, upper_s)) {
                        sdirm |= 0x10;
                        enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }

            if (yp < mmax_m_xy) {
                this_s = mtsurfaces_[x + yp + z];
                upper_s = mtsurfaces_[x + yp + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    nxtfp = &(mdpoints_[x + yp + zp]);
                    sdirh |= 0x01;
                    enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                } else if (this_s == SurfaceType::kSlopeEW) {
                    nxtfp = &(mdpoints_[x + yp + z]);
                    if (sWalkable(this_s, upper_s)) {
                        sdirm |= 0x01;
                        enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }
            cfp->dirsSame = sdirm;
            cfp->dirsAbove = sdirh;
            cfp->dirsBelow = sdirl;

            break;
        case SurfaceType::kSlopeWE:
            cfp->flags = kWalkable;
            cfp->flags |= kSafeWalk;
            if (zm >= 0) {
                mdpoints_[x + y + zm].flags = kNonWalkable;
                if (xp < mmax_x_) {
                    this_s = mtsurfaces_[xp + y + zm];
                    upper_s = mtsurfaces_[xp + y + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[xp + y + z]);
                        sdirm |= 0x04;
                        enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                    } else if (this_s == SurfaceType::kSlopeWE) {
                        nxtfp = &(mdpoints_[xp + y + zm]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x04;
                            enqueueIfUndefined(nxtfp, xp, y, zm, vtodefine);
                        } else
                            nxtfp->flags = kNonWalkable;
                    }
                }
                if (ym >= 0) {
                    this_s = mtsurfaces_[x + ym + zm];
                    upper_s = mtsurfaces_[x + ym + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[x + ym + z]);
                        sdirm |= 0x10;
                        enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                    } else if (SurfaceUtils::isStairs(this_s)) {
                        nxtfp = &(mdpoints_[x + ym + zm]);
                        enqueueIfUndefined(nxtfp, x, ym, zm, vtodefine);
                    }
                }
                if (yp < mmax_m_xy) {
                    this_s = mtsurfaces_[x + yp + zm];
                    upper_s = mtsurfaces_[x + yp + z];
                    if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                        nxtfp = &(mdpoints_[x + yp + z]);
                        sdirm |= 0x01;
                        enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                    } else if (SurfaceUtils::isStairs(this_s)) {
                        nxtfp = &(mdpoints_[x + yp + zm]);
                        enqueueIfUndefined(nxtfp, x, yp, zm, vtodefine);
                    }
                }
            }

            if (xm >= 0) {
                nxtfp = &(mdpoints_[xm + y + zp]);
                this_s = mtsurfaces_[xm + y + z];
                upper_s = mtsurfaces_[xm + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    sdirh |= 0x40;
                    enqueueIfUndefined(nxtfp, xm, y, zp, vtodefine);
                } else if(upper_s == SurfaceType::kSlopeWE && (zp + mmax_m_xy) < mmax_m_all) {
                    if(sWalkable(upper_s, mtsurfaces_[
                        xm + y + (zp + mmax_m_xy)]))
                    {
                        sdirh |= 0x40;
                        enqueueIfUndefined(nxtfp, xm, y, zp, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }

            if (ym >= 0) {
                this_s = mtsurfaces_[x + ym + z];
                upper_s = mtsurfaces_[x + ym + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    nxtfp = &(mdpoints_[x + ym + zp]);
                    sdirh |= 0x10;
                    enqueueIfUndefined(nxtfp, x, ym, zp, vtodefine);
                } else if (this_s == SurfaceType::kSlopeWE) {
                    nxtfp = &(mdpoints_[x + ym + z]);
                    if (sWalkable(this_s, upper_s)) {
                        sdirm |= 0x10;
                        enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }

            if (yp < mmax_m_xy) {
                this_s = mtsurfaces_[x + yp + z];
                upper_s = mtsurfaces_[x + yp + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s)) {
                    nxtfp = &(mdpoints_[x + yp + zp]);
                    sdirh |= 0x01;
                    enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                } else if (this_s == SurfaceType::kSlopeWE) {
                    nxtfp = &(mdpoints_[x + yp + z]);
                    if (sWalkable(this_s, upper_s)) {
                        sdirm |= 0x01;
                        enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                    } else
                        nxtfp->flags = kNonWalkable;
                }
            }
            cfp->dirsSame = sdirm;
            cfp->dirsAbove = sdirh;
            cfp->dirsBelow = sdirl;

            break;
        case SurfaceType::kGround:
        case SurfaceType::kRoad:
        case SurfaceType::kRoof:
        case SurfaceType::kRoadPedCross:
            cfp->flags = kWalkable;
            if (this_s != SurfaceType::kRoad) {
                cfp->flags |= kSafeWalk;
            }
            if (xm >= 0) {
                this_s = mtsurfaces_[xm + y + z];
                upper_s = mtsurfaces_[xm + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x20 | 0x40 | 0x80);
                    nxtfp = &(mdpoints_[xm + y + zp]);
                    enqueueIfUndefined(nxtfp, xm, y, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x20 | 0x80);
                    if (this_s == SurfaceType::kSlopeSN || this_s == SurfaceType::kSlopeNS
                        || this_s == SurfaceType::kSlopeEW)
                    {
                        sdirl |= 0x40;
                    }
                    nxtfp = &(mdpoints_[xm + y + z]);
                    enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                } else {
                    sdirmr |= (0x20 | 0x80);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeSN || upper_s == SurfaceType::kSlopeNS || upper_s == SurfaceType::kSlopeWE
                        || upper_s == SurfaceType::kTrainPlatformEW)) {
                        if (sWalkable(upper_s,
                            mtsurfaces_[xm + y + (zp + mmax_m_xy)]))
                        {
                            if (upper_s == SurfaceType::kTrainPlatformEW)
                                sdirh |= 0x40;
                            else
                                sdirm |= 0x40;
                            nxtfp = &(mdpoints_[xm + y + zp]);
                            enqueueIfUndefined(nxtfp, xm, y, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x20 | 0x80);

            if (xp < mmax_x_) {
                this_s = mtsurfaces_[xp + y + z];
                upper_s = mtsurfaces_[xp + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x02 | 0x04 | 0x08);
                    nxtfp = &(mdpoints_[xp + y + zp]);
                    enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x02 | 0x08);
                    if (this_s == SurfaceType::kSlopeSN || this_s == SurfaceType::kSlopeNS
                        || this_s == SurfaceType::kSlopeWE)
                    {
                        sdirl |= 0x04;
                    }
                    nxtfp = &(mdpoints_[xp + y + z]);
                    enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                } else {
                    sdirmr |= (0x02 | 0x08);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeSN || upper_s == SurfaceType::kSlopeNS
                        || upper_s == SurfaceType::kSlopeEW || upper_s == SurfaceType::kTrainPlatformNS))
                    {
                        if (sWalkable(upper_s,
                            mtsurfaces_[xp + y + (zp + mmax_m_xy)]))
                        {
                            if (upper_s == SurfaceType::kTrainPlatformNS)
                                sdirh |= 0x04;
                            else
                                sdirm |= 0x04;
                            nxtfp = &(mdpoints_[xp + y + zp]);
                            enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x02 | 0x08);

            if(ym >= 0) {
                this_s = mtsurfaces_[x + ym + z];
                upper_s = mtsurfaces_[x + ym + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x08 | 0x10 | 0x20);
                    nxtfp = &(mdpoints_[x + ym + zp]);
                    enqueueIfUndefined(nxtfp, x, ym, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x08 | 0x20);
                    if (this_s == SurfaceType::kSlopeNS || this_s == SurfaceType::kSlopeEW || this_s == SurfaceType::kSlopeWE){
                        sdirl |= 0x10;
                    }
                    nxtfp = &(mdpoints_[x + ym + z]);
                    enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                } else {
                    sdirmr |= (0x08 | 0x20);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeSN || upper_s == SurfaceType::kSlopeEW
                        || upper_s == SurfaceType::kSlopeWE || upper_s == SurfaceType::kTrainPlatformNS))
                    {
                        if (sWalkable(upper_s,
                            mtsurfaces_[x + ym + (zp + mmax_m_xy)]))
                        {
                            if (upper_s == SurfaceType::kTrainPlatformNS)
                                sdirh |= 0x10;
                            else
                                sdirm |= 0x10;
                            nxtfp = &(mdpoints_[x + ym + zp]);
                            enqueueIfUndefined(nxtfp, x, ym, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x08 | 0x20);

            if (yp < mmax_m_xy) {
                this_s = mtsurfaces_[x + yp + z];
                upper_s = mtsurfaces_[x + yp + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x80 | 0x01 | 0x02);
                    nxtfp = &(mdpoints_[x + yp + zp]);
                    enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x80 | 0x02);
                    if (this_s == SurfaceType::kSlopeSN || this_s == SurfaceType::kSlopeEW
                        || this_s == SurfaceType::kSlopeWE)
                    {
                        sdirl |= 0x01;
                    }
                    nxtfp = &(mdpoints_[x + yp + z]);
                    enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                } else {
                    sdirmr |= (0x80 | 0x02);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeNS || upper_s == SurfaceType::kSlopeEW
                        || upper_s == SurfaceType::kSlopeWE || upper_s == SurfaceType::kTrainPlatformEW))
                    {
                        if (sWalkable(upper_s,
                            mtsurfaces_[x + yp + (zp + mmax_m_xy)]))
                        {
                            if (upper_s == SurfaceType::kTrainPlatformEW)
                                sdirh |= 0x01;
                            else
                                sdirm |= 0x01;
                            nxtfp = &(mdpoints_[x + yp + zp]);
                            enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x80 | 0x02);
            sdirm &= (0xFF ^ sdirmr);

            // edges

            if (xm >= 0) {
                if (ym >= 0 && (sdirm & 0x20) != 0) {
                    nxtfp = &(mdpoints_[xm + ym + zp]);
                    this_s = mtsurfaces_[xm + ym + z];
                    upper_s = mtsurfaces_[xm + ym + zp];
                    if (!(isSurface(this_s) && sWalkable(this_s,
                        upper_s)))
                    {
                        sdirm &= (0xFF ^ 0x20);
                    } else {
                        enqueueIfUndefined(nxtfp, xm, ym, zp, vtodefine);
                    }
                }

                if (yp < mmax_m_xy && (sdirm & 0x80) != 0) {
                    nxtfp = &(mdpoints_[xm + yp + zp]);
                    this_s = mtsurfaces_[xm + yp + z];
                    upper_s = mtsurfaces_[xm + yp + zp];
                    if (!(isSurface(this_s) && sWalkable(this_s,
                        upper_s)))
                    {
                        sdirm &= (0xFF ^ 0x80);
                    } else {
                        enqueueIfUndefined(nxtfp, xm, yp, zp, vtodefine);
                    }
                }
            }

            if (xp < mmax_x_) {
                if (ym >= 0 && (sdirm & 0x08) != 0) {
                    nxtfp = &(mdpoints_[xp + ym + zp]);
                    this_s = mtsurfaces_[xp + ym + z];
                    upper_s = mtsurfaces_[xp + ym + zp];
                    if (!(isSurface(this_s) && sWalkable(this_s,
                        upper_s)))
                    {
                        sdirm &= (0xFF ^ 0x08);
                    } else {
                        enqueueIfUndefined(nxtfp, xp, ym, zp, vtodefine);
                    }
                }

                if (yp < mmax_m_xy && (sdirm & 0x02) != 0) {
                    nxtfp = &(mdpoints_[xp + yp + zp]);
                    this_s = mtsurfaces_[xp + yp + z];
                    upper_s = mtsurfaces_[xp + yp + zp];
                    if (!(isSurface(this_s) && sWalkable(this_s,
                        upper_s)))
                    {
                        sdirm &= (0xFF ^ 0x02);
                    } else {
                        enqueueIfUndefined(nxtfp, xp, yp, zp, vtodefine);
                    }
                }
            }
            cfp->dirsSame = sdirm;
            cfp->dirsAbove = sdirh;
            cfp->dirsBelow = sdirl;

            break;
        case SurfaceType::kNonWalkable:
        case SurfaceType::kHandrailLight:
        case SurfaceType::kTrainStop:
            cfp->flags = kNonWalkable;
            break;
        case SurfaceType::kTrainPlatformNS:
            cfp->flags = kWalkable;
            cfp->flags |= kSafeWalk;
            if (zm >= 0) {
                mdpoints_[x + y + zm].flags = kNonWalkable;
                if (xm >= 0) {
                    this_s = mtsurfaces_[xm + y + zm];
                    upper_s = mtsurfaces_[xm + y + z];
                    if (isSurface(this_s)) {
                        nxtfp = &(mdpoints_[xm + y + z]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x40;
                            enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                        }
                    } else if (SurfaceUtils::isStairs(upper_s) && upper_s != SurfaceType::kSlopeWE) {
                        nxtfp = &(mdpoints_[xm + y + z]);
                        this_s = upper_s;
                        upper_s = mtsurfaces_[xm + y + zp];
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x40;
                            enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                        }
                    }
                }
                if (ym >= 0) {
                    this_s = mtsurfaces_[x + ym + zm];
                    upper_s = mtsurfaces_[x + ym + z];
                    if (isSurface(this_s)) {
                        nxtfp = &(mdpoints_[x + ym + z]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x10;
                            enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                        }
                    } else if (SurfaceUtils::isStairs(upper_s) && upper_s != SurfaceType::kSlopeSN) {
                        nxtfp = &(mdpoints_[x + ym + z]);
                        this_s = upper_s;
                        upper_s = mtsurfaces_[x + ym + zp];
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x10;
                            enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                        }
                    }
                }
                if (yp < mmax_m_xy) {
                    this_s = mtsurfaces_[x + yp + zm];
                    upper_s = mtsurfaces_[x + yp + z];
                    if (isSurface(this_s)) {
                        nxtfp = &(mdpoints_[x + yp + z]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x01;
                            enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                        }
                    } else if (SurfaceUtils::isStairs(upper_s) && upper_s != SurfaceType::kSlopeNS) {
                        nxtfp = &(mdpoints_[x + yp + z]);
                        this_s = upper_s;
                        upper_s = mtsurfaces_[x + yp + zp];
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x01;
                            enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                        }
                    }
                }
            }

            if (xp < mmax_x_) {
                this_s = mtsurfaces_[xp + y + z];
                upper_s = mtsurfaces_[xp + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x02 | 0x04 | 0x08);
                    nxtfp = &(mdpoints_[xp + y + zp]);
                    enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x02 | 0x08);
                    if (this_s == SurfaceType::kSlopeSN || this_s == SurfaceType::kSlopeNS || this_s == SurfaceType::kSlopeWE){
                        sdirl |= 0x04;
                    }
                    nxtfp = &(mdpoints_[xp + y + z]);
                    enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                } else {
                    sdirmr |= (0x02 | 0x08);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeSN || upper_s == SurfaceType::kSlopeNS
                        || upper_s == SurfaceType::kSlopeEW))
                    {
                        if (sWalkable(upper_s,
                            mtsurfaces_[xp + y + (zp + mmax_m_xy)]))
                        {
                            sdirm |= 0x04;
                            nxtfp = &(mdpoints_[xp + y + zp]);
                            enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x02 | 0x08);

            if(ym >= 0) {
                this_s = mtsurfaces_[x + ym + z];
                upper_s = mtsurfaces_[x + ym + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x08 | 0x10);
                    nxtfp = &(mdpoints_[x + ym + zp]);
                    enqueueIfUndefined(nxtfp, x, ym, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x08 | 0x20);
                    if (this_s == SurfaceType::kSlopeNS || this_s == SurfaceType::kSlopeEW || this_s == SurfaceType::kSlopeWE) {
                        sdirl |= 0x10;
                    }
                    nxtfp = &(mdpoints_[x + ym + z]);
                    enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                } else {
                    sdirmr |= (0x08 | 0x20);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeSN || upper_s == SurfaceType::kSlopeEW || upper_s == SurfaceType::kSlopeWE)) {
                        if (sWalkable(upper_s,
                            mtsurfaces_[x + ym + (zp + mmax_m_xy)]))
                        {
                            sdirm |= 0x10;
                            nxtfp = &(mdpoints_[x + ym + zp]);
                            enqueueIfUndefined(nxtfp, x, ym, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x08);

            if (yp < mmax_m_xy) {
                this_s = mtsurfaces_[x + yp + z];
                upper_s = mtsurfaces_[x + yp + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x01 | 0x02);
                    nxtfp = &(mdpoints_[x + yp + zp]);
                    enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x80 | 0x02);
                    if (this_s == SurfaceType::kSlopeSN || this_s == SurfaceType::kSlopeEW || this_s == SurfaceType::kSlopeWE) {
                        sdirl |= 0x01;
                    }
                    nxtfp = &(mdpoints_[x + yp + z]);
                    enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                } else {
                    sdirmr |= (0x80 | 0x02);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeNS || upper_s == SurfaceType::kSlopeEW
                        || upper_s == SurfaceType::kSlopeWE))
                    {
                        if (sWalkable(upper_s,
                            mtsurfaces_[x + yp + (zp + mmax_m_xy)]))
                        {
                            sdirm |= 0x01;
                            nxtfp = &(mdpoints_[x + yp + zp]);
                            enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x80 | 0x02);
            sdirm &= (0xFF ^ sdirmr);

            // edges
            if (xp < mmax_x_) {
                if (ym >= 0 && (sdirm & 0x08) != 0) {
                    nxtfp = &(mdpoints_[xp + ym + zp]);
                    this_s = mtsurfaces_[xp + ym + z];
                    upper_s = mtsurfaces_[xp + ym + zp];
                    if (!(isSurface(this_s) && sWalkable(this_s,
                        upper_s)))
                    {
                        sdirm &= (0xFF ^ 0x08);
                    } else {
                        enqueueIfUndefined(nxtfp, xp, ym, zp, vtodefine);
                    }
                }

                if (yp < mmax_m_xy && (sdirm & 0x02) != 0) {
                    nxtfp = &(mdpoints_[xp + yp + zp]);
                    this_s = mtsurfaces_[xp + yp + z];
                    upper_s = mtsurfaces_[xp + yp + zp];
                    if (!(isSurface(this_s) && sWalkable(this_s,
                        upper_s)))
                    {
                        sdirm &= (0xFF ^ 0x02);
                    } else {
                        enqueueIfUndefined(nxtfp, xp, yp, z, vtodefine);
                    }
                }
            }
            cfp->dirsSame = sdirm;
            cfp->dirsAbove = sdirh;
            cfp->dirsBelow = sdirl;

            break;
        case SurfaceType::kTrainPlatformEW:
            cfp->flags = kWalkable;
            cfp->flags |= kSafeWalk;
            if (zm >= 0) {
                mdpoints_[x + y + zm].flags = kNonWalkable;
                if (ym >= 0) {
                    this_s = mtsurfaces_[x + ym + zm];
                    upper_s = mtsurfaces_[x + ym + z];
                    if (isSurface(this_s)) {
                        nxtfp = &(mdpoints_[x + ym + z]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x10;
                            enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                        }
                    } else if (SurfaceUtils::isStairs(upper_s) && upper_s != SurfaceType::kSlopeSN) {
                        nxtfp = &(mdpoints_[x + ym + z]);
                        this_s = upper_s;
                        upper_s = mtsurfaces_[x + ym + zp];
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x10;
                            enqueueIfUndefined(nxtfp, x, ym, z, vtodefine);
                        }
                    }
                }
                if (xm >= 0) {
                    this_s = mtsurfaces_[xm + y + zm];
                    upper_s = mtsurfaces_[xm + y + z];
                    if (isSurface(this_s)) {
                        nxtfp = &(mdpoints_[xm + y + z]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x40;
                            enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                        }
                    } else if (SurfaceUtils::isStairs(upper_s) && upper_s != SurfaceType::kSlopeWE) {
                        nxtfp = &(mdpoints_[xm + y + z]);
                        this_s = upper_s;
                        upper_s = mtsurfaces_[xm + y + zp];
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x40;
                            enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                        }
                    }
                }
                if (xp < mmax_x_) {
                    this_s = mtsurfaces_[xp + y + zm];
                    upper_s = mtsurfaces_[xp + y + z];
                    if (isSurface(this_s)) {
                        nxtfp = &(mdpoints_[xp + y + z]);
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x04;
                            enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                        }
                    } else if (SurfaceUtils::isStairs(upper_s) && upper_s != SurfaceType::kSlopeEW) {
                        nxtfp = &(mdpoints_[xp + y + z]);
                        this_s = upper_s;
                        upper_s = mtsurfaces_[xp + y + zp];
                        if (sWalkable(this_s, upper_s)) {
                            sdirl |= 0x04;
                            enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                        }
                    }
                }
            }

            if (xm >=0) {
                this_s = mtsurfaces_[xm + y + z];
                upper_s = mtsurfaces_[xm + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x40 | 0x80);
                    nxtfp = &(mdpoints_[xm + y + zp]);
                    enqueueIfUndefined(nxtfp, xm, y, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x20 | 0x80);
                    if (this_s == SurfaceType::kSlopeSN || this_s == SurfaceType::kSlopeNS || this_s == SurfaceType::kSlopeEW){
                        sdirl |= 0x40;
                    }
                    nxtfp = &(mdpoints_[xm + y + z]);
                    enqueueIfUndefined(nxtfp, xm, y, z, vtodefine);
                } else {
                    sdirmr |= (0x20 | 0x80);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeSN || upper_s == SurfaceType::kSlopeNS
                        || upper_s == SurfaceType::kSlopeWE))
                    {
                        if (sWalkable(upper_s,
                            mtsurfaces_[xm + y + (zp + mmax_m_xy)]))
                        {
                            sdirm |= 0x40;
                            nxtfp = &(mdpoints_[xm + y + zp]);
                            enqueueIfUndefined(nxtfp, xm, y, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x20 | 0x80);

            if (xp < mmax_x_) {
                this_s = mtsurfaces_[xp + y + z];
                upper_s = mtsurfaces_[xp + y + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x02 | 0x04);
                    nxtfp = &(mdpoints_[xp + y + zp]);
                    enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x02 | 0x08);
                    if (this_s == SurfaceType::kSlopeSN || this_s == SurfaceType::kSlopeNS
                        || this_s == SurfaceType::kSlopeWE)
                    {
                        sdirl |= 0x04;
                    }
                    nxtfp = &(mdpoints_[xp + y + z]);
                    enqueueIfUndefined(nxtfp, xp, y, z, vtodefine);
                } else {
                    sdirmr |= (0x02 | 0x08);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeSN || upper_s == SurfaceType::kSlopeNS
                        || upper_s == SurfaceType::kSlopeEW))
                    {
                        if (sWalkable(upper_s,
                            mtsurfaces_[xp + y + (zp + mmax_m_xy)]))
                        {
                            sdirm |= 0x04;
                            nxtfp = &(mdpoints_[xp + y + zp]);
                            enqueueIfUndefined(nxtfp, xp, y, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x02 | 0x08);

            if (yp < mmax_m_xy) {
                this_s = mtsurfaces_[x + yp + z];
                upper_s = mtsurfaces_[x + yp + zp];
                if (isSurface(this_s) && sWalkable(this_s, upper_s))
                {
                    sdirm |= (0x80 | 0x01 | 0x02);
                    nxtfp = &(mdpoints_[x + yp + zp]);
                    enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                } else if (SurfaceUtils::isStairs(this_s) && sWalkable(this_s,
                    upper_s))
                {
                    sdirmr |= (0x80 | 0x02);
                    if (this_s == SurfaceType::kSlopeSN || this_s == SurfaceType::kSlopeEW || this_s == SurfaceType::kSlopeWE) {
                        sdirl |= 0x01;
                    }
                    nxtfp = &(mdpoints_[x + yp + z]);
                    enqueueIfUndefined(nxtfp, x, yp, z, vtodefine);
                } else {
                    sdirmr |= (0x80 | 0x02);
                    if ((zp + mmax_m_xy) < mmax_m_all
                        && (upper_s == SurfaceType::kSlopeNS || upper_s == SurfaceType::kSlopeEW
                        || upper_s == SurfaceType::kSlopeWE))
                    {
                        if (sWalkable(upper_s,
                            mtsurfaces_[x + yp + (zp + mmax_m_xy)]))
                        {
                            sdirm |= 0x01;
                            nxtfp = &(mdpoints_[x + yp + zp]);
                            enqueueIfUndefined(nxtfp, x, yp, zp, vtodefine);
                        }
                    }
                }
            } else
                sdirmr |= (0x80 | 0x02);
            sdirm &= (0xFF ^ sdirmr);

            // edges
            if (yp < mmax_m_xy) {
                if (xm >= 0 && (sdirm & 0x80) != 0) {
                    nxtfp = &(mdpoints_[xm + yp + zp]);
                    this_s = mtsurfaces_[xm + yp + z];
                    upper_s = mtsurfaces_[xm + yp + zp];
                    if (!(isSurface(this_s) && sWalkable(this_s,
                        upper_s)))
                    {
                        sdirm &= (0xFF ^ 0x80);
                    } else {
                        enqueueIfUndefined(nxtfp, xm, yp, zp, vtodefine);
                    }
                }
                if (xp < mmax_x_ && (sdirm & 0x02) != 0) {
                    nxtfp = &(mdpoints_[xp + yp + zp]);
                    this_s = mtsurfaces_[xp + yp + z];
                    upper_s = mtsurfaces_[xp + yp + zp];
                    if (!(isSurface(this_s) && sWalkable(this_s,
                        upper_s)))
                    {
                        sdirm &= (0xFF ^ 0x02);
                    } else {
                        enqueueIfUndefined(nxtfp, xp, yp, zp, vtodefine);
                    }
                }
            }

            cfp->dirsSame = sdirm;
            cfp->dirsAbove = sdirh;
            cfp->dirsBelow = sdirl;

            break;
        case SurfaceType::kUnknown:
            LOG(Log::k_FLG_GAME, "Mission", "classifyTile()", ("Unknown surface"));
            break;
    }
}

bool Mission::findWalkableTileFromBase(TilePoint &basePt) {
    for (int z = mmax_z_ - 1; z >= 0; z--) {
        TilePoint candidate = p_map_->projectToZLevel(basePt, z);
        
        if (!p_map_->isWithinMapBounds(candidate)) {
            if (candidate.tz < 0 || candidate.tx < 0 || candidate.ty < 0) {
                break; // Out of bounds below/behind, stop searching
            }
            continue; // Out of bounds above/ahead, try next Z level
        }
        
        int tileIndex = getTileIndex(candidate);
        
        // Try to find walkable position at current Z level
        if (isTileWalkable(tileIndex)) {
            TilePoint adjusted = adjustPositionForSurface(candidate, tileIndex);
            if (adjusted.isValid()) {
                basePt = adjusted;
                return true;
            }
        }
        
        // Try to find walkable position on slope at Z-1 level
        if (z > 0) {
            TilePoint onLowerSlope = tryProjectOntoLowerSlope(candidate);
            if (onLowerSlope.isValid()) {
                basePt = onLowerSlope;
                return true;
            }
        }
    }
    
    return false;
}

/**
 * Adjusts the position within a tile based on its surface type (flat or sloped).
 * Returns an invalid TilePoint if the position cannot be adjusted to be walkable.
 */
TilePoint Mission::adjustPositionForSurface(const TilePoint &point, int tileIndex) {
    SurfaceType surfaceType = mtsurfaces_[tileIndex];
    
    switch (surfaceType) {
        case SurfaceType::kSlopeSN:
            return adjustForSlopeSN(point, tileIndex);
        case SurfaceType::kSlopeNS:
            return adjustForSlopeNS(point, tileIndex);
        case SurfaceType::kSlopeEW:
            return adjustForSlopeEW(point, tileIndex);
        case SurfaceType::kSlopeWE:
            return adjustForSlopeWE(point, tileIndex);
        default:
            // Flat surface or other walkable type - position is valid as-is
            return point;
    }
}

/**
 * Adjusts position for South-to-North slope (rises northward).
 */
TilePoint Mission::adjustForSlopeSN(const TilePoint &point, int tileIndex) {
    int adjustedY = ((point.oy + 128) * 2) / 3;
    int adjustedX = point.ox + 128 - adjustedY / 2;
    
    if (adjustedX < 256) {
        // Position fits within current tile
        return TilePoint(point.tx, point.ty, point.tz, adjustedX, adjustedY);
    }
    
    // Position overflows to adjacent tile in +X direction
    return tryAdjacentTile(point, +1, 0, adjustedX - 256, adjustedY, 
                          SurfaceType::kSlopeSN);
}

/**
 * Adjusts position for North-to-South slope (rises southward).
 */
TilePoint Mission::adjustForSlopeNS(const TilePoint &point, int tileIndex) {
    if (point.oy >= 128) {
        // TODO: Southern half not implemented
        return TilePoint::invalid();
    }
    
    int adjustedY = point.oy * 2;
    int adjustedX = point.ox + point.oy;
    
    if (adjustedX < 256) {
        // Position fits within current tile
        return TilePoint(point.tx, point.ty, point.tz, adjustedX, adjustedY);
    }
    
    // Position overflows to adjacent tile in +X direction
    return tryAdjacentTile(point, +1, 0, adjustedX - 256, adjustedY, 
                          SurfaceType::kSlopeNS);
}

/**
 * Adjusts position for East-to-West slope (rises westward).
 */
TilePoint Mission::adjustForSlopeEW(const TilePoint &point, int tileIndex) {
    if (point.ox >= 128) {
        // TODO: Eastern half not implemented
        return TilePoint::invalid();
    }
    
    int adjustedX = point.ox * 2;
    int adjustedY = point.ox + point.oy;
    
    if (adjustedY < 256) {
        // Position fits within current tile
        return TilePoint(point.tx, point.ty, point.tz, adjustedX, adjustedY);
    }
    
    // Position overflows to adjacent tile in +Y direction
    return tryAdjacentTile(point, 0, +1, adjustedX, adjustedY - 256, 
                          SurfaceType::kSlopeEW);
}

/**
 * Adjusts position for West-to-East slope (rises eastward).
 */
TilePoint Mission::adjustForSlopeWE(const TilePoint &point, int tileIndex) {
    int adjustedX = ((point.ox + 128) * 2) / 3;
    int adjustedY = point.oy + 128 - adjustedX / 2;
    
    if (adjustedY < 256) {
        // Position fits within current tile
        return TilePoint(point.tx, point.ty, point.tz, adjustedX, adjustedY);
    }
    
    // Position overflows to adjacent tile in +Y direction
    return tryAdjacentTile(point, 0, +1, adjustedX, adjustedY - 256, 
                          SurfaceType::kSlopeWE);
}

/**
 * Tries to continue slope adjustment onto an adjacent tile.
 */
TilePoint Mission::tryAdjacentTile(const TilePoint &point, int deltaX, int deltaY,
                                   int newOffsetX, int newOffsetY, 
                                   SurfaceType expectedSurfaceType) {
    int adjacentTx = point.tx + deltaX;
    int adjacentTy = point.ty + deltaY;
    
    // Check if adjacent tile is within bounds
    if ((deltaX > 0 && adjacentTx >= mmax_x_) || 
        (deltaY > 0 && adjacentTy >= mmax_y_)) {
        return TilePoint::invalid();
    }
    
    int adjacentIndex = adjacentTx + adjacentTy * mmax_x_ + point.tz * mmax_m_xy;
    
    // Check if adjacent tile is walkable and has the same slope type
    if (isTileWalkable(adjacentIndex) && 
        mtsurfaces_[adjacentIndex] == expectedSurfaceType) {
        return TilePoint(adjacentTx, adjacentTy, point.tz, newOffsetX, newOffsetY);
    }
    
    return TilePoint::invalid();
}

/**
 * Attempts to project the point onto a slope at Z-1 level.
 * This handles cases where the character is "above" a sloped surface.
 */
TilePoint Mission::tryProjectOntoLowerSlope(const TilePoint &point) {
    int lowerZ = point.tz - 1;
    if (lowerZ < 0) {
        return TilePoint::invalid();
    }
    
    int lowerIndex = point.tx + point.ty * mmax_x_ + lowerZ * mmax_m_xy;
    
    if (!isTileWalkable(lowerIndex)) {
        return TilePoint::invalid();
    }
    
    SurfaceType surfaceType = mtsurfaces_[lowerIndex];
    
    switch (surfaceType) {
        case SurfaceType::kSlopeSN:
            return projectOntoSlopeSN(point, lowerZ);
        case SurfaceType::kSlopeNS:
            return projectOntoSlopeNS(point, lowerZ);
        case SurfaceType::kSlopeEW:
            return projectOntoSlopeEW(point, lowerZ);
        case SurfaceType::kSlopeWE:
            return projectOntoSlopeWE(point, lowerZ);
        default:
            return TilePoint::invalid();
    }
}

/**
 * Projects point onto South-to-North slope at lower Z level.
 */
TilePoint Mission::projectOntoSlopeSN(const TilePoint &point, int lowerZ) {
    int projectedY = (point.oy * 2) / 3;
    int projectedX = point.ox - projectedY / 2;
    
    if (projectedX >= 0) {
        return TilePoint(point.tx, point.ty, lowerZ, projectedX, projectedY);
    }
    
    return TilePoint::invalid();
}

/**
 * Projects point onto North-to-South slope at lower Z level.
 */
TilePoint Mission::projectOntoSlopeNS(const TilePoint &point, int lowerZ) {
    int projectedY = (point.oy - 128) * 2;
    int projectedX = (point.ox + projectedY / 2) - 128;
    
    if (projectedY >= 0 && projectedX >= 0 && projectedX < 256) {
        return TilePoint(point.tx, point.ty, lowerZ, projectedX, projectedY);
    }
    
    return TilePoint::invalid();
}

/**
 * Projects point onto East-to-West slope at lower Z level.
 */
TilePoint Mission::projectOntoSlopeEW(const TilePoint &point, int lowerZ) {
    int projectedX = (point.ox - 128) * 2;
    int projectedY = (point.oy + projectedX / 2) - 128;
    
    if (projectedX >= 0 && projectedY >= 0 && projectedY < 256) {
        return TilePoint(point.tx, point.ty, lowerZ, projectedX, projectedY);
    }
    
    return TilePoint::invalid();
}

/**
 * Projects point onto West-to-East slope at lower Z level.
 */
TilePoint Mission::projectOntoSlopeWE(const TilePoint &point, int lowerZ) {
    int projectedX = (point.ox * 2) / 3;
    int projectedY = point.oy - projectedX / 2;
    
    if (projectedY >= 0) {
        return TilePoint(point.tx, point.ty, lowerZ, projectedX, projectedY);
    }
    
    return TilePoint::invalid();
}

bool Mission::getWalkableClosestByZ(TilePoint &mtp) {
    // NOTE: using z from mtp as start to find closest z
    int inc_z = mtp.tz;
    int dec_z = mtp.tz;
    bool found = false;

    do {
        if (inc_z < mmax_z_) {
            if ((mdpoints_[mtp.tx + mtp.ty * mmax_x_
                + inc_z * mmax_m_xy].flags & kWalkable) == kWalkable)
            {
                mtp.tz = inc_z;
                found = true;
                break;
            }
            inc_z++;
        }
        if (dec_z >= 0) {
            if ((mdpoints_[mtp.tx + mtp.ty * mmax_x_
                + dec_z * mmax_m_xy].flags & kWalkable) == kWalkable)
            {
                mtp.tz = dec_z;
                found = true;
                break;
            }
            dec_z--;
        }
    } while (inc_z < mmax_z_ || dec_z >= 0);

    return found;
}

/*!
 * Inserts obj into every cell of grid whose tile overlaps the object's bounding box.
 * Each object is in exactly the cells it occupies (1 cell for small objects, up to a
 * few cells for large vehicles/statics).
 */
void Mission::insertIntoSpatialGrid(std::vector<std::vector<MapObject*>>& grid, MapObject* obj) {
    int wx = obj->tileX() * 256 + obj->offX();
    int wy = obj->tileY() * 256 + obj->offY();
    int wz = obj->tileZ() * 128 + obj->offZ();

    int tx_min = std::max(0, (wx - obj->sizeX()) / 256);
    int tx_max = std::min(mmax_x_ - 1, (wx + obj->sizeX()) / 256);
    int ty_min = std::max(0, (wy - obj->sizeY()) / 256);
    int ty_max = std::min(mmax_y_ - 1, (wy + obj->sizeY()) / 256);
    int tz_min = std::max(0, wz / 128);
    int tz_max = std::min(mmax_z_ - 1, (wz + obj->sizeZ()) / 128);

    for (int tz = tz_min; tz <= tz_max; ++tz) {
        for (int ty = ty_min; ty <= ty_max; ++ty) {
            for (int tx = tx_min; tx <= tx_max; ++tx) {
                grid[tx + ty * mmax_x_ + tz * mmax_m_xy].push_back(obj);
            }
        }
    }
}

/*!
 * Rebuilds the dynamic spatial grid from the current positions of all mobile entities.
 * Called once per tick, before any entity animation, so the grid reflects positions
 * from the end of the previous tick (positional error < 1 tick, negligible).
 */
void Mission::buildDynamicSpatialGrid() {
    dynamicSpatialGrid_.assign(mmax_x_ * mmax_y_ * mmax_z_, {});

    for (PedInstance* pPed : peds_) {
        if (pPed->isAlive() && !pPed->isInVehicle()) {
            insertIntoSpatialGrid(dynamicSpatialGrid_, pPed);
        }
    }
    for (Vehicle* pVehicle : vehicles_) {
        insertIntoSpatialGrid(dynamicSpatialGrid_, pVehicle);
    }
    for (WeaponInstance* pWeapon : weaponsOnGround_) {
        if (!pWeapon->hasOwner()) {
            insertIntoSpatialGrid(dynamicSpatialGrid_, pWeapon);
        }
    }
}

/*!
 * @brief Searches for the closest object blocking the ray between two world points.
 *
 * Uses a spatial grid to test only objects present in the tiles the ray traverses,
 * rather than scanning all entities linearly. The stepping method mirrors
 * checkBlockedByTile() (8 world-unit steps).
 *
 * Excluded from the search:
 * - \p pOrigin itself (the shooter)
 * - the vehicle \p pOrigin is currently riding, if any
 * - Static objects whose isExcludedFromBlockers() returns true (e.g. open doors)
 * - Dead peds
 *
 * If a blocker is found, \p pStartPt and \p pEndPt are updated to the intersection
 * points with the blocker's bounding box, and \p dist is set to the distance from
 * the original \p pStartPt to that intersection.
 *
 * @param pStartPt  Origin of the ray (world coordinates). Updated to the blocker
 *                  entry point if a blocker is found.
 * @param pEndPt    End of the ray (world coordinates). Updated to the blocker
 *                  exit point if a blocker is found.
 * @param dist      Distance from \p pStartPt to \p pEndPt. Updated to the distance
 *                  to the closest blocker if one is found.
 * @param pOrigin   The entity that initiated the ray (shooter). May be nullptr.
 * @return Pointer to the closest blocking MapObject, or nullptr if none was found.
 */
MapObject * Mission::checkBlockedByObject(WorldPoint * pStartPt, WorldPoint * pEndPt,
        double *dist, const ShootableMapObject *pOrigin) {
    double inc_xyz[3];
    inc_xyz[0] = (pEndPt->x - pStartPt->x) / (*dist);
    inc_xyz[1] = (pEndPt->y - pStartPt->y) / (*dist);
    inc_xyz[2] = (pEndPt->z - pStartPt->z) / (*dist);
    WorldPoint copyStartPt = *pStartPt;
    WorldPoint copyEndPt = *pEndPt;
    WorldPoint blockStartPt;
    WorldPoint blockEndPt;
    double closest = *dist;
    MapObject *pBlocker = NULL;

    // if shooter is a Ped shooting from a vehicle, skip that vehicle
    Vehicle *pShooterVehicle = NULL;
    if (pOrigin && pOrigin->is(MapObject::kNaturePed)) {
        const PedInstance *pPed = static_cast<const PedInstance *>(pOrigin);
        pShooterVehicle = pPed->inVehicle(); // can be null
    }

    // Test one object against the ray and update the closest blocker if hit.
    auto testObject = [&](MapObject* obj) {
        if (static_cast<const MapObject*>(obj) == static_cast<const MapObject*>(pOrigin)) return;
        if (obj == pShooterVehicle) return;
        // Peds may have died since the grid was built this tick
        if (obj->is(MapObject::kNaturePed)) {
            PedInstance *pPed = static_cast<PedInstance*>(obj);
            if (pPed->isDead()) return;
            if (pPed->isOurAgent()) {
                if (pOrigin && pOrigin->is(MapObject::kNaturePed) &&
                    static_cast<const PedInstance*>(pOrigin)->isOurAgent()) {
                        // no friendly fire
                        return;
                }
            }
        }
        if (obj->isBlocker(&copyStartPt, &copyEndPt, inc_xyz)) {
            int cx = pStartPt->x - copyStartPt.x;
            int cy = pStartPt->y - copyStartPt.y;
            int cz = pStartPt->z - copyStartPt.z;
            double dist_blocker = sqrt((double)(cx * cx + cy * cy + cz * cz));
            if (closest == -1 || dist_blocker < closest) {
                closest = dist_blocker;
                pBlocker = obj;
                blockStartPt = copyStartPt;
                blockEndPt = copyEndPt;
            }
            copyStartPt = *pStartPt;
            copyEndPt = *pEndPt;
        }
    };

    // Test all objects present in a single grid cell.
    auto testCell = [&](int idx) {
        for (MapObject* obj : staticSpatialGrid_[idx]) testObject(obj);
        for (MapObject* obj : dynamicSpatialGrid_[idx]) testObject(obj);
    };

    // Step along the ray (same stepping method as checkBlockedByTile) and visit
    // each tile the ray crosses, testing only the objects present in that tile.
    double sx = (double)pStartPt->x;
    double sy = (double)pStartPt->y;
    double sz = (double)pStartPt->z;
    const double incrX = inc_xyz[0] * 8.0;
    const double incrY = inc_xyz[1] * 8.0;
    const double incrZ = inc_xyz[2] * 8.0;
    const double dist_dec = 8.0;
    double dist_close = *dist;
    int oldtx = -1, oldty = -1, oldtz = -1;

    while (dist_close > dist_dec) {
        int ntx = (int)sx / 256;
        int nty = (int)sy / 256;
        int ntz = (int)sz / 128;
        if (ntx != oldtx || nty != oldty || ntz != oldtz) {
            if (ntx >= 0 && ntx < mmax_x_ && nty >= 0 && nty < mmax_y_
                    && ntz >= 0 && ntz < mmax_z_) {
                testCell(ntx + nty * mmax_x_ + ntz * mmax_m_xy);
            }
            oldtx = ntx; oldty = nty; oldtz = ntz;
        }
        sx += incrX;
        sy += incrY;
        sz += incrZ;
        dist_close -= dist_dec;
    }
    // Also test the end tile (may be missed by the stepping loop)
    {
        int ntx = pEndPt->x / 256, nty = pEndPt->y / 256, ntz = pEndPt->z / 128;
        if (ntx >= 0 && ntx < mmax_x_ && nty >= 0 && nty < mmax_y_ && ntz >= 0 && ntz < mmax_z_
                && (ntx != oldtx || nty != oldty || ntz != oldtz)) {
            testCell(ntx + nty * mmax_x_ + ntz * mmax_m_xy);
        }
    }

    if (pBlocker != NULL) {
        *pStartPt = blockStartPt;
        *pEndPt = blockEndPt;
        *dist = closest;
    }

    return pBlocker;
}

/*!
 * Verify that the path from originPosW to pTargetPosW is not blocked by a tile.
 * If such a tile exists, pTargetPosW is updated with the position of the blocking tile.
 * \param originPosW Path starting point
 * \param pTargetPosW Path end point
 * \param updateLoc Set to true to update pTargetPosW when blocking tile is found
 * \param distanceMax Maximum distance we cannot cross. If distanceMax is
 *   reached before pTargetPosW, then path is stopped.
 * \param pInitialDistance This is the distance between origin and initial target position
 * \return a bitmask indicating the type of result:
 *      - 0b(kBMaskBlockerTargetInRange) : target in range
 *      - 3b(8) : distanceMax is reached
 *      - 4b(kBMaskBlockerBlockedByTile): blocker tile, "pTargetLoc" is set
 *      - 5b(kBMaskBlockerTargetOutOfMap): out of visible reach
 */
uint8_t Mission::checkBlockedByTile(const WorldPoint & originPosW, WorldPoint *pTargetPosW,
                                  bool updateLoc, double distanceMax, double *pInitialDistance) {
    // TODO: some objects mid point is higher then map z
    assert(distanceMax >= 0);

    int cx = originPosW.x;
    int cy = originPosW.y;
    int cz = originPosW.z;
    if (cz > (mmax_z_ - 1) * 128)
        return kBMaskBlockerTargetOutOfMap;

    // This variable will store the target location as it may moves if
    // a tile blocks the path.
    WorldPoint tmpTargetWLoc = *pTargetPosW;

    if (tmpTargetWLoc.z > (mmax_z_ - 1) * 128)
        return kBMaskBlockerTargetOutOfMap;

    // This is the distance between the origin and the target
    double distanceToTarget = 0;
    distanceToTarget = sqrt((double)((tmpTargetWLoc.x - cx) * (tmpTargetWLoc.x - cx) + (tmpTargetWLoc.y - cy) * (tmpTargetWLoc.y - cy)
        + (tmpTargetWLoc.z - cz) * (tmpTargetWLoc.z - cz)));
    uint8_t block_mask = kBMaskBlockerTargetInRange;

    if (pInitialDistance)
        *pInitialDistance = distanceToTarget;
    if (distanceToTarget == 0)
        return block_mask;

    double sx = (double) cx;
    double sy = (double) cy;
    double sz = (double) cz;

    if (distanceToTarget >= distanceMax) {
        // the distance we have to cross (distanceToTarget) is higher than the maximum
        // distance we are allowed to cross (distanceMax)

        // update target position according to distanceMax
        double dist_k = (double)distanceMax / distanceToTarget;
        tmpTargetWLoc.x = cx + (int)((tmpTargetWLoc.x - cx) * dist_k);
        tmpTargetWLoc.y = cy + (int)((tmpTargetWLoc.y - cy) * dist_k);
        tmpTargetWLoc.z = cz + (int)((tmpTargetWLoc.z - cz) * dist_k);
        // set mask to indicate distanceMax is reached
        block_mask = 8;
        if (updateLoc) {
            *pTargetPosW = tmpTargetWLoc;
        }
        distanceToTarget = distanceMax;
    }

    // NOTE: these values are less then 1.
    // If they are incremented, time required to check range will be shorter but less precise check,
    // If decremented longer but more precise.
    // Increment is (n * 8)
    double incrX = ((tmpTargetWLoc.x - cx) * 8) / distanceToTarget;
    double incrY = ((tmpTargetWLoc.y - cy) * 8) / distanceToTarget;
    double incrZ = ((tmpTargetWLoc.z - cz) * 8) / distanceToTarget;

    int oldx = cx / 256;
    int oldy = cy / 256;
    int oldz = cz / 128;
    double dist_close = distanceToTarget;
    // look note before, should be same increment
    double dist_dec = 1.0 * 8;

    while (dist_close > dist_dec) {
        int nx = (int)sx / 256;
        int ny = (int)sy / 256;
        int nz = (int)sz / 128;
        SurfaceType surfType = mtsurfaces_[nx + ny * mmax_x_
            + nz * mmax_m_xy];
        if (oldx != nx || oldy != ny || oldz != nz
            || (surfType >= SurfaceType::kSlopeSN && surfType <= SurfaceType::kSlopeWE))
        {
            if (!(surfType == SurfaceType::kEmpty || surfType == SurfaceType::kHandrailLight || surfType == SurfaceType::kTrainStop)) {
                bool is_blocked = false;
                int offz = (int)sz % 128;
                switch (surfType) {
                    case SurfaceType::kSlopeSN:
                        if (offz <= (127 - (((int)sy % 256) >> 1)))
                            is_blocked = true;
                        break;
                    case SurfaceType::kSlopeNS:
                        if (offz <= (((int)sy % 256) >> 1))
                            is_blocked = true;
                        break;
                    case SurfaceType::kSlopeEW:
                        if (offz <= (((int)sx % 256) >> 1))
                            is_blocked = true;
                        break;
                    case SurfaceType::kSlopeWE:
                        if (offz <= (127 - (((int)sx % 256) >> 1)))
                            is_blocked = true;
                        break;
                    default:
                        is_blocked = true;
                }
                if (is_blocked) {
                    sx -= incrX;
                    sy -= incrY;
                    sz -= incrZ;
                    double dsx = sx - (double)cx;
                    double dsy = sy - (double)cy;
                    double dsz = sz - (double)cz;
                    tmpTargetWLoc.x = (int)sx;
                    tmpTargetWLoc.y = (int)sy;
                    tmpTargetWLoc.z = (int)sz;
                    dist_close = sqrt(dsx * dsx + dsy * dsy + dsz * dsz);
                    // set mask to indicate path is blocked by a tile
                    if (block_mask == kBMaskBlockerTargetInRange)
                        block_mask = kBMaskBlockerBlockedByTile;
                    else
                        block_mask |= kBMaskBlockerBlockedByTile;
                    if (updateLoc) {
                        pTargetPosW->x = (int)sx;
                        pTargetPosW->y = (int)sy;
                        pTargetPosW->z = (int)sz;
                    }
                    break;
                }
            }
            oldx = nx;
            oldy = ny;
            oldz = nz;
        }
        sx += incrX;
        sy += incrY;
        sz += incrZ;
        dist_close -= dist_dec;
    } // end while

    return block_mask;
}

/*!
 * \param originLoc
 * \param pTarget
 * \param pTargetPosW
 * \param distTo
 * \return mask where bits are:
 *   - 0b : target in range(1)
 *   - 1b : blocker is object, pTarget is set(2)
 *   - 2b : blocker object, "pn" is set(4)
 *   - 3b : reachable point set (8)
 *   - 4b : blocker tile, "pn" is set(16)
 *   - 5b : out of visible reach(32)
 * NOTE: only if "pn" or "t" are not null, variables are set

*/
uint8_t Mission::checkIfBlockersInShootingLine(const WorldPoint & originLoc,  const BlockerCriteria crits, 
        ShootableMapObject **pTarget, WorldPoint *pTargetPosW, double * distTo)
{
    // search for a tile blocking the path towards the target
    // tmpPosW will hold the updated position after that search
    WorldPoint tmpPosW;
    if (pTarget && *pTarget) {
        tmpPosW.convertFromTilePoint((*pTarget)->position());
    } else {
        tmpPosW = *pTargetPosW;
    }

    uint8_t bfBlockerFound = checkBlockedByTile(originLoc, &tmpPosW, true, crits.maxr, distTo);
    if (bfBlockerFound == kBMaskBlockerTargetOutOfMap) {
        // coords are out of map limits
        return bfBlockerFound;
    }

    if (crits.setBlocker) {
        *pTargetPosW = tmpPosW;
    }

    if (crits.checkTileOnly)
        return bfBlockerFound;

    WorldPoint tmpOrigin = originLoc;
    WorldPoint tmpEnd = tmpPosW;

    // We search for a possible object blocking the way on the path
    // between origin and the reached position
    int dx = tmpPosW.x - originLoc.x;
    int dy = tmpPosW.y - originLoc.y;
    int dz = tmpPosW.z - originLoc.z;
    double distToBlocker = sqrt((double)(dx * dx + dy * dy + dz * dz));
    MapObject *blockerObj = checkBlockedByObject(&tmpOrigin, &tmpEnd, &distToBlocker, crits.pOrigin);

    if (blockerObj) {
        if (bfBlockerFound == kBMaskBlockerTargetInRange)
            bfBlockerFound = 0;

        if (crits.setBlocker) {
            if (pTargetPosW) {
                *pTargetPosW = tmpOrigin;
                bfBlockerFound |= kBMaskBlockerTargetPosUpdated;
            }
            if (pTarget) {
                *pTarget = (ShootableMapObject *)blockerObj;
                bfBlockerFound |= kBMaskBlockerTargetObjectUpdated;
            }
        } else {
            if (pTarget && *pTarget) {
                if (*pTarget != blockerObj)
                    bfBlockerFound |= kBMaskBlockerTargetObjectUpdated | kBMaskBlockerTargetPosUpdated;
                else
                    bfBlockerFound = kBMaskBlockerTargetInRange;
            } else
                bfBlockerFound |= kBMaskBlockerTargetObjectUpdated | kBMaskBlockerTargetPosUpdated;
        }
    } else {
        if (crits.setBlocker) {
            if (bfBlockerFound != kBMaskBlockerTargetInRange && pTarget)
                *pTarget = nullptr;
        }
    }

    return bfBlockerFound;
}

/*!
 * Returns all dynamic objects present at tile tile this tick.
 * The returned reference is valid until the next call to buildDynamicSpatialGrid().
 * @param tile Tile coordinate
 * @return Reference to the list of MapObject* at that tile in the dynamic grid.
 */
const std::vector<MapObject*>& Mission::getObjectsAtTile(const TilePoint & tile) const {
    return dynamicSpatialGrid_[tile.tx + tile.ty * mmax_x_ + tile.tz * mmax_m_xy];
}

/*!
 * Returns the length of the path between a ped and a object if such a path exists and it is
 * shorter than the maximum length allowed.
 * \param pPed The origin of the path
 * \param objectToReach The end of the path
 * \param maxLength The length of the path must not exceed this value
 * \param length The returned length if the path exists
 * \return 0 if a path exists, else path does not exist so length is not set.
 */
uint8_t Mission::getPathLengthBetween(PedInstance *pPed, ShootableMapObject* objectToReach, double maxLength, double *length) {
    WorldPoint cur_xyz(pPed->position());
    cur_xyz.z += (pPed->sizeZ() >> 1);
    // TODO : it's not inRangeCPos that must be called but a method for path calculation
    BlockerCriteria crits;
    crits.checkTileOnly = true;
    crits.maxr = maxLength;
    crits.pOrigin = pPed;
    uint8_t res = checkIfBlockersInShootingLine(cur_xyz, crits, &objectToReach, nullptr, length);
    return res == 1 ? 0 : 1;
}

constexpr bool isSolidSurface(SurfaceType type)
{
    return !(type == SurfaceType::kEmpty ||
            type == SurfaceType::kHandrailLight ||
            type == SurfaceType::kTrainStop);
}

SurfaceType Mission::surfaceAt(int x, int y, int z) const
{
    if (x < 0 || y < 0 || z < 0 || x >= mmax_x_ || y >= mmax_y_ || z >= mmax_z_)
        return SurfaceType::kUnknown;

    return static_cast<SurfaceType>(mtsurfaces_[x + y * mmax_x_ + z * mmax_m_xy]);
}


bool Mission::getShootableTile(TilePoint *pLocT) {
    bool gotIt = false;
    int bx, by, box, boy;
    int bz = mmax_z_;

    while (bz-- > 0 && !gotIt) {
        int bzm = bz - 1;

        bx = pLocT->tx * 256 + pLocT->ox + 128 * bzm;
        by = pLocT->ty * 256 + pLocT->oy + 128 * bzm;
        box = bx % 256;
        boy = by % 256;
        bx /= 256;
        by /= 256;

        const SurfaceType twd = surfaceAt(bx, by, bzm);
        int dx = 0, dy = 0;

        if (twd == SurfaceType::kSlopeSN) {
            dy = (boy * 2) / 3;
            dx = box - dy / 2;
            gotIt = (dx >= 0) || tryShiftX(bx, by, bzm, box, boy, -1, dx + 256, dy, SurfaceType::kSlopeSN);
        } else if (twd == SurfaceType::kSlopeNS) {
            dy = (boy - 128) * 2;
            dx = (box + dy / 2) - 128;
            if (dy >= 0)
                gotIt = (dx >= 0 && dx < 256) || tryShiftX(bx, by, bzm, box, boy, (dx < 0) ? -1 : 1, (dx + ((dx < 0) ? 256 : -256)), dy, SurfaceType::kSlopeNS);
        } else if (twd == SurfaceType::kSlopeEW) {
            dx = (box - 128) * 2;
            dy = (boy + dx / 2) - 128;
            if (dx >= 0)
                gotIt = (dy >= 0 && dy < 256) || tryShiftY(bx, by, bzm, box, boy, (dy < 0) ? -1 : 1, dx, (dy + ((dy < 0) ? 256 : -256)), SurfaceType::kSlopeEW);
        } else if (twd == SurfaceType::kSlopeWE) {
            dx = (box * 2) / 3;
            dy = boy - dx / 2;
            gotIt = (dy >= 0) || tryShiftY(bx, by, bzm, box, boy, -1, dx, dy + 256, SurfaceType::kSlopeWE);
        } else {
            gotIt = isSolidSurface(twd);
        }

        if (!gotIt)
            gotIt = tryNeighbourAdjustments(bx, by, bzm, box, boy);
    }

    if (gotIt) {
        TilePoint tempTile(bx, by, bz, box, boy);
        finalizeTile(tempTile, pLocT);
    }

    return gotIt;
}


bool Mission::tryShiftX(int &bx, int by, int bzm, int &box, int &boy, int shift, int newBox, int newBoy, SurfaceType expected) {
    int newBx = bx + shift;
    if (surfaceAt(newBx, by, bzm) == expected) {
        bx = newBx;
        box = newBox;
        boy = newBoy;
        return true;
    }
    return false;
}

bool Mission::tryShiftY(int bx, int &by, int bzm, int &box, int &boy, int shift, int newBox, int newBoy, SurfaceType expected) {
    int newBy = by + shift;
    if (surfaceAt(bx, newBy, bzm) == expected) {
        by = newBy;
        box = newBox;
        boy = newBoy;
        return true;
    }
    return false;
}

bool Mission::tryNeighbourAdjustments(int &bx, int &by, int bzm, int &box, int &boy) {
    auto check = [&](int offsetX, int offsetY) -> bool {
        const SurfaceType twd = surfaceAt(bx + offsetX, by + offsetY, bzm);
        if (twd == SurfaceType::kSlopeSN || twd == SurfaceType::kSlopeWE) {
            int dx = (twd == SurfaceType::kSlopeSN) ? ((boy + 256 * (offsetY != 0)) * 2) / 3 : ((box + 256 * (offsetX != 0)) * 2) / 3;
            int dy = (twd == SurfaceType::kSlopeSN) ? (box + 256 * (offsetX != 0)) - dx / 2 : (boy + 256 * (offsetY != 0)) - dx / 2;

            if (dx >= 0 && dx < 256 && dy >= 0 && dy < 256) {
                bx += offsetX;
                by += offsetY;
                box = (twd == SurfaceType::kSlopeSN) ? dy : dx;
                boy = (twd == SurfaceType::kSlopeSN) ? dx : dy;
                return true;
            }
        }
        return false;
    };

    return check(-1, 0) || check(0, -1) || check(-1, -1);
}

void Mission::finalizeTile(TilePoint tempTile, TilePoint *pLocT) {
    SurfaceType twd = surfaceAt(tempTile.tx, tempTile.ty, tempTile.tz - 1);

    switch (twd)
    {
    case SurfaceType::kSlopeSN:
        pLocT->oz = 127 - (tempTile.oy >> 1);
        --tempTile.tz;
        break;
    case SurfaceType::kSlopeNS:
        pLocT->oz = tempTile.oy >> 1;
        --tempTile.tz;
        break;
    case SurfaceType::kSlopeEW:
        pLocT->oz = tempTile.ox >> 1;
        --tempTile.tz;
        break;
    case SurfaceType::kSlopeWE:
        pLocT->oz = 127 - (tempTile.ox >> 1);
        --tempTile.tz;
        break;
    default:
        finalizeDefault(tempTile, pLocT);
        break;
    }

    pLocT->tx = tempTile.tx;
    pLocT->ty = tempTile.ty;
    pLocT->tz = tempTile.tz;
    pLocT->ox = tempTile.ox;
    pLocT->oy = tempTile.oy;

    assert(tempTile.tz >= 0);
}

void Mission::finalizeDefault(TilePoint &tempTile, TilePoint *pLocT) {
    SurfaceType twd = surfaceAt(tempTile.tx, tempTile.ty, tempTile.tz);
    if (isSolidSurface(twd))
    {
        pLocT->oz = (tempTile.ox > 192 || tempTile.oy > 192) ? (((tempTile.ox >= tempTile.oy) ? (256 - tempTile.ox) : (256 - tempTile.oy)) << 1) : 128;
        tempTile.tx = (pLocT->tx * 256 + pLocT->ox + 128 * (tempTile.tz - 1) + pLocT->oz) / 256;
        tempTile.ox = (pLocT->tx * 256 + pLocT->ox + 128 * (tempTile.tz - 1) + pLocT->oz) % 256;
        tempTile.ty = (pLocT->ty * 256 + pLocT->oy + 128 * (tempTile.tz - 1) + pLocT->oz) / 256;
        tempTile.oy = (pLocT->ty * 256 + pLocT->oy + 128 * (tempTile.tz - 1) + pLocT->oz) % 256;
        tempTile.tz += pLocT->oz / 128;
        pLocT->oz %= 128;
    }
    else
    {
        pLocT->oz = 0;
    }
}

/*bool Mission::isTileSolid(const TilePoint &point) {
    SurfaceType surface = surfaceAt(point.tx, point.ty, point.tz);
    switch (surface) {
        case 0x00:
        case 0x0C:
        case 0x10:
            solid = false;
            break;
        case 0x01:
            if (oz > (127 - (oy >> 1)))
                solid = false;
            break;
        case 0x02:
            if (oz > (oy >> 1))
                solid = false;
            break;
        case 0x03:
            if (oz > (ox >> 1))
                solid = false;
            break;
        case 0x04:
            if (oz > (127 - (ox >> 1)))
                solid = false;
            break;
        case SurfaceType::kRoadMark:
            return true;
        default:
            return false;
    }
}*/

/**
 * Gets the linear index for a tile in the map arrays.
 */
int Mission::getTileIndex(const TilePoint &point) const {
    return point.tx + point.ty * mmax_x_ + point.tz * mmax_m_xy;
}

/**
 * Checks if a tile at the given index is marked as walkable.
 */
bool Mission::isTileWalkable(int tileIndex) const {
    return (mdpoints_[tileIndex].flags & kWalkable) == kWalkable;
}

}