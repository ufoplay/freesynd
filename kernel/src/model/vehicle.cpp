/*
 *  FreeSynd - a remake of the classic Bullfrog game "Syndicate".
 *
 *   Copyright (C) 2005  Stuart Binge  <skbinge@gmail.com>
 *   Copyright (C) 2005  Joost Peters  <joostp@users.sourceforge.net>
 *   Copyright (C) 2006  Trent Waddington <qg@biodome.org>
 *   Copyright (C) 2006  Tarjei Knapstad <tarjei.knapstad@gmail.com>
 *   Copyright (C) 2010  Bohdan Stelmakh <chamel@users.sourceforge.net>
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

#include "fs-kernel/model/vehicle.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <queue>

#include "fs-utils/log/log.h"
#include "fs-engine/gfx/animationmanager.h"
#include "fs-kernel/model/mission.h"
#include "fs-kernel/model/shot.h"
#include "fs-kernel/mgr/missionmanager.h"

namespace fs_knl {

const uint8_t Vehicle::kVehicleTypeLargeArmored = 0x01;
const uint8_t Vehicle::kVehicleTypeLargeArmoredDamaged = 0x04;
const uint8_t Vehicle::kVehicleTypeTrainHead = 0x05;
const uint8_t Vehicle::kVehicleTypeTrainBody = 0x09;
const uint8_t Vehicle::kVehicleTypeRegularCar = 0x0D;
const uint8_t Vehicle::kVehicleTypeFireFighter = 0x11;
const uint8_t Vehicle::kVehicleTypeSmallArmored = 0x1C;
const uint8_t Vehicle::kVehicleTypePolice = 0x24;
const uint8_t Vehicle::kVehicleTypeMedics = 0x28;

const float GenericCar::kInfiniteDistance = 100000.0f;

void Vehicle::draw(const Point2D &screenPos)
{
    Point2D posWithOffs = screenPos;
    posWithOffs.y += fs_eng::Tile::kTileHeight / 3;
    posWithOffs = addOffs(posWithOffs);

    // ensure on map
    if (posWithOffs.x < 90 || posWithOffs.y < -20)
        return;

    //offset anim depends on direction
    // for regular animation there is an empty animation between
    // each animation so we skip one out of 2
    uint16_t offsetAnim = getDiscreteDirection(4);
    if (animationPlayer_->isCurrentAnimation(regularAnimation_)) {
        offsetAnim *= 2;
    }
    animationPlayer_->draw(posWithOffs, offsetAnim);
}

void Vehicle::doUpdateState(uint32_t elapsed) {
    if (health_ > 0) {
        doMove(elapsed);
    }
}

/**
 * Adds given ped to the list of passengers.
 * \param pPed PedInstance*
 * \return void
 *
 */
void Vehicle::addPassenger(PedInstance *pPed) {
    if(!containsPed(pPed)) {
        passengers_.push_back(pPed);
        pPed->putInVehicle(this);
    }
}

/*!
 * Removes given passenger from vehicle.
 * \param pPed Ped to remove
 */
void Vehicle::dropPassenger(PedInstance *pPed) {
    for (std::list<PedInstance *>::iterator it = passengers_.begin();
        it != passengers_.end(); it++)
        {
            if ((*it)->id() == pPed->id()) {
                pPed->leaveVehicle();
                passengers_.erase(it);
                return;
            }
        }
}

/*!
 * Returns true if at least one of our agent is inside the vehicle.
 */
bool Vehicle::containsOurAgents() {
    for (std::list<PedInstance *>::iterator it = passengers_.begin();
        it != passengers_.end(); it++)
    {
        if ((*it)->isOurAgent()) {
            return true;
        }
    }
    return false;
}

/*!
 * Returns true if the vehicle contains peds considered hostile by the given ped.
 * \param pPed The ped evaluating the hostility of the vehicle
 * \param hostile_desc_alt Parameter for evaluating the hostility
 * \return True if at least one hostile ped is found.
 */
bool Vehicle::containsHostilesForPed(PedInstance* p,
                                          unsigned int hostile_desc_alt)
{
    for (std::list<PedInstance *>::iterator it = passengers_.begin();
        it != passengers_.end(); it++)
    {
        if (p->isHostileTo((ShootableMapObject *)(*it), hostile_desc_alt))
            return true;
    }
    return false;
}

/*!
 * Initialize the animations to use for this vehicle
 * @param baseSpriteAnimationId the base animation id to use
 */
void Vehicle::setAnimations(uint16_t baseSpriteAnimationId) {
    regularAnimation_ = animationPlayer_->addAnimation(baseSpriteAnimationId);

    burningAnimation_ = 
        animationPlayer_->addAnimation(baseSpriteAnimationId + 8,
                                       fs_eng::kAnimationModeLoop, 8, 10000);

    burntAnimation_ = animationPlayer_->addAnimation(baseSpriteAnimationId + 12);
}

void Vehicle::handleAnimationEnded() {
    if (animationPlayer_->isCurrentAnimation(burningAnimation_)) {
        // At the end of burning animation, there is the burnt animation
        animationPlayer_->play(burntAnimation_);
    }
}

GenericCar::GenericCar(uint16_t anId, uint8_t aType, Map *pMap, int maxSpeed):
    Vehicle(anId, aType, pMap, maxSpeed) {
    pDriver_ = NULL;
    unblockPath();
}

/*!
 * Computes a path on the road network from the car's current position to \p destinationPt
 * and stores it in dest_path_.
 *
 * \par Algorithm
 * Implements an \b A* search on the road tile graph. At each iteration the open node with
 * the smallest f-score (f = g + h) is expanded, where g is the cumulative tile cost from
 * the start and h is the Manhattan distance to the destination. The heuristic is admissible
 * (never overestimates), so the algorithm always finds the shortest path when one exists.
 * A min-heap is used for O(log n) node selection; stale heap entries are discarded via lazy
 * deletion when a node is popped after already being closed.
 *
 * \par Road constraints
 * Expansion is limited to tiles that are connected by compatible road exits as determined 
 * by Map::getPossibleConnexionsForRoadTile(). 
 *
 * \par Off-road recovery
 * If the car's current tile is not drivable (e.g. parked on non road), findPathToNearestWalkableTile()
 * is called first to build a short recovery path; this recovery path is prepended to the main path.
 *
 * \par Watchdog
 * If the search exceeds kPathfindingWatchdog iterations without reaching the goal, method returns false
 * to indicate that no path to destination was found. With A* this should only trigger on
 * pathologically disconnected road networks.
 *
 * \param pMission Current mission (unused in the body — kept for interface compatibility).
 * \param destinationPt Target tile point (tx, ty, tz, ox, oy).
 * \return true if a non-empty path was computed; false if the destination is unreachable.
 */
bool GenericCar::initMovementToDestination([[maybe_unused]] Mission *pMission, const TilePoint &destinationPt) {
    // A* search on the road network.
    // At each step, expand the open node with the smallest f-score = g + h,
    // where g is the cumulative tile cost and h is the Manhattan distance to the destination.

    // Min-heap ordered by f-score (smallest first).
    using OpenEntry = std::pair<int, TilePoint>;
    std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<OpenEntry>> openQueue;
    // Cumulative cost from the start node to each visited node
    std::map<TilePoint, int> gScore;

    std::set < TilePoint > closed;
    std::map < TilePoint, TilePoint > parent; // child → parent, for path reconstruction

    TilePoint destPt(destinationPt);

    pMap_->clip(&destPt);
    // Road tiles sit one level below the vehicle's visual Z coordinate.
    // All pathfinding is done at (pos_.tz - 1); the real tz is restored at the end.
    destPt.tz = pos_.tz - 1;

    clearDestination();

    if (!isDrawable() || isDead()) {
        LOG(Log::k_FLG_GAME, "GenericCar", "initMovementToDestination", ("Car is invisible or dead"))
        return false;
    }

    if (!(pMap_->isTileWalkableByCar(destPt.tx, destPt.ty, destPt.tz))) {
        LOG(Log::k_FLG_GAME, "GenericCar", "initMovementToDestination", ("Destination point is not walkable by car %d : %d, %d, %d", id(), destPt.tx, destPt.ty, destPt.tz))
        return false;
    }

    // Effective start tile (may be updated if the car is currently off-road)
    TilePoint startNode(pos_.tx , pos_.ty, destPt.tz, pos_.ox, pos_.oy);

    // If vehicle is on a non drivable place, first set a path to a drivable tile
    // Recovery path prepended when the car starts on a non-drivable tile
    std::vector < TilePoint > recoveryPath;
    recoveryPath.reserve(kMaxWalkableSearchRadius);
    if (!pMap_->isTileWalkableByCar(pos_.tx, pos_.ty, destPt.tz)) {
        if(!findPathToNearestWalkableTile(startNode, recoveryPath)) {
            return false;
        }
    }

    // Seed the open set with the start tile.
    gScore[startNode] = 0;
    int hStart = abs(destPt.tx - startNode.tx) + abs(destPt.ty - startNode.ty);
    openQueue.push({hStart, startNode});

    int watchDog = kPathfindingWatchdog;

    while (!openQueue.empty()) {
        watchDog--;

        // --- Pop the node with the lowest f-score = g + h ---
        auto [fCurrent, p] = openQueue.top();
        openQueue.pop();

        // Lazy deletion: skip nodes already expanded (stale heap entries)
        if (closed.count(p)) continue;

        closed.insert(p);

        // --- Goal test (or watchdog expiry → use best node reached so far) ---
        if (p.isSameTile(destPt) || watchDog < 0) {
            if (watchDog < 0) {
                LOG(Log::k_FLG_GAME, "GenericCar", "initMovementToDestination", ("Hit wathdog before finding path\n"))
                break;
            } else {
                dest_path_.push_front(TilePoint(destPt));
            }

            // Reconstruct path by following parent links back to the start tile
            while (parent.find(p) != parent.end()) {
                p = parent[p];
                if (p.tx == pos_.tx && p.ty == pos_.ty && p.tz == destPt.tz)
                    break;
                dest_path_.push_front(p);
            }
            break;
        }

        // Get candidate amoung neigbours
        std::list<TilePoint> candidateNeighbors;
        expandCandidateNeighbours(p, candidateNeighbors);

        int gCurrent = gScore.count(p) ? gScore[p] : 0;

        if (candidateNeighbors.empty()) {
            LOG(Log::k_FLG_GAME, "GenericCar", "initMovementToDestination", ("No neigbours for car %d at point %d, %d, %d\n", id(), p.tx, p.ty, p.tz))
        }

        for (auto& neighbor : candidateNeighbors) {
            if (closed.count(neighbor)) {
                continue;
            }

            int gNew = gCurrent + 1;

            // Only enqueue if we found a strictly better path to this neighbour
            if (!gScore.count(neighbor) || gNew < gScore[neighbor]) {
                gScore[neighbor] = gNew;
                parent[neighbor] = p;

                int h = abs(destPt.tx - neighbor.tx) + abs(destPt.ty - neighbor.ty);
                openQueue.push({gNew + h, neighbor});
            }
        }
    }

    if(!dest_path_.empty()) {
        setSpeedToMax();
        addIntraTileOffsetsToPath();

        // Prepend recovery path (computed if the car started off a drivable tile)
        if (!recoveryPath.empty()) {
            for (std::vector < TilePoint >::reverse_iterator it = recoveryPath.rbegin();
                it != recoveryPath.rend(); it++)
            {
                it->tz = pos_.tz;
                dest_path_.push_front(*it);
            }
        }
    }
    
    return !dest_path_.empty();
}

/*!
 * Find only road-adjacent tiles that respect driving direction
 * @param p is the current tile for which we look candidates
 * @param candidateNeighbors a map of candidates
 */
void GenericCar::expandCandidateNeighbours(const TilePoint &p, std::list<TilePoint> & candidateNeighbors) {
    uint8_t possibleConnexions = pMap_->getPossibleConnexionsForRoadTile(p);
    
    if (fs_utl::isBitsOnWithMask(possibleConnexions, Map::kConnexionMaskExitNorth)) {
        candidateNeighbors.push_back(TilePoint(p.tx, p.ty - 1, p.tz));
    }

    if (fs_utl::isBitsOnWithMask(possibleConnexions, Map::kConnexionMaskExitSouth)) {
        candidateNeighbors.push_back(TilePoint(p.tx, p.ty + 1, p.tz));
    }

    if (fs_utl::isBitsOnWithMask(possibleConnexions, Map::kConnexionMaskExitEast)) {
        candidateNeighbors.push_back(TilePoint(p.tx + 1, p.ty, p.tz));
    }

    if (fs_utl::isBitsOnWithMask(possibleConnexions, Map::kConnexionMaskExitWest)) {
        candidateNeighbors.push_back(TilePoint(p.tx - 1, p.ty, p.tz));
    }
}

/*!
 * Adjust intra-tile offsets (ox, oy) so the car stays centered in the correct lane.
 * Use of kLaneOffsetLow/High to position the car on the appropriate side of the 
 * road based on driving direction.
 */
void GenericCar::addIntraTileOffsetsToPath() {
    TilePoint previous = pos_;

    for(std::list < TilePoint >::iterator it = dest_path_.begin();
        it != dest_path_.end(); it++) {
        if (it->tx == previous.tx + 1) { // moving from west to east
            it->ox = kLaneOffsetMiddle;
            it->oy = kLaneOffsetLow;
        } else if (it->tx == previous.tx - 1) { // moving from east to west
            it->ox = kLaneOffsetMiddle;
            it->oy = kLaneOffsetHigh;
        } else if (it->ty == previous.ty + 1) { // moving from north to south
            it->ox = kLaneOffsetHigh;
            it->oy = kLaneOffsetMiddle;
        } else if (it->ty == previous.ty - 1) { // moving from south to north
            it->ox = kLaneOffsetLow;
            it->oy = kLaneOffsetMiddle;
        }

        auto next = std::next(it, 1);
        if (next != dest_path_.end()) {
            if (it->tx == next->tx + 1) { // moving from east to west
                it->oy = kLaneOffsetHigh;
            } else if (it->tx == next->tx - 1) { // moving from west to east
                it->oy = kLaneOffsetLow;
            } else if (it->ty == next->ty + 1) { // moving from south to north
                it->ox = kLaneOffsetLow;
            } else if (it->ty == next->ty - 1) { // moving from north to south
                it->ox = kLaneOffsetHigh;
            }
        }

        // Restore real vehicle Z coordinate (road tiles are at pos_.tz - 1)
        it->tz = pos_.tz;
        previous = *it;
    }
}

/*!
 * Finds the nearest drivable road tile to the car's current position and builds a
 * straight-line recovery path to reach it.
 *
 * Called when the car is on a non-drivable tile. Scans up to kMaxWalkableSearchRadius tiles
 * in each of the four cardinal directions (+X, -X, -Y, +Y) and keeps the candidate with the
 * smallest squared distance (to avoid sqrt). The winning path is stored in \p recoveryPath
 * and the tile coordinates of its end point are written into \p startTx / \p startTy so that
 * the main pathfinder can continue from there.
 *
 * \param startPt     The car's current position (off-road).
 * \param startTx     [out] X tile coordinate of the nearest walkable tile found.
 * \param startTy     [out] Y tile coordinate of the nearest walkable tile found.
 * \param recoveryPath [out] Sequence of tile points leading to the nearest walkable tile.
 * \return true if a walkable tile was found within the search radius; false otherwise.
 */
bool GenericCar::findPathToNearestWalkableTile(TilePoint &startPt, std::vector < TilePoint > &recoveryPath) {
    // Scan up to kMaxWalkableSearchRadius tiles in each of the 4 cardinal directions
    // to find the closest drivable tile. Uses squared distance to avoid sqrt.
    int bestDist = (int)kInfiniteDistance, curDist;
    std::vector < TilePoint > candidatePath;
    candidatePath.reserve(kMaxWalkableSearchRadius);
    // we got somewhere we shouldn't, we need to find somewhere that is walkable
    TilePoint probePoint = startPt;
    // Store the final starting point
    int startTx = startPt.tx;
    int startTy = startPt.ty;
    for (int i = 1; i < kMaxWalkableSearchRadius; i++) {
        if (pos_.tx + i >= pMap_->maxTx())
            break;
        probePoint.tx = pos_.tx + i;
        candidatePath.push_back(probePoint);
        if (pMap_->isTileWalkableByCar(pos_.tx + i, pos_.ty, startPt.tz)) {
            curDist = i * i;
            if(curDist < bestDist) {
                bestDist = curDist;
                recoveryPath.assign(candidatePath.begin(), candidatePath.end());
                startTx = pos_.tx + i;
                startTy = pos_.ty;
                break;
            }
        }
    }

    candidatePath.clear();
    probePoint = startPt;
    for (int i = -1; i > -kMaxWalkableSearchRadius; --i) {
        if (pos_.tx + i < 0)
            break;
        probePoint.tx = (pos_.tx + i);
        candidatePath.push_back(probePoint);
        if (pMap_->isTileWalkableByCar(pos_.tx + i, pos_.ty, startPt.tz)) {
            curDist = i * i;
            if(curDist < bestDist) {
                bestDist = curDist;
                recoveryPath.assign(candidatePath.begin(), candidatePath.end());
                startTx = pos_.tx + i;
                startTy = pos_.ty;
                break;
            }
        }
    }

    candidatePath.clear();
    probePoint = startPt;
    for (int i = -1; i > -kMaxWalkableSearchRadius; --i) {
        if (pos_.ty + i < 0)
            break;
        probePoint.ty = (pos_.ty + i);
        candidatePath.push_back(probePoint);
        if (pMap_->isTileWalkableByCar(pos_.tx, pos_.ty + i, startPt.tz)) {
            curDist = i * i;
            if(curDist < bestDist) {
                bestDist = curDist;
                recoveryPath.assign(candidatePath.begin(), candidatePath.end());
                startTx = pos_.tx;
                startTy = pos_.ty + i;
                break;
            }
        }
    }

    candidatePath.clear();
    probePoint = startPt;
    for (int i = 1; i < kMaxWalkableSearchRadius; i++) {
        if (pos_.ty + i >= pMap_->maxTy())
            break;
        probePoint.ty = pos_.ty + i;
        candidatePath.push_back(probePoint);
        if (pMap_->isTileWalkableByCar(pos_.tx, pos_.ty + i, startPt.tz)) {
            curDist = i * i;
            if(curDist < bestDist) {
                bestDist = curDist;
                recoveryPath.assign(candidatePath.begin(), candidatePath.end());
                startTx = pos_.tx;
                startTy = pos_.ty + i;
                break;
            }
        }
    }

    startPt.tx = startTx;
    startPt.ty = startTy;
    return (bestDist != (int)kInfiniteDistance);
}

/*!
 * @return true if this vehicle is currently blocked by something
 */
bool GenericCar::isBlocked() {
    if (hold_on_.pathBlocker != nullptr) { // blocked by something
        if (hold_on_.terminatePath) {
            // Must stop : clear destination and stop
            clearDestination();
            return true;
        }
        if (hold_on_.pathBlocker->is(MapObject::kNatureVehicle)) {
            // For vehicle blockers: check if the blocking car has left the tile
            if (hold_on_.pathBlocker->tileX() == hold_on_.tilex
                && hold_on_.pathBlocker->tileY() == hold_on_.tiley) {
                return true;  // still occupying the tile — wait
            }
            unblockPath();  // blocker has moved away — resume
        } else if (hold_on_.pathBlocker->is(MapObject::kNaturePed)) {
            // For ped blockers on a crossroad: resume when the ped has left or died
            PedInstance *pBlockingPed = static_cast<PedInstance *>(hold_on_.pathBlocker);
            if (!pBlockingPed->isAlive()
                    || pBlockingPed->tileX() != hold_on_.tilex
                    || pBlockingPed->tileY() != hold_on_.tiley) {
                unblockPath();  // ped has cleared the tile — resume
            } else {
                return true;  // ped still on the crossroad — wait
            }
        } else {
            return true;  // non-vehicle blocker (door, obstacle…) — unchanged behaviour
        }
    }

    return false;
}

/*!
 * Check if the next tile is already occupied by another vehicle,
 * or if it is a crossroad tile with living pedestrians (for civilian/police drivers)
 * @param checkForCrossings True to check if peds crossing the road
 * @return true if there is a blocker ahead
 */
bool GenericCar::checkForBlockers(bool checkForCrossings) {
    Mission *pMission = g_missionCtrl.mission();
    assert(pMission != nullptr);

    // Determine the effective waypoint to check for blockers.
    // If the vehicle's leading edge has already passed nextPt
    // (dist(center, nextPt) < vehicle half-size), advance to dest_path_[1]
    // so that the vehicle stops before its body overlaps the blocker's tile.
    auto it = dest_path_.begin();
    if (dest_path_.size() > 1) {
        const TilePoint &nextPt = *it;
        int diffx = nextPt.tx * 256 + nextPt.ox - (pos_.tx * 256 + pos_.ox);
        int diffy = nextPt.ty * 256 + nextPt.oy - (pos_.ty * 256 + pos_.oy);
        int halfSize = std::max(sizeX(), sizeY());
        if (diffx * diffx + diffy * diffy < halfSize * halfSize) {
            ++it;  // vehicle front is past nextPt → check the following waypoint
        }
    }

    const TilePoint &checkPt = *it;
    const std::vector<MapObject*> &occupants =
        pMission->getObjectsAtTile(checkPt);
    for (MapObject *obj : occupants) {
        if (obj != this && obj->is(MapObject::kNatureVehicle)) {
            Vehicle *pBlocker = static_cast<Vehicle *>(obj);
            if (pBlocker->isBlockedBy(this)) { 
                // trying to prevent interblocking of two cars
                // by no blocking the second one
                continue; }
            // Dead vehicles won't move: terminate the path so the car recomputes a route
            bool terminate = pBlocker->isDead();
            blockPathWith(obj, terminate,
                          checkPt.tx, checkPt.ty, checkPt.tz);
            return true;
        }
    }
    if (checkForCrossings) {
        // Road is one level below the car
        fs_eng::Tile *pTile = pMap_->getTileAt(checkPt.tx, checkPt.ty, checkPt.tz - 1);
        if (pTile->isPedCrossing()) {
            for (MapObject *obj : occupants) {
                if (obj->is(MapObject::kNaturePed)) {
                    PedInstance *pPed = static_cast<PedInstance *>(obj);
                    if (pPed->isAlive()) {
                        blockPathWith(pPed, false,
                                      checkPt.tx, checkPt.ty, checkPt.tz);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

/*!
 * Moves a vehicle on the map.
 * \param elapsed Elapsed time sine last frame.
 */
bool GenericCar::doMove(uint32_t elapsed) {
    bool updated = false;
    int used_time = static_cast<int>(elapsed);

    // Civilian and police drivers yield to pedestrians on crossroad tiles
    PedInstance *pDriver = getDriver();
    bool driverYieldsToCrossing = pDriver != nullptr
        && (pDriver->type() == PedInstance::kPedTypeCivilian
            || pDriver->type() == PedInstance::kPedTypePolice);

    while (!dest_path_.empty() && used_time != 0) {
        if (isBlocked()) {
            return false;
        }

        // Check if there are any obstacle to stop the vehicle
        if (checkForBlockers(driverYieldsToCrossing)) {
            return false;
        }

        const TilePoint &nextPt = dest_path_.front();
        int adx = nextPt.tx * 256 + nextPt.ox;
        int ady = nextPt.ty * 256 + nextPt.oy;
        int diffx = adx - (pos_.tx * 256 + pos_.ox);
        int diffy = ady - (pos_.ty * 256 + pos_.oy);

        if (abs(diffx) >= 16 || abs(diffy) >= 16) {
            // Not yet at waypoint: advance toward it
            setDirection(diffx, diffy, &dir_);
            int dx = 0, dy = 0;
            double d = sqrt((double)(diffx * diffx + diffy * diffy));
            // This is the time for all the remaining distance to the node
            double avail_time_use = (d / (double)speed()) * 1000.0;
            // correcting time available regarding the time we have
            if (avail_time_use > used_time)
                avail_time_use = used_time;

            // computes distance travelled by vehicle in the available time
            if (abs(diffx) > 0)
                dx = (int)((diffx * (speed() * avail_time_use) / d) / 1000);
            if (abs(diffy) > 0)
                dy = (int)((diffy * (speed() * avail_time_use) / d) / 1000);

            // Updates the available time
            if (dx || dy) {
                int prv_time = used_time;
                if (dx) {
                    used_time -= (int)(((double) dx * 1000.0 * d)
                        / (double)(diffx * speed()));
                } else if (dy) {
                    used_time -= (int)(((double) dy * 1000.0 * d)
                        / (double)(diffy * speed()));
                } else {
                    used_time = 0;
                }
                if (used_time < 0 || prv_time == used_time)
                    used_time = 0;
            } else {
                used_time = 0;
            }

            addOffsetToPosition(dx, dy);

            // Recompute diff after movement (addOffsetToPosition may have changed tx/ty)
            diffx = adx - (pos_.tx * 256 + pos_.ox);
            diffy = ady - (pos_.ty * 256 + pos_.oy);
        }

        // Unified arrival check: snap to waypoint and advance path
        if (abs(diffx) < 16 && abs(diffy) < 16) {
            pos_.tx = nextPt.tx; pos_.ty = nextPt.ty;
            pos_.ox = nextPt.ox; pos_.oy = nextPt.oy;
            dest_path_.pop_front();
            if (dest_path_.empty()) stop();
        }

        updated = true;
    }

    if (dest_path_.empty() && isMoving()) {
        FSERR(Log::k_FLG_GAME, "GenericCar", "doMove", ("Car has no destination but has speed : %i", speed()));
        stop();
    }

    // Update passengers position to be in sync with car position
    for (auto passenger : passengers_) {
        passenger->setPosition(pos_);
    }

    return updated;
}


/*!
 * Method called when object is hit by a weapon shot.
 * \param d Damage description
 */
void GenericCar::handleHit(DamageToInflict &d) {
    if (health_ <= 0)
        return;

    decreaseHealth(d.dvalue);
    if (health_ == 0) {
        clearDestination();
        switch (d.dtype) {
            case kDmgTypeBullet:
            case kDmgTypeLaser:
            case kDmgTypeBurn:
            case kDmgTypeExplosion:
                animationPlayer_->play(burningAnimation_);
                break;
            default:
                break;
        }
        pDriver_ = NULL;
        while (passengers_.size() != 0)
        {
            PedInstance *p = *(passengers_.begin());
            dropPassenger(p);
        }

        Explosion::createExplosion(g_missionCtrl.mission(), this, 512.0);
    } else if (pDriver_ != NULL && !pDriver_->isOurAgent()) {
        // in case the car is drived by someone else than our agents
        // and one of our agent shot the car then
        // the driver is ejected from the car
        // Usually he is alone in the car so don't bother with any passengers
        PedInstance *pShooter = dynamic_cast<PedInstance *>(d.d_owner);
        if (pShooter && pShooter->isOurAgent()) {
            PedInstance *pPed = pDriver_;
            dropPassenger(pPed);
            // Remove scripted actions for driving car
            pPed->destroyAllActions(true);
            // Make the ped start walking
            pPed->addToDefaultActions(new WalkToDirectionAction());
        }
    }
}

/*!
 * Adds the given ped to the passenger but if the vehicle
 * has no driver, ped becomes the driver.
 * \param p The ped
 */
void GenericCar::addPassenger(PedInstance *p) {
    Vehicle::addPassenger(p);
    if (pDriver_ == NULL && !p->isPersuaded()) {
        // Ped becomes the driver
        pDriver_ = p;
    }
}

/*!
 * Overload initial method to manage driver.
 * \param pPed The ped to remove.
 */
void GenericCar::dropPassenger(PedInstance *pPed) {
    Vehicle::dropPassenger(pPed);
    if (pDriver_ == pPed) {
        pDriver_ = NULL;
        clearDestination();

        // find another driver in the remaining passengers
        for (std::list<PedInstance *>::iterator it = passengers_.begin();
            it != passengers_.end(); it++) {
            // take the first non persuaded
            if (!(*it)->isPersuaded()) {
                pDriver_ = *it;
                break;
            }
        }
    }
}

/**
 * Set this ped as the driver of the vehicle and add him as a passenger
 * if he's not already in the vehicle.
 * \param pPed PedInstance*
 * \param forceDriver bool if true, set the driver even if there is already
 * another driver
 * \return void
 *
 */
void GenericCar::setDriver(PedInstance *pPed, bool forceDriver) {
    if (pPed != NULL) {
        if (pDriver_ == NULL || forceDriver) {
            pDriver_ = pPed;
        }

        if (!containsPed(pPed)) {
            Vehicle::addPassenger(pPed);
        }
    }
}

}