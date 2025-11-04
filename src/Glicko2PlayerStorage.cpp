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

#include "Glicko2PlayerStorage.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Config.h"

Glicko2PlayerStorage* Glicko2PlayerStorage::instance()
{
    static Glicko2PlayerStorage instance;
    return &instance;
}

BattlegroundRatingData Glicko2PlayerStorage::GetRating(ObjectGuid playerGuid)
{
    std::shared_lock lock(_mutex);

    auto itr = _ratings.find(playerGuid);
    if (itr != _ratings.end())
        return itr->second;

    BattlegroundRatingData defaultData;
    defaultData.rating = sConfigMgr->GetOption<float>("Glicko2.InitialRating", 1500.0f);
    defaultData.ratingDeviation = sConfigMgr->GetOption<float>("Glicko2.InitialRatingDeviation", 350.0f);
    defaultData.volatility = sConfigMgr->GetOption<float>("Glicko2.InitialVolatility", 0.06f);
    return defaultData;
}

void Glicko2PlayerStorage::SetRating(ObjectGuid playerGuid, BattlegroundRatingData const& data)
{
    std::unique_lock lock(_mutex);
    _ratings[playerGuid] = data;
}

bool Glicko2PlayerStorage::HasRating(ObjectGuid playerGuid)
{
    std::shared_lock lock(_mutex);
    return _ratings.find(playerGuid) != _ratings.end();
}

void Glicko2PlayerStorage::RemoveRating(ObjectGuid playerGuid)
{
    std::unique_lock lock(_mutex);
    _ratings.erase(playerGuid);
}

void Glicko2PlayerStorage::LoadRating(ObjectGuid playerGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT rating, rating_deviation, volatility, matches_played, matches_won, matches_lost, last_match_time "
        "FROM character_battleground_rating WHERE guid = {}", playerGuid.GetCounter());

    if (!result)
    {
        BattlegroundRatingData data;
        data.rating = sConfigMgr->GetOption<float>("Glicko2.InitialRating", 1500.0f);
        data.ratingDeviation = sConfigMgr->GetOption<float>("Glicko2.InitialRatingDeviation", 350.0f);
        data.volatility = sConfigMgr->GetOption<float>("Glicko2.InitialVolatility", 0.06f);
        data.loaded = true;

        std::unique_lock lock(_mutex);
        _ratings[playerGuid] = data;
        return;
    }

    Field* fields = result->Fetch();
    BattlegroundRatingData data;
    data.rating = fields[0].Get<float>();
    data.ratingDeviation = fields[1].Get<float>();
    data.volatility = fields[2].Get<float>();
    data.matchesPlayed = fields[3].Get<uint32>();
    data.wins = fields[4].Get<uint32>();
    data.losses = fields[5].Get<uint32>();
    data.loaded = true;

    std::unique_lock lock(_mutex);
    _ratings[playerGuid] = data;

    LOG_DEBUG("module.glicko2", "Loaded BG rating for player GUID {}: rating={:.1f}, RD={:.1f}, vol={:.4f}",
        playerGuid.ToString(), data.rating, data.ratingDeviation, data.volatility);
}

void Glicko2PlayerStorage::SaveRating(ObjectGuid playerGuid)
{
    BattlegroundRatingData data;
    {
        std::shared_lock lock(_mutex);
        auto itr = _ratings.find(playerGuid);
        if (itr == _ratings.end() || !itr->second.loaded)
            return;

        data = itr->second;
    }

    SaveRating(playerGuid, data);
}

void Glicko2PlayerStorage::SaveRating(ObjectGuid playerGuid, BattlegroundRatingData const& data)
{
    if (!data.loaded)
        return;

    CharacterDatabase.Execute(
        "REPLACE INTO character_battleground_rating "
        "(guid, rating, rating_deviation, volatility, matches_played, matches_won, matches_lost, last_match_time) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {})",
        playerGuid.GetCounter(), data.rating, data.ratingDeviation, data.volatility,
        data.matchesPlayed, data.wins, data.losses, static_cast<uint32>(time(nullptr)));

    LOG_DEBUG("module.glicko2", "Saved BG rating for player GUID {}: rating={:.1f}, RD={:.1f}, matches={}",
        playerGuid.ToString(), data.rating, data.ratingDeviation, data.matchesPlayed);
}

void Glicko2PlayerStorage::SaveAll()
{
    std::shared_lock lock(_mutex);
    LOG_INFO("module.glicko2", "Saving all BG ratings ({} entries)...", _ratings.size());

    for (auto const& [guid, data] : _ratings)
    {
        if (data.loaded)
        {
            SaveRating(guid, data);
        }
    }

    LOG_INFO("module.glicko2", "All BG ratings saved successfully.");
}

void Glicko2PlayerStorage::ClearCache()
{
    std::unique_lock lock(_mutex);
    size_t count = _ratings.size();
    _ratings.clear();
    LOG_INFO("module.glicko2", "Cleared BG rating cache ({} entries removed).", count);
}

size_t Glicko2PlayerStorage::GetCacheSize() const
{
    std::shared_lock lock(_mutex);
    return _ratings.size();
}
