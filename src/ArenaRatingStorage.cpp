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

#include "ArenaRatingStorage.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "Config.h"

ArenaRatingStorage* ArenaRatingStorage::instance()
{
    static ArenaRatingStorage instance;
    return &instance;
}

ArenaRatingData ArenaRatingStorage::GetRating(ObjectGuid playerGuid, ArenaBracket bracket)
{
    std::shared_lock lock(_mutex);

    RatingKey key{playerGuid, bracket};
    auto itr = _ratings.find(key);
    if (itr != _ratings.end())
    {
        return itr->second;
    }

    // Return default rating if not found
    ArenaRatingData data;
    data.bracket = bracket;
    data.rating = sConfigMgr->GetOption<float>("Glicko2.Arena.InitialRating", 1500.0f);
    data.ratingDeviation = sConfigMgr->GetOption<float>("Glicko2.Arena.InitialRatingDeviation", 350.0f);
    data.volatility = sConfigMgr->GetOption<float>("Glicko2.Arena.InitialVolatility", 0.06f);
    return data;
}

void ArenaRatingStorage::SetRating(ObjectGuid playerGuid, ArenaBracket bracket, ArenaRatingData const& data)
{
    std::unique_lock lock(_mutex);

    RatingKey key{playerGuid, bracket};
    _ratings[key] = data;
}

bool ArenaRatingStorage::HasRating(ObjectGuid playerGuid, ArenaBracket bracket)
{
    std::shared_lock lock(_mutex);

    RatingKey key{playerGuid, bracket};
    return _ratings.find(key) != _ratings.end();
}

void ArenaRatingStorage::RemoveRating(ObjectGuid playerGuid, ArenaBracket bracket)
{
    std::unique_lock lock(_mutex);

    RatingKey key{playerGuid, bracket};
    _ratings.erase(key);
}

void ArenaRatingStorage::RemoveAllRatings(ObjectGuid playerGuid)
{
    std::unique_lock lock(_mutex);

    for (uint8 i = 0; i < static_cast<uint8>(ArenaBracket::MAX_SLOTS); ++i)
    {
        RatingKey key{playerGuid, static_cast<ArenaBracket>(i)};
        _ratings.erase(key);
    }
}

void ArenaRatingStorage::LoadRating(ObjectGuid playerGuid, ArenaBracket bracket)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT rating, rating_deviation, volatility, matches_played, matches_won, matches_lost "
        "FROM character_arena_stats WHERE guid = {} AND slot = {}",
        playerGuid.GetCounter(), static_cast<uint8>(bracket));

    if (!result)
    {
        return; // No rating found, will use defaults
    }

    Field* fields = result->Fetch();

    ArenaRatingData data;
    data.rating = fields[0].Get<float>();
    data.ratingDeviation = fields[1].Get<float>();
    data.volatility = fields[2].Get<float>();
    data.matchesPlayed = fields[3].Get<uint32>();
    data.wins = fields[4].Get<uint32>();
    data.losses = fields[5].Get<uint32>();
    data.bracket = bracket;
    data.loaded = true;

    SetRating(playerGuid, bracket, data);
}

void ArenaRatingStorage::LoadAllRatings(ObjectGuid playerGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT slot, rating, rating_deviation, volatility, matches_played, matches_won, matches_lost "
        "FROM character_arena_stats WHERE guid = {}",
        playerGuid.GetCounter());

    if (!result)
    {
        return; // No ratings found
    }

    do
    {
        Field* fields = result->Fetch();

        uint8 slotId = fields[0].Get<uint8>();
        if (slotId >= static_cast<uint8>(ArenaBracket::MAX_SLOTS))
        {
            LOG_ERROR("module", "ArenaRatingStorage::LoadAllRatings: Invalid slot {} for player {}",
                slotId, playerGuid.ToString());
            continue;
        }

        ArenaBracket bracket = static_cast<ArenaBracket>(slotId);

        ArenaRatingData data;
        data.rating = fields[1].Get<float>();
        data.ratingDeviation = fields[2].Get<float>();
        data.volatility = fields[3].Get<float>();
        data.matchesPlayed = fields[4].Get<uint32>();
        data.wins = fields[5].Get<uint32>();
        data.losses = fields[6].Get<uint32>();
        data.bracket = bracket;
        data.loaded = true;

        SetRating(playerGuid, bracket, data);

    } while (result->NextRow());
}

void ArenaRatingStorage::SaveRating(ObjectGuid playerGuid, ArenaBracket bracket)
{
    ArenaRatingData data = GetRating(playerGuid, bracket);
    SaveRating(playerGuid, bracket, data);
}

void ArenaRatingStorage::SaveRating(ObjectGuid playerGuid, ArenaBracket bracket, ArenaRatingData const& data)
{
    // Insert or update arena stats with Glicko-2 data
    CharacterDatabase.Execute(
        "INSERT INTO character_arena_stats "
        "(guid, slot, matchMakerRating, maxMMR, rating, rating_deviation, volatility, matches_played, matches_won, matches_lost, last_match_time) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, UNIX_TIMESTAMP()) "
        "ON DUPLICATE KEY UPDATE "
        "rating = VALUES(rating), "
        "rating_deviation = VALUES(rating_deviation), "
        "volatility = VALUES(volatility), "
        "matches_played = VALUES(matches_played), "
        "matches_won = VALUES(matches_won), "
        "matches_lost = VALUES(matches_lost), "
        "last_match_time = VALUES(last_match_time)",
        playerGuid.GetCounter(),
        static_cast<uint8>(bracket),
        static_cast<uint16>(data.rating),  // matchMakerRating (for compatibility)
        static_cast<uint16>(data.rating),  // maxMMR (track highest rating)
        data.rating,
        data.ratingDeviation,
        data.volatility,
        data.matchesPlayed,
        data.wins,
        data.losses);
}

void ArenaRatingStorage::SaveAllRatings(ObjectGuid playerGuid)
{
    std::shared_lock lock(_mutex);

    for (uint8 i = 0; i < static_cast<uint8>(ArenaBracket::MAX_SLOTS); ++i)
    {
        ArenaBracket bracket = static_cast<ArenaBracket>(i);
        RatingKey key{playerGuid, bracket};

        auto itr = _ratings.find(key);
        if (itr != _ratings.end() && itr->second.loaded)
        {
            // Unlock during database operation
            lock.unlock();
            SaveRating(playerGuid, bracket, itr->second);
            lock.lock();
        }
    }
}

void ArenaRatingStorage::SaveAll()
{
    std::shared_lock lock(_mutex);

    LOG_INFO("module", "ArenaRatingStorage: Saving all arena ratings to database...");

    // Collect all unique player GUIDs
    std::unordered_set<ObjectGuid> playerGuids;
    for (auto const& [key, data] : _ratings)
    {
        if (data.loaded)
        {
            playerGuids.insert(key.guid);
        }
    }

    lock.unlock();

    // Save all ratings for each player
    for (ObjectGuid guid : playerGuids)
    {
        SaveAllRatings(guid);
    }

    LOG_INFO("module", "ArenaRatingStorage: Saved arena ratings for {} players", playerGuids.size());
}

void ArenaRatingStorage::ClearCache()
{
    std::unique_lock lock(_mutex);
    _ratings.clear();
    LOG_INFO("module", "ArenaRatingStorage: Cache cleared");
}

size_t ArenaRatingStorage::GetCacheSize() const
{
    std::shared_lock lock(_mutex);
    return _ratings.size();
}
