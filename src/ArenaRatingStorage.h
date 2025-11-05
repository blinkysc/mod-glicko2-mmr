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

#ifndef ARENA_RATING_STORAGE_H
#define ARENA_RATING_STORAGE_H

#include "ObjectGuid.h"
#include "DatabaseEnvFwd.h"
#include <unordered_map>
#include <shared_mutex>

class Player;

/// @brief Arena bracket types (maps to character_arena_stats.slot)
enum class ArenaBracket : uint8
{
    SLOT_2v2 = 33,       ///< 2v2 rated arena (slot 0)
    SLOT_3v3 = 1,       ///< 3v3 rated arena (slot 1)
    SLOT_5v5 = 2,       ///< 5v5 rated arena (slot 2)

    MAX_SLOTS = 3
};

/// @brief Arena rating data for a specific bracket
struct ArenaRatingData
{
    float rating = 1500.0f;             ///< Player skill rating
    float ratingDeviation = 350.0f;     ///< Rating uncertainty (RD)
    float volatility = 0.06f;           ///< Performance consistency
    uint32 matchesPlayed = 0;           ///< Total matches played in this bracket
    uint32 wins = 0;                    ///< Total wins in this bracket
    uint32 losses = 0;                  ///< Total losses in this bracket
    ArenaBracket bracket;               ///< Which bracket this rating is for
    bool loaded = false;                ///< Whether data is loaded from DB

    ArenaRatingData() : bracket(ArenaBracket::SLOT_2v2) { }

    ArenaRatingData(float r, float rd, float v, uint32 mp, uint32 w, uint32 l, ArenaBracket b)
        : rating(r), ratingDeviation(rd), volatility(v),
          matchesPlayed(mp), wins(w), losses(l), bracket(b), loaded(true) { }
};

/// @brief Thread-safe storage for player arena ratings per bracket
class ArenaRatingStorage
{
public:
    static ArenaRatingStorage* instance();

    /// Get rating for specific bracket
    ArenaRatingData GetRating(ObjectGuid playerGuid, ArenaBracket bracket);

    /// Set rating for specific bracket
    void SetRating(ObjectGuid playerGuid, ArenaBracket bracket, ArenaRatingData const& data);

    /// Check if player has rating for bracket
    bool HasRating(ObjectGuid playerGuid, ArenaBracket bracket);

    /// Remove rating for specific bracket
    void RemoveRating(ObjectGuid playerGuid, ArenaBracket bracket);

    /// Remove all ratings for a player
    void RemoveAllRatings(ObjectGuid playerGuid);

    /// Load rating for specific bracket from database
    void LoadRating(ObjectGuid playerGuid, ArenaBracket bracket);

    /// Load all brackets for a player from database
    void LoadAllRatings(ObjectGuid playerGuid);

    /// Save rating for specific bracket to database
    void SaveRating(ObjectGuid playerGuid, ArenaBracket bracket);

    /// Save specific rating data to database
    void SaveRating(ObjectGuid playerGuid, ArenaBracket bracket, ArenaRatingData const& data);

    /// Save all brackets for a player to database
    void SaveAllRatings(ObjectGuid playerGuid);

    /// Save all cached ratings to database
    void SaveAll();

    /// Clear in-memory cache
    void ClearCache();

    /// Get number of cached entries
    size_t GetCacheSize() const;

private:
    ArenaRatingStorage() = default;
    ~ArenaRatingStorage() = default;

    ArenaRatingStorage(ArenaRatingStorage const&) = delete;
    ArenaRatingStorage& operator=(ArenaRatingStorage const&) = delete;

    /// Composite key for rating lookups
    struct RatingKey
    {
        ObjectGuid guid;
        ArenaBracket bracket;

        bool operator==(RatingKey const& other) const
        {
            return guid == other.guid && bracket == other.bracket;
        }
    };

    /// Hash function for RatingKey
    struct RatingKeyHash
    {
        size_t operator()(RatingKey const& key) const
        {
            return std::hash<uint64>()(key.guid.GetRawValue()) ^
                   (std::hash<uint8>()(static_cast<uint8>(key.bracket)) << 1);
        }
    };

    std::unordered_map<RatingKey, ArenaRatingData, RatingKeyHash> _ratings;
    mutable std::shared_mutex _mutex;
};

#define sArenaRatingStorage ArenaRatingStorage::instance()

/// @brief Helper function to get arena slot from arena type
inline ArenaBracket GetArenaSlot(uint8 arenaType, bool /*isRated*/)
{
    // Map arena type to slot (matches character_arena_stats.slot)
    switch (arenaType)
    {
        case 2: return ArenaBracket::SLOT_2v2;  // ARENA_TYPE_2v2 -> slot 0
        case 3: return ArenaBracket::SLOT_3v3;  // ARENA_TYPE_3v3 -> slot 1
        case 5: return ArenaBracket::SLOT_5v5;  // ARENA_TYPE_5v5 -> slot 2
        default: return ArenaBracket::SLOT_2v2;
    }
}

/// @brief Get bracket name for display
inline const char* GetBracketName(ArenaBracket bracket)
{
    switch (bracket)
    {
        case ArenaBracket::SLOT_2v2: return "2v2";
        case ArenaBracket::SLOT_3v3: return "3v3";
        case ArenaBracket::SLOT_5v5: return "5v5";
        default: return "Unknown";
    }
}

#endif // ARENA_RATING_STORAGE_H
