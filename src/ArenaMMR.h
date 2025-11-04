/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ARENA_MMR_H
#define ARENA_MMR_H

#include "Glicko2.h"
#include "ArenaRatingStorage.h"
#include "ObjectGuid.h"
#include <vector>

class Player;
class Battleground;

/// @brief Manager for arena Glicko-2 rating calculations
class ArenaMMRMgr
{
public:
    static ArenaMMRMgr* instance();

    /// Initialize rating for a new player in a bracket
    void InitializePlayerRating(ObjectGuid playerGuid, ArenaBracket bracket);

    /// Update player rating after an arena match
    void UpdatePlayerRating(ObjectGuid playerGuid, ArenaBracket bracket, bool won,
                           std::vector<ObjectGuid> const& opponents);

    /// Update all players in an arena match (called once per match)
    void UpdateArenaMatch(Battleground* bg, std::vector<ObjectGuid> const& winnerGuids,
                         std::vector<ObjectGuid> const& loserGuids, ArenaBracket bracket);

    /// Get player's current rating for a bracket
    float GetPlayerRating(ObjectGuid playerGuid, ArenaBracket bracket) const;

    /// Get player's rating deviation for a bracket
    float GetPlayerRatingDeviation(ObjectGuid playerGuid, ArenaBracket bracket) const;

    /// Calculate average rating for a list of players
    float CalculateAverageRating(std::vector<ObjectGuid> const& playerGuids, ArenaBracket bracket) const;

    /// Calculate relaxed MMR range based on queue time
    float GetRelaxedMMRRange(uint32 queueTimeSeconds, ArenaBracket bracket) const;

    /// Configuration accessors
    bool IsEnabled() const { return _enabled; }
    bool IsSkirmishSeparateRating() const { return _skirmishSeparateRating; }
    float GetInitialRating() const { return _initialRating; }
    float GetInitialRatingDeviation() const { return _initialRD; }
    float GetInitialVolatility() const { return _initialVolatility; }
    float GetSystemTau() const { return _systemTau; }

    /// Per-bracket matchmaking settings
    float GetInitialRange(ArenaBracket bracket) const;
    float GetMaxRange(ArenaBracket bracket) const;
    float GetRelaxationRate(ArenaBracket bracket) const;

    /// Load configuration from worldserver.conf
    void LoadConfig();

private:
    ArenaMMRMgr() = default;
    ~ArenaMMRMgr() = default;

    ArenaMMRMgr(ArenaMMRMgr const&) = delete;
    ArenaMMRMgr& operator=(ArenaMMRMgr const&) = delete;

    /// Global arena settings
    bool _enabled = true;
    float _initialRating = 1500.0f;
    float _initialRD = 350.0f;
    float _initialVolatility = 0.06f;
    float _systemTau = 0.5f;
    bool _skirmishSeparateRating = true;

    /// Per-bracket matchmaking ranges
    struct BracketSettings
    {
        float initialRange = 200.0f;
        float maxRange = 1000.0f;
        float relaxationRate = 10.0f;
    };

    BracketSettings _bracketSettings[static_cast<uint8>(ArenaBracket::MAX_SLOTS)];

    /// Glicko-2 calculation system
    Glicko2System _glicko;
};

#define sArenaMMRMgr ArenaMMRMgr::instance()

#endif // ARENA_MMR_H
