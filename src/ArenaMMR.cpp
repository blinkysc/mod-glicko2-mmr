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

#include "ArenaMMR.h"
#include "Config.h"
#include "Log.h"
#include "Player.h"
#include "Battleground.h"
#include <algorithm>

ArenaMMRMgr* ArenaMMRMgr::instance()
{
    static ArenaMMRMgr instance;
    return &instance;
}

void ArenaMMRMgr::InitializePlayerRating(ObjectGuid playerGuid, ArenaBracket bracket)
{
    if (!_enabled)
        return;

    // Check if player already has rating for this bracket
    if (sArenaRatingStorage->HasRating(playerGuid, bracket))
        return;

    // Create new rating with default values
    ArenaRatingData data;
    data.rating = _initialRating;
    data.ratingDeviation = _initialRD;
    data.volatility = _initialVolatility;
    data.matchesPlayed = 0;
    data.wins = 0;
    data.losses = 0;
    data.bracket = bracket;
    data.loaded = true;

    sArenaRatingStorage->SetRating(playerGuid, bracket, data);
}

void ArenaMMRMgr::UpdatePlayerRating(ObjectGuid playerGuid, ArenaBracket bracket, bool won,
                                     std::vector<ObjectGuid> const& opponents)
{
    if (!_enabled || opponents.empty())
        return;

    // Get player's current rating
    ArenaRatingData playerData = sArenaRatingStorage->GetRating(playerGuid, bracket);

    // Calculate average opponent rating
    float opponentAvgRating = CalculateAverageRating(opponents, bracket);
    float opponentAvgRD = 0.0f;

    // Calculate average opponent RD
    for (ObjectGuid opponentGuid : opponents)
    {
        ArenaRatingData opponentData = sArenaRatingStorage->GetRating(opponentGuid, bracket);
        opponentAvgRD += opponentData.ratingDeviation;
    }
    opponentAvgRD /= opponents.size();

    // Update rating using Glicko-2
    Glicko2Rating playerRating(playerData.rating, playerData.ratingDeviation, playerData.volatility);

    std::vector<Glicko2Opponent> opponents_;
    opponents_.emplace_back(opponentAvgRating, opponentAvgRD, won ? 1.0f : 0.0f);

    Glicko2Rating newRating = _glicko.UpdateRating(playerRating, opponents_);

    // Update player data
    playerData.rating = newRating.rating;
    playerData.ratingDeviation = newRating.ratingDeviation;
    playerData.volatility = newRating.volatility;
    playerData.matchesPlayed++;
    if (won)
        playerData.wins++;
    else
        playerData.losses++;

    // Save updated rating
    sArenaRatingStorage->SetRating(playerGuid, bracket, playerData);
}

void ArenaMMRMgr::UpdateArenaMatch(Battleground* /*bg*/, std::vector<ObjectGuid> const& winnerGuids,
                                   std::vector<ObjectGuid> const& loserGuids, ArenaBracket bracket)
{
    if (!_enabled || winnerGuids.empty() || loserGuids.empty())
        return;

    // Update all winners
    for (ObjectGuid winnerGuid : winnerGuids)
    {
        UpdatePlayerRating(winnerGuid, bracket, true, loserGuids);
    }

    // Update all losers
    for (ObjectGuid loserGuid : loserGuids)
    {
        UpdatePlayerRating(loserGuid, bracket, false, winnerGuids);
    }

    LOG_DEBUG("module", "ArenaMMRMgr: Updated ratings for arena match (bracket {})", GetBracketName(bracket));
}

float ArenaMMRMgr::GetPlayerRating(ObjectGuid playerGuid, ArenaBracket bracket) const
{
    ArenaRatingData data = sArenaRatingStorage->GetRating(playerGuid, bracket);
    return data.rating;
}

float ArenaMMRMgr::GetPlayerRatingDeviation(ObjectGuid playerGuid, ArenaBracket bracket) const
{
    ArenaRatingData data = sArenaRatingStorage->GetRating(playerGuid, bracket);
    return data.ratingDeviation;
}

float ArenaMMRMgr::CalculateAverageRating(std::vector<ObjectGuid> const& playerGuids, ArenaBracket bracket) const
{
    if (playerGuids.empty())
        return _initialRating;

    float totalRating = 0.0f;
    for (ObjectGuid guid : playerGuids)
    {
        ArenaRatingData data = sArenaRatingStorage->GetRating(guid, bracket);
        totalRating += data.rating;
    }

    return totalRating / playerGuids.size();
}

float ArenaMMRMgr::GetRelaxedMMRRange(uint32 queueTimeSeconds, ArenaBracket bracket) const
{
    const BracketSettings& settings = _bracketSettings[static_cast<uint8>(bracket)];

    // Linear relaxation: initialRange + (relaxationRate * time)
    float currentRange = settings.initialRange + (settings.relaxationRate * queueTimeSeconds);

    // Cap at maximum range
    return std::min(currentRange, settings.maxRange);
}

float ArenaMMRMgr::GetInitialRange(ArenaBracket bracket) const
{
    return _bracketSettings[static_cast<uint8>(bracket)].initialRange;
}

float ArenaMMRMgr::GetMaxRange(ArenaBracket bracket) const
{
    return _bracketSettings[static_cast<uint8>(bracket)].maxRange;
}

float ArenaMMRMgr::GetRelaxationRate(ArenaBracket bracket) const
{
    return _bracketSettings[static_cast<uint8>(bracket)].relaxationRate;
}

void ArenaMMRMgr::LoadConfig()
{
    // Global arena settings
    _enabled = sConfigMgr->GetOption<bool>("Glicko2.Arena.Enabled", false);
    _initialRating = sConfigMgr->GetOption<float>("Glicko2.Arena.InitialRating", 1500.0f);
    _initialRD = sConfigMgr->GetOption<float>("Glicko2.Arena.InitialRatingDeviation", 350.0f);
    _initialVolatility = sConfigMgr->GetOption<float>("Glicko2.Arena.InitialVolatility", 0.06f);
    _systemTau = sConfigMgr->GetOption<float>("Glicko2.Arena.Tau", 0.5f);

    // 2v2 settings
    _bracketSettings[static_cast<uint8>(ArenaBracket::SLOT_2v2)].initialRange =
        sConfigMgr->GetOption<float>("Glicko2.Arena.2v2.Matchmaking.InitialRange", 150.0f);
    _bracketSettings[static_cast<uint8>(ArenaBracket::SLOT_2v2)].maxRange =
        sConfigMgr->GetOption<float>("Glicko2.Arena.2v2.Matchmaking.MaxRange", 800.0f);
    _bracketSettings[static_cast<uint8>(ArenaBracket::SLOT_2v2)].relaxationRate =
        sConfigMgr->GetOption<float>("Glicko2.Arena.2v2.Matchmaking.RelaxationRate", 15.0f);

    // 3v3 settings
    _bracketSettings[static_cast<uint8>(ArenaBracket::SLOT_3v3)].initialRange =
        sConfigMgr->GetOption<float>("Glicko2.Arena.3v3.Matchmaking.InitialRange", 200.0f);
    _bracketSettings[static_cast<uint8>(ArenaBracket::SLOT_3v3)].maxRange =
        sConfigMgr->GetOption<float>("Glicko2.Arena.3v3.Matchmaking.MaxRange", 1000.0f);
    _bracketSettings[static_cast<uint8>(ArenaBracket::SLOT_3v3)].relaxationRate =
        sConfigMgr->GetOption<float>("Glicko2.Arena.3v3.Matchmaking.RelaxationRate", 12.0f);

    // 5v5 settings
    _bracketSettings[static_cast<uint8>(ArenaBracket::SLOT_5v5)].initialRange =
        sConfigMgr->GetOption<float>("Glicko2.Arena.5v5.Matchmaking.InitialRange", 250.0f);
    _bracketSettings[static_cast<uint8>(ArenaBracket::SLOT_5v5)].maxRange =
        sConfigMgr->GetOption<float>("Glicko2.Arena.5v5.Matchmaking.MaxRange", 1200.0f);
    _bracketSettings[static_cast<uint8>(ArenaBracket::SLOT_5v5)].relaxationRate =
        sConfigMgr->GetOption<float>("Glicko2.Arena.5v5.Matchmaking.RelaxationRate", 10.0f);

    LOG_INFO("module", "ArenaMMRMgr: Loaded configuration (Enabled: {}, Initial Rating: {})",
        _enabled, _initialRating);
}
