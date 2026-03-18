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

#include "fs-kernel/model/vehicle.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

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
        doMove(elapsed, NULL);
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
    hold_on_.wayFree = 0;
}

bool GenericCar::dirWalkable(TilePoint *p, int x, int y, int z) {
    if(!(pMap_->isTileWalkableByCar(x,y,z)))
        return false;

    uint16_t dirStart = pMap_->getPossibleDirectionsFromRoadTile(p->tx,p->ty,p->tz);
    uint16_t dirEnd = pMap_->getPossibleDirectionsFromRoadTile(x,y,z);
    if (dirStart == Map::kTileDirNone || dirEnd == Map::kTileDirNone)
        return false;
    if (dirStart == Map::kTileDirAll || dirEnd == Map::kTileDirAll)
        return true;

    // A transition is valid if both tiles share a matching non-blocked nibble
    // (i.e. the road exits align in at least one cardinal direction).
    if (((dirStart & Map::kDirMaskWest) != Map::kDirMaskWest)
        || ((dirEnd & Map::kDirMaskWest) != Map::kDirMaskWest))
        if ((dirStart & Map::kDirMaskWest) == (dirEnd & Map::kDirMaskWest))
                return true;
    if (((dirStart & Map::kDirMaskNorth) != Map::kDirMaskNorth)
        || ((dirEnd & Map::kDirMaskNorth) != Map::kDirMaskNorth))
        if ((dirStart & Map::kDirMaskNorth) == (dirEnd & Map::kDirMaskNorth))
                return true;
    if (((dirStart & Map::kDirMaskEast) != Map::kDirMaskEast)
        || ((dirEnd & Map::kDirMaskEast) != Map::kDirMaskEast))
        if ((dirStart & Map::kDirMaskEast) == (dirEnd & Map::kDirMaskEast))
                return true;
    if (((dirStart & Map::kDirMaskSouth) != Map::kDirMaskSouth)
        || ((dirEnd & Map::kDirMaskSouth) != Map::kDirMaskSouth))
        if ((dirStart & Map::kDirMaskSouth) == (dirEnd & Map::kDirMaskSouth))
                return true;

    return false;
}

uint16_t GenericCar::forbiddenDirFromCurrentHeading() {
    // Converts the car's current discrete heading (returned by getDiscreteDirection(4))
    // into the forbiddenDir mask that blocks the pathfinder from U-turning on the first step.
    // Heading values: 0=South, 1=West, 2=North, 3=East
    switch (getDiscreteDirection(4)) {
        case 0: return Map::kForbidDirNorth;  // heading South → forbid going back North
        case 1: return Map::kForbidDirWest;   // heading East  → forbid going back West
        case 2: return Map::kForbidDirSouth;  // heading North → forbid going back South
        case 3: return Map::kForbidDirEast;   // heading West  → forbid going back East
        default: return Map::kForbidDirNorth;
    }
}

/*!
 * Computes a path on the road network from the car's current position to \p destinationPt
 * and stores it in dest_path_.
 *
 * \par Algorithm
 * Implements a \b greedy \b best-first search (not A*): at each iteration the open node with
 * the smallest Euclidean distance to the destination is expanded, without accumulating a path
 * cost. This can produce sub-optimal paths on complex road layouts but is fast in practice.
 *
 * \par Road constraints
 * Expansion is limited to tiles that are both walkable by cars (isTileWalkableByCar()) and
 * connected by compatible road exits as determined by tileDir() and dirWalkable().
 * U-turns are prevented by forbiddenDirFromCurrentHeading() and the forbiddenDir field stored
 * per open node.
 *
 * \par Off-road recovery
 * If the car's current tile is not drivable (e.g. after a collision), findPathToNearestWalkableTile()
 * is called first to build a short recovery path; this recovery path is prepended to the main path.
 *
 * \par Watchdog
 * If the search exceeds kPathfindingWatchdog iterations without reaching the goal, the closest
 * node reached so far is used as a fallback destination.
 *
 * \param pMission Current mission (unused in the body — kept for interface compatibility).
 * \param destinationPt Target tile point (tx, ty, tz, ox, oy).
 * \return true if a non-empty path was computed; false if the destination is unreachable.
 */
bool GenericCar::initMovementToDestination(Mission *pMission, const TilePoint &destinationPt) {
    // Greedy best-first search on the road network.
    // At each step, expand the open node with the smallest Euclidean distance
    // to the destination (no cumulative cost — not A*).
    std::map < TilePoint, uint16_t > open;   // node → forbiddenDir of that node
    std::set < TilePoint > closed;
    std::map < TilePoint, TilePoint > parent; // child → parent, for path reconstruction

    // Effective start tile (may be updated if the car is currently off-road)
    int startTx = pos_.tx, startTy = pos_.ty;
    // Recovery path prepended when the car starts on a non-drivable tile
    std::vector < TilePoint > recoveryPath;
    recoveryPath.reserve(kMaxWalkableSearchRadius);

    int destTx = destinationPt.tx;
    int destTy = destinationPt.ty;
    int destTz = destinationPt.tz;
    int destOx = destinationPt.ox;
    int destOy = destinationPt.oy;

    pMap_->adjXYZ(destTx, destTy, destTz);
    // Road tiles sit one level below the vehicle's visual Z coordinate.
    // All pathfinding is done at (pos_.tz - 1); the real tz is restored at the end.
    destTz = pos_.tz - 1;

    clearDestination();

    if (!isDrawable() || isDead()) {
        LOG(Log::k_FLG_GFX, "GenericCar", "initMovementToDestination", ("Car is invisible or dead"))
        return false;
    }

    if (!(pMap_->isTileWalkableByCar(destTx, destTy, destTz))) {
        LOG(Log::k_FLG_GFX, "GenericCar", "initMovementToDestination", ("Destination point is not walkable by car %d : %d, %d, %d", id(), destTx, destTy, destTz))
        return false;
    }

    // If vehicle is on a non drivable place, first set a path to a drivable tile
    if (!pMap_->isTileWalkableByCar(pos_.tx, pos_.ty, destTz)) {
        TilePoint currentPos(pos_.tx , pos_.ty, destTz, pos_.ox, pos_.oy);

        if(!findPathToNearestWalkableTile(currentPos, &startTx, &startTy, &recoveryPath)) {
            return false;
        }
    }

    // Fallback node: the closest tile reached if the watchdog fires before we hit the goal
    TilePoint closestReached;
    float closestDist = kInfiniteDistance;

    // Seed the open set with the start tile.
    // forbiddenDir is derived from the car's current heading so we don't immediately U-turn.
    uint16_t forbiddenDir = forbiddenDirFromCurrentHeading();
    open.insert(std::pair< TilePoint, uint16_t >(TilePoint(startTx, startTy, destTz, pos_.ox, pos_.oy),
        forbiddenDir));
    int watchDog = kPathfindingWatchdog;

    while (!open.empty()) {
        watchDog--;

        // --- Select the open node closest to the destination (greedy criterion) ---
        float bestDistToGoal = kInfiniteDistance;
        TilePoint p;
        std::map < TilePoint, uint16_t >::iterator bestIt;
        for (std::map < TilePoint, uint16_t >::iterator it = open.begin();
             it != open.end(); it++)
        {
            float distToGoal =
                sqrt((float) (destTx - it->first.tx) * (destTx - it->first.tx) +
                     (float) (destTy - it->first.ty) * (destTy - it->first.ty));
            if (distToGoal < bestDistToGoal) {
                bestDistToGoal = distToGoal;
                p = it->first;
                bestIt = it;        // it cannot be const_iterator because of this assign
                forbiddenDir = it->second;
            }
        }
        if (bestDistToGoal < closestDist) {
            closestReached = p;
            closestDist = bestDistToGoal;
        }
        open.erase(bestIt);
        closed.insert(p);

        // --- Goal test (or watchdog expiry → use best node reached so far) ---
        if ((p.tx == destTx && p.ty == destTy && p.tz == destTz)
            || watchDog < 0)
        {
            if (watchDog < 0) {
                p = closestReached;
                dest_path_.push_front(TilePoint(p.tx, p.ty, p.tz, destOx, destOy));
            } else
                dest_path_.push_front(TilePoint(destTx, destTy, destTz, destOx, destOy));

            // Reconstruct path by following parent links back to the start tile
            while (parent.find(p) != parent.end()) {
                p = parent[p];
                if (p.tx == pos_.tx && p.ty == pos_.ty && p.tz == destTz)
                    break;
                dest_path_.push_front(p);
            }
            break;
        }

        // --- Expand neighbours: only road-adjacent tiles that respect driving direction ---
        // tileDir() encodes which exits a tile has (4 nibbles, one per cardinal direction).
        // forbiddenDir is the direction back to the parent — we skip it to prevent U-turns.
        std::map <TilePoint, uint16_t> candidateNeighbors;
        uint16_t currentTileDir = pMap_->getPossibleDirectionsFromRoadTile(p.tx, p.ty, p.tz);

        // Try going West (tx-1): allowed if current tile has a West exit and we didn't come from West
        if (forbiddenDir != Map::kForbidDirWest && p.tx > 0) {
            if (dirWalkable(&p, p.tx - 1, p.ty, p.tz)
                && ((currentTileDir & Map::kDirMaskWest) == Map::kForbidDirWest || currentTileDir == Map::kTileDirAll))
                candidateNeighbors[TilePoint(p.tx - 1, p.ty, p.tz)] = Map::kForbidDirEast;
        }

        // Try going East (tx+1): allowed if current tile has an East exit and we didn't come from East
        if (forbiddenDir != Map::kForbidDirEast && p.tx < pMap_->maxTx()) {
            if (dirWalkable(&p, p.tx + 1, p.ty, p.tz)
                && ((currentTileDir & Map::kDirMaskEast) == Map::kForbidDirEast || currentTileDir == Map::kTileDirAll))
                candidateNeighbors[TilePoint(p.tx + 1, p.ty, p.tz)] = Map::kForbidDirWest;
        }

        // Try going North (ty-1): allowed if current tile has a North exit and we didn't come from North
        if (forbiddenDir != Map::kForbidDirNorth && p.ty > 0)
            if (dirWalkable(&p, p.tx, p.ty - 1, p.tz)
                && ((currentTileDir & Map::kDirMaskNorth) == Map::kForbidDirNorth || currentTileDir == Map::kTileDirAll))
                candidateNeighbors[TilePoint(p.tx, p.ty - 1, p.tz)] = Map::kForbidDirSouth;

        // Try going South (ty+1): allowed if current tile has a South exit and we didn't come from South
        if (forbiddenDir != Map::kForbidDirSouth && p.ty < pMap_->maxTy())
            if (dirWalkable(&p, p.tx, p.ty + 1, p.tz)
                && ((currentTileDir & Map::kDirMaskSouth) == Map::kForbidDirSouth || currentTileDir == Map::kTileDirAll))
                candidateNeighbors[TilePoint(p.tx, p.ty + 1, p.tz)] = Map::kForbidDirNorth;

        for (std::map <TilePoint, uint16_t>::iterator it = candidateNeighbors.begin();
            it != candidateNeighbors.end(); it++)
            if (dirWalkable(&p, it->first.tx, it->first.ty,
                it->first.tz)
                && open.find(it->first) == open.end()
                && closed.find(it->first) == closed.end())
            {
                parent[it->first] = p;
                open.insert(*it);
            }
    }

    if(!dest_path_.empty()) {
        // Adjust intra-tile offsets (ox, oy) so the car stays centered in the correct lane.
        // Each case matches a tileDir() pattern; kLaneOffsetLow/High position the car
        // on the appropriate side of the road based on driving direction.
        setSpeedToMax();
        int curox = pos_.ox;
        int curoy = pos_.oy;
        for(std::list < TilePoint >::iterator it = dest_path_.begin();
            it != dest_path_.end(); it++)
        {
            // TODO : adjust offsets respecting direction relative to
            // close next tiles
            switch(pMap_->getPossibleDirectionsFromRoadTile(it->tx, it->ty, it->tz)) {
                case 0xFFF0:
                case 0xFF20:
                    it->ox = kLaneOffsetHigh;
                    it->oy = kLaneOffsetLow;
                    curox = kLaneOffsetHigh;
                    curoy = kLaneOffsetLow;
                    break;
                case 0xF4FF:
                    it->ox = kLaneOffsetLow;
                    it->oy = kLaneOffsetHigh;
                    curox = kLaneOffsetLow;
                    curoy = kLaneOffsetHigh;
                    break;
                case 0xFF2F:
                case 0xF42F:
                    it->ox = kLaneOffsetLow;
                    it->oy = kLaneOffsetLow;
                    curox = kLaneOffsetLow;
                    curoy = kLaneOffsetLow;
                    break;
                case 0x6FFF:
                case 0x64FF:
                    it->ox = kLaneOffsetLow;
                    it->oy = kLaneOffsetHigh;
                    curox = kLaneOffsetLow;
                    curoy = kLaneOffsetHigh;
                    break;
                case 0x6FF0:
                    it->ox = kLaneOffsetHigh;
                    it->oy = kLaneOffsetHigh;
                    curox = kLaneOffsetHigh;
                    curoy = kLaneOffsetHigh;
                    break;
                default:
#if 0
#if _DEBUG
                    printf("hmm tileDir %X at %i, %i, %i\n",
                        (unsigned int)tileDir(it->tileX(), it->tileY(),
                        it->tileZ()), it->tileX(), it->tileY(), it->tileZ());
                    printf("tileAt %i\n",
                        (unsigned int)pMap_->tileAt(
                        it->tileX(), it->tileY(), it->tileZ()));
#endif
#endif
                    it->ox = curox;
                    it->oy = curoy;
                    break;
            }
            // Restore real vehicle Z coordinate (road tiles are at pos_.tz - 1)
            it->tz = pos_.tz;
        }
    }
    // Prepend recovery path (computed if the car started off a drivable tile)
    if((!recoveryPath.empty()) && (!dest_path_.empty())) {
        for (std::vector < TilePoint >::reverse_iterator it = recoveryPath.rbegin();
            it != recoveryPath.rend(); it++)
        {
            it->tz = pos_.tz;
            dest_path_.push_front(*it);
        }
    }

    return !dest_path_.empty();
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
bool GenericCar::findPathToNearestWalkableTile(const TilePoint &startPt, int *startTx, int *startTy, std::vector < TilePoint > *recoveryPath) {
    // Scan up to kMaxWalkableSearchRadius tiles in each of the 4 cardinal directions
    // to find the closest drivable tile. Uses squared distance to avoid sqrt.
    int bestDist = (int)kInfiniteDistance, curDist;
    std::vector < TilePoint > candidatePath;
    candidatePath.reserve(kMaxWalkableSearchRadius);
    // we got somewhere we shouldn't, we need to find somewhere that is walkable
    TilePoint probePoint = startPt;
    for (int i = 1; i < kMaxWalkableSearchRadius; i++) {
        if (pos_.tx + i >= pMap_->maxTx())
            break;
        probePoint.tx = pos_.tx + i;
        candidatePath.push_back(probePoint);
        if (pMap_->isTileWalkableByCar(pos_.tx + i, pos_.ty, startPt.tz)) {
            curDist = i * i;
            if(curDist < bestDist) {
                bestDist = curDist;
                recoveryPath->assign(candidatePath.begin(), candidatePath.end());
                *startTx = pos_.tx + i;
                *startTy = pos_.ty;
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
                recoveryPath->assign(candidatePath.begin(), candidatePath.end());
                *startTx = pos_.tx + i;
                *startTy = pos_.ty;
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
                recoveryPath->assign(candidatePath.begin(), candidatePath.end());
                *startTx = pos_.tx;
                *startTy = pos_.ty + i;
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
                recoveryPath->assign(candidatePath.begin(), candidatePath.end());
                *startTx = pos_.tx;
                *startTy = pos_.ty + i;
                break;
            }
        }
    }
    return (bestDist != (int)kInfiniteDistance);
}

/*!
 * Moves a vehicle on the map.
 * \param elapsed Elapsed time sine last frame.
 */
bool GenericCar::doMove(uint32_t elapsed, Mission *m)
{
    bool updated = false;
    int used_time = elapsed;

    while ((!dest_path_.empty()) && used_time != 0) {
        if (hold_on_.wayFree == 1) { // Must wait
            return updated;
        } else if (hold_on_.wayFree == 2){
            // Must stop : clear destination and stop
            clearDestination();
            return updated;
        }

        // Get distance between car and next NodePath
        int adx =
            dest_path_.front().tx * 256 + dest_path_.front().ox;
        int ady =
            dest_path_.front().ty * 256 + dest_path_.front().oy;
        int atx = pos_.tx * 256 + pos_.ox;
        int aty = pos_.ty * 256 + pos_.oy;
        int diffx = adx - atx, diffy = ady - aty;

        if (abs(diffx) < 16 && abs(diffy) < 16) {
            // We reached the next point : remove it from path
            pos_.oy = dest_path_.front().oy;
            pos_.ox = dest_path_.front().ox;
            pos_.ty = dest_path_.front().ty;
            pos_.tx = dest_path_.front().tx;
            dest_path_.pop_front();
            // There's no following point so stop moving
            if (dest_path_.size() == 0)
                stop();
            updated = true;
        } else {
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
                // dx = diffx * (speed_ * used_time / 1000) / d;
                dx = (int)((diffx * (speed() * avail_time_use) / d) / 1000);
            if (abs(diffy) > 0)
                // dy = diffy * (speed_ * used_time / 1000) / d;
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
                } else
                    used_time = 0;
                if (used_time < 0 || prv_time == used_time)
                    used_time = 0;
            } else
                used_time = 0;

            // Moves vehicle
            addOffsetToPosition(dx, dy);
#if 0
            if (addOffsetToPosition(dx, dy)) {
                ;
            } else {
                // TODO: avoid obstacles.
                speed_ = 0;
            }
#endif
            if(dest_path_.front().tx == pos_.tx
                && dest_path_.front().ty == pos_.ty
                && dest_path_.front().ox == pos_.ox
                && dest_path_.front().oy == pos_.oy)
                dest_path_.pop_front();
            if (dest_path_.size() == 0)
                stop();

            updated = true;
        }
    }

    if (dest_path_.empty() && isMoving()) {
        printf("Destination Unknown, full speed driving = %i ... doing full stop\n",
               speed());
        stop();
    }
    if (!passengers_.empty()) {
        for (std::list<PedInstance *>::iterator it = passengers_.begin();
            it != passengers_.end(); it++
        ) {
            (*it)->setPosition(pos_);
        }
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