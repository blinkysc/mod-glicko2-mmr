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

#ifndef GLICKO2_PLAYER_STORAGE_H
#define GLICKO2_PLAYER_STORAGE_H

#include "ObjectGuid.h"
#include "DatabaseEnvFwd.h"
#include <unordered_map>
#include <shared_mutex>

class Player;

/// @brief Glicko-2 rating data for a player
struct BattlegroundRatingData
{
    float rating = 1500.0f;             ///< Player skill rating
    float ratingDeviation = 350.0f;     ///< Rating uncertainty (RD)
    float volatility = 0.06f;           ///< Performance consistency
    uint32 matchesPlayed = 0;           ///< Total matches played
    uint32 wins = 0;                    ///< Total wins
    uint32 losses = 0;                  ///< Total losses
    bool loaded = false;                ///< Whether data is loaded from DB

    BattlegroundRatingData() = default;

    BattlegroundRatingData(float r, float rd, float v, uint32 mp, uint32 w, uint32 l)
        : rating(r), ratingDeviation(rd), volatility(v),
          matchesPlayed(mp), wins(w), losses(l), loaded(true) { }
};

/// @brief Thread-safe external storage for player battleground ratings
class Glicko2PlayerStorage
{
public:
    static Glicko2PlayerStorage* instance();

    BattlegroundRatingData GetRating(ObjectGuid playerGuid);
    void SetRating(ObjectGuid playerGuid, BattlegroundRatingData const& data);
    bool HasRating(ObjectGuid playerGuid);
    void RemoveRating(ObjectGuid playerGuid);

    void LoadRating(ObjectGuid playerGuid);
    void SaveRating(ObjectGuid playerGuid);
    void SaveRating(ObjectGuid playerGuid, BattlegroundRatingData const& data);

    void SaveAll();
    void ClearCache();
    size_t GetCacheSize() const;

private:
    Glicko2PlayerStorage() = default;
    ~Glicko2PlayerStorage() = default;

    Glicko2PlayerStorage(Glicko2PlayerStorage const&) = delete;
    Glicko2PlayerStorage& operator=(Glicko2PlayerStorage const&) = delete;

    std::unordered_map<ObjectGuid, BattlegroundRatingData> _ratings;
    mutable std::shared_mutex _mutex;
};

#define sGlicko2Storage Glicko2PlayerStorage::instance()

#endif // GLICKO2_PLAYER_STORAGE_H
