/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BATTLEGROUND_MMR_H
#define _BATTLEGROUND_MMR_H

#include "Glicko2.h"
#include "Player.h"

/// @brief Singleton manager for battleground MMR calculations
class BattlegroundMMRMgr
{
private:
    BattlegroundMMRMgr() = default;
    ~BattlegroundMMRMgr() = default;

public:
    static BattlegroundMMRMgr* instance();

    void UpdatePlayerRating(Player* player, bool won, const std::vector<Player*>& opponents);
    float CalculateGearScore(Player* player);
    float GetPlayerMMR(Player* player) const;
    float GetPlayerGearScore(Player* player);
    float GetPlayerCombinedScore(Player* player);
    void InitializePlayerRating(Player* player);

    bool IsEnabled() const { return _enabled; }
    float GetMMRWeight() const { return _mmrWeight; }
    float GetGearWeight() const { return _gearWeight; }
    float GetStartingRating() const { return _startingRating; }
    float GetStartingRD() const { return _startingRD; }
    float GetStartingVolatility() const { return _startingVolatility; }
    float GetSystemTau() const { return _systemTau; }

    bool IsQueueRelaxationEnabled() const { return _queueRelaxationEnabled; }
    float GetRelaxedMMRTolerance(uint32 queueTimeSeconds) const;
    float GetInitialMaxMMRDifference() const { return _initialMaxMMRDifference; }

    void LoadConfig();

private:
    bool _enabled;
    float _startingRating;
    float _startingRD;
    float _startingVolatility;
    float _systemTau;
    float _mmrWeight;
    float _gearWeight;

    bool _queueRelaxationEnabled;
    float _initialMaxMMRDifference;
    uint32 _relaxationIntervalSeconds;
    float _relaxationStepMMR;
    uint32 _maxRelaxationSeconds;

    Glicko2System _glicko;
};

#define sBattlegroundMMRMgr BattlegroundMMRMgr::instance()

#endif // _BATTLEGROUND_MMR_H
