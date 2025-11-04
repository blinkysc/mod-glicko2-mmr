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

#include "ScriptMgr.h"
#include "Player.h"
#include "Battleground.h"
#include "BattlegroundQueue.h"
#include "Config.h"
#include "Glicko2PlayerStorage.h"
#include "BattlegroundMMR.h"
#include "ArenaMMR.h"
#include "ArenaRatingStorage.h"
#include "Log.h"
#include "GameTime.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cmath>

struct MatchTracker
{
    std::unordered_set<ObjectGuid> alliancePlayers;
    std::unordered_set<ObjectGuid> hordePlayers;
    uint32 winnerTeam = 0;
    bool processed = false;
    time_t startTime = 0;
};

/// @brief Tracks players in a queue's selection pool for MMR matching
struct PoolTracker
{
    std::unordered_set<ObjectGuid> players;
    time_t lastUpdateTime = 0;

    void AddGroup(GroupQueueInfo* group)
    {
        for (ObjectGuid guid : group->Players)
            players.insert(guid);
        lastUpdateTime = time(nullptr);
    }

    void Clear()
    {
        players.clear();
        lastUpdateTime = 0;
    }

    bool IsStale(time_t now, uint32 maxAgeSeconds = 300) const
    {
        return (now - lastUpdateTime) > maxAgeSeconds;
    }
};

/// @brief Key for tracking selection pools per queue/bracket
struct PoolKey
{
    BattlegroundQueue* queue;
    BattlegroundBracketId bracketId;

    bool operator==(PoolKey const& other) const
    {
        return queue == other.queue && bracketId == other.bracketId;
    }
};

namespace std
{
    template<>
    struct hash<PoolKey>
    {
        size_t operator()(PoolKey const& key) const
        {
            return hash<void*>()(key.queue) ^ hash<uint8>()(key.bracketId);
        }
    };
}

/// @brief Handles battleground rating updates using Glicko-2 algorithm
class Glicko2BGScript : public AllBattlegroundScript
{
    std::unordered_map<uint32, MatchTracker> _activeMatches;
    mutable std::mutex _matchMutex;

    std::unordered_map<PoolKey, PoolTracker> _poolTracking;
    mutable std::mutex _poolMutex;

public:
    Glicko2BGScript() : AllBattlegroundScript("Glicko2BGScript")
    {
        LOG_INFO("module.glicko2", "[Glicko2] Glicko2BGScript initialized");
    }

    void OnBattlegroundAddPlayer(Battleground* bg, Player* player) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", false))
            return;

        std::lock_guard lock(_matchMutex);

        uint32 instanceId = bg->GetInstanceID();
        auto& match = _activeMatches[instanceId];

        if (player->GetBgTeamId() == TEAM_ALLIANCE)
            match.alliancePlayers.insert(player->GetGUID());
        else
            match.hordePlayers.insert(player->GetGUID());

        if (match.startTime == 0)
            match.startTime = time(nullptr);

        LOG_INFO("module.glicko2", "[Glicko2] Player {} added to BG instance {} (team: {})",
            player->GetName(), instanceId, player->GetBgTeamId() == TEAM_ALLIANCE ? "Alliance" : "Horde");
    }

    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId winnerTeamId) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", false))
            return;

        // Handle arena matches separately
        if (bg->isArena())
        {
            if (!sConfigMgr->GetOption<bool>("Glicko2.Arena.Enabled", false))
                return;

            std::lock_guard lock(_matchMutex);

            uint32 instanceId = bg->GetInstanceID();
            auto itr = _activeMatches.find(instanceId);

            if (itr == _activeMatches.end())
            {
                LOG_INFO("module.glicko2", "[Glicko2 Arena] Processing match for instance {}, winner: {}",
                    instanceId, winnerTeamId);

                // Determine arena bracket from arena type
                uint8 arenaType = bg->GetArenaType();
                ArenaBracket bracket = ArenaBracket::SLOT_2v2; // Default to 2v2

                if (arenaType == 2)
                    bracket = ArenaBracket::SLOT_2v2;
                else if (arenaType == 3)
                    bracket = ArenaBracket::SLOT_3v3;
                else if (arenaType == 5)
                    bracket = ArenaBracket::SLOT_5v5;

                // Collect all players by team
                std::vector<ObjectGuid> winnerGuids;
                std::vector<ObjectGuid> loserGuids;

                auto const& players = bg->GetPlayers();
                for (auto const& [guid, player_ref] : players)
                {
                    if (!player_ref)
                        continue;

                    if (player_ref->GetBgTeamId() == winnerTeamId)
                        winnerGuids.push_back(guid);
                    else
                        loserGuids.push_back(guid);
                }

                LOG_INFO("module.glicko2", "[Glicko2 Arena] Match complete - Bracket: {}, Winners: {}, Losers: {}",
                    static_cast<uint8>(bracket), winnerGuids.size(), loserGuids.size());

                // Update all player ratings
                if (!winnerGuids.empty() && !loserGuids.empty())
                {
                    sArenaMMRMgr->UpdateArenaMatch(bg, winnerGuids, loserGuids, bracket);
                }

                // Mark as processed so we only update once
                auto& match = _activeMatches[instanceId];
                match.processed = true;
            }

            return; // Done with arena, skip battleground logic
        }

        // Battleground logic (existing)
        LOG_INFO("module.glicko2", "[Glicko2] OnBattlegroundEndReward fired for player {} in BG instance {}, winner: {}",
            player->GetName(), bg->GetInstanceID(), winnerTeamId);

        std::lock_guard lock(_matchMutex);

        uint32 instanceId = bg->GetInstanceID();
        auto itr = _activeMatches.find(instanceId);

        if (itr == _activeMatches.end())
        {
            LOG_INFO("module.glicko2", "[Glicko2] No match tracker found for BG instance {}, building from current BG state", instanceId);
            auto& match = _activeMatches[instanceId];
            match.winnerTeam = static_cast<uint32>(winnerTeamId);

            auto const& players = bg->GetPlayers();
            for (auto const& [guid, player_ref] : players)
            {
                if (player_ref && player_ref->GetBgTeamId() == TEAM_ALLIANCE)
                    match.alliancePlayers.insert(guid);
                else if (player_ref && player_ref->GetBgTeamId() == TEAM_HORDE)
                    match.hordePlayers.insert(guid);
            }

            LOG_INFO("module.glicko2", "[Glicko2] Built match tracker: {} Alliance, {} Horde players",
                match.alliancePlayers.size(), match.hordePlayers.size());

            itr = _activeMatches.find(instanceId);
        }

        MatchTracker& match = itr->second;

        if (!match.processed)
        {
            match.winnerTeam = static_cast<uint32>(winnerTeamId);
            match.processed = true;

            LOG_INFO("module.glicko2", "[Glicko2] Processing ratings for BG instance {}, winner: {}",
                instanceId, match.winnerTeam);

            ProcessMatchRatings(bg, match);
        }

        match.alliancePlayers.erase(player->GetGUID());
        match.hordePlayers.erase(player->GetGUID());

        if (match.alliancePlayers.empty() && match.hordePlayers.empty())
        {
            _activeMatches.erase(itr);
            LOG_INFO("module.glicko2", "[Glicko2] BG instance {} cleanup complete, all players processed.", instanceId);
        }
    }

    void OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", false))
            return;

        LOG_INFO("module.glicko2", "[Glicko2] Player {} leaving BG instance {}, status: {}",
            player->GetName(), bg->GetInstanceID(), bg->GetStatus());
    }

    bool GetPlayerMatchmakingRating(ObjectGuid playerGuid, BattlegroundTypeId /*bgTypeId*/, float& outRating) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", false))
            return false;

        BattlegroundRatingData data = sGlicko2Storage->GetRating(playerGuid);
        if (data.loaded || data.matchesPlayed > 0)
        {
            outRating = data.rating;
            return true;
        }

        outRating = sConfigMgr->GetOption<float>("Glicko2.InitialRating", 1500.0f);
        return true;
    }

    bool CanAddGroupToMatchingPool(BattlegroundQueue* queue, GroupQueueInfo* group, uint32 poolPlayerCount,
                                   Battleground* /*bg*/, BattlegroundBracketId bracketId) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", false))
            return true;

        if (!group || group->Players.empty())
            return true;

        std::lock_guard lock(_poolMutex);

        // Cleanup stale pool tracking
        CleanupStalePools();

        PoolKey key{queue, bracketId};
        PoolTracker& pool = _poolTracking[key];

        // Check if this is an arena group
        if (IsArenaGroup(group))
        {
            if (!sConfigMgr->GetOption<bool>("Glicko2.Arena.Enabled", false))
                return true;

            // Determine arena bracket from ArenaType field
            ArenaBracket bracket = ArenaBracket::SLOT_2v2; // Default to 2v2

            if (group->ArenaType == 2)
                bracket = ArenaBracket::SLOT_2v2;
            else if (group->ArenaType == 3)
                bracket = ArenaBracket::SLOT_3v3;
            else if (group->ArenaType == 5)
                bracket = ArenaBracket::SLOT_5v5;

            // If pool is empty (starting fresh), clear tracking and allow first group
            if (poolPlayerCount == 0)
            {
                pool.Clear();
                pool.AddGroup(group);
                LOG_DEBUG("module.glicko2", "[Glicko2 Arena] First group added to pool, MMR: {:.1f}, Bracket: {}",
                    CalculateGroupArenaRating(group, bracket), static_cast<uint8>(bracket));
                return true;
            }

            // Calculate average MMR of the group being considered
            float groupAvgMMR = CalculateGroupArenaRating(group, bracket);

            // Calculate average MMR of players already in pool
            float poolAvgMMR = CalculatePoolArenaRating(pool.players, bracket);

            // Calculate how long this group has been in queue
            uint32 queueTimeMS = GameTime::GetGameTimeMS().count() - group->JoinTime;
            uint32 queueTimeSec = queueTimeMS / 1000;

            // Get bracket-specific relaxation settings
            float initialRange = sArenaMMRMgr->GetInitialRange(bracket);
            float maxRange = sArenaMMRMgr->GetMaxRange(bracket);
            float relaxationRate = sArenaMMRMgr->GetRelaxationRate(bracket);

            // Calculate current MMR range with time-based relaxation
            float currentRange = initialRange + (relaxationRate * queueTimeSec);
            currentRange = std::min(currentRange, maxRange);

            float mmrDiff = std::abs(groupAvgMMR - poolAvgMMR);
            bool allowed = mmrDiff <= currentRange;

            LOG_DEBUG("module.glicko2", "[Glicko2 Arena] Bracket: {}, Group MMR: {:.1f}, Pool MMR: {:.1f}, Diff: {:.1f}, Range: {:.1f}, Queue: {}s - {}",
                static_cast<uint8>(bracket), groupAvgMMR, poolAvgMMR, mmrDiff, currentRange, queueTimeSec, allowed ? "ALLOWED" : "REJECTED");

            // If allowed, track this group in the pool
            if (allowed)
            {
                pool.AddGroup(group);
            }

            return allowed;
        }

        // Battleground matchmaking logic
        // If pool is empty (starting fresh), clear tracking and allow first group
        if (poolPlayerCount == 0)
        {
            pool.Clear();
            pool.AddGroup(group);
            LOG_DEBUG("module.glicko2", "[Glicko2 Matchmaking] First group added to pool, MMR: {:.1f}",
                CalculateGroupAverageMMR(group));
            return true;
        }

        // Calculate average MMR of the group being considered
        float groupAvgMMR = CalculateGroupAverageMMR(group);

        // Calculate average MMR of players already in pool
        float poolAvgMMR = CalculatePoolAverageMMR(pool.players);

        // Calculate how long this group has been in queue
        uint32 queueTimeMS = GameTime::GetGameTimeMS().count() - group->JoinTime;
        uint32 queueTimeSec = queueTimeMS / 1000;

        // Get relaxation settings
        float initialRange = sConfigMgr->GetOption<float>("Glicko2.Matchmaking.InitialRange", 200.0f);
        float maxRange = sConfigMgr->GetOption<float>("Glicko2.Matchmaking.MaxRange", 1000.0f);
        float relaxationRate = sConfigMgr->GetOption<float>("Glicko2.Matchmaking.RelaxationRate", 10.0f);

        // Calculate current MMR range with time-based relaxation
        float currentRange = initialRange + (relaxationRate * queueTimeSec);
        currentRange = std::min(currentRange, maxRange);

        float mmrDiff = std::abs(groupAvgMMR - poolAvgMMR);
        bool allowed = mmrDiff <= currentRange;

        LOG_DEBUG("module.glicko2", "[Glicko2 Matchmaking] Group MMR: {:.1f}, Pool MMR: {:.1f}, Diff: {:.1f}, Range: {:.1f}, Queue: {}s - {}",
            groupAvgMMR, poolAvgMMR, mmrDiff, currentRange, queueTimeSec, allowed ? "ALLOWED" : "REJECTED");

        // If allowed, track this group in the pool
        if (allowed)
        {
            pool.AddGroup(group);
        }

        return allowed;
    }

private:
    void ProcessMatchRatings(Battleground* bg, MatchTracker& match)
    {
        if (match.winnerTeam == 0)
        {
            LOG_DEBUG("module.glicko2", "BG instance {} ended in a draw, no rating update.", bg->GetInstanceID());
            return;
        }

        LOG_INFO("module.glicko2", "Processing BG rating updates for instance {} (winner: {})",
            bg->GetInstanceID(), match.winnerTeam == ALLIANCE ? "Alliance" : "Horde");

        float allianceAvgMMR = CalculateAverageMMR(match.alliancePlayers);
        float allianceAvgRD = CalculateAverageRD(match.alliancePlayers);
        float hordeAvgMMR = CalculateAverageMMR(match.hordePlayers);
        float hordeAvgRD = CalculateAverageRD(match.hordePlayers);

        LOG_DEBUG("module.glicko2", "Team stats - Alliance: MMR={:.1f} RD={:.1f}, Horde: MMR={:.1f} RD={:.1f}",
            allianceAvgMMR, allianceAvgRD, hordeAvgMMR, hordeAvgRD);

        UpdateTeamRatings(match.alliancePlayers, hordeAvgMMR, hordeAvgRD, match.winnerTeam == TEAM_ALLIANCE);
        UpdateTeamRatings(match.hordePlayers, allianceAvgMMR, allianceAvgRD, match.winnerTeam == TEAM_HORDE);

        LOG_INFO("module.glicko2", "BG rating updates complete for instance {}", bg->GetInstanceID());
    }

    float CalculateAverageMMR(std::unordered_set<ObjectGuid> const& players)
    {
        if (players.empty())
            return 1500.0f;

        float totalMMR = 0.0f;
        for (ObjectGuid guid : players)
        {
            BattlegroundRatingData data = sGlicko2Storage->GetRating(guid);
            totalMMR += data.rating;
        }

        return totalMMR / static_cast<float>(players.size());
    }

    float CalculatePoolAverageMMR(std::unordered_set<ObjectGuid> const& players)
    {
        return CalculateAverageMMR(players);
    }

    void CleanupStalePools()
    {
        time_t now = time(nullptr);
        for (auto it = _poolTracking.begin(); it != _poolTracking.end();)
        {
            if (it->second.IsStale(now))
            {
                LOG_DEBUG("module.glicko2", "[Glicko2 Matchmaking] Cleaning up stale pool");
                it = _poolTracking.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    float CalculateGroupAverageMMR(GroupQueueInfo* group)
    {
        if (!group || group->Players.empty())
            return sConfigMgr->GetOption<float>("Glicko2.InitialRating", 1500.0f);

        float totalMMR = 0.0f;
        for (ObjectGuid guid : group->Players)
        {
            BattlegroundRatingData data = sGlicko2Storage->GetRating(guid);
            totalMMR += data.rating;
        }

        return totalMMR / static_cast<float>(group->Players.size());
    }

    float CalculateAverageRD(std::unordered_set<ObjectGuid> const& players)
    {
        if (players.empty())
            return 200.0f;

        float totalRD = 0.0f;
        for (ObjectGuid guid : players)
        {
            BattlegroundRatingData data = sGlicko2Storage->GetRating(guid);
            totalRD += data.ratingDeviation;
        }

        return totalRD / static_cast<float>(players.size());
    }

    void UpdateTeamRatings(std::unordered_set<ObjectGuid> const& players, float opponentAvgMMR, float opponentAvgRD, bool won)
    {
        for (ObjectGuid guid : players)
        {
            BattlegroundRatingData data = sGlicko2Storage->GetRating(guid);

            Glicko2Rating oldRating(data.rating, data.ratingDeviation, data.volatility);
            Glicko2Opponent opponent(opponentAvgMMR, opponentAvgRD, won ? 1.0f : 0.0f);

            Glicko2System glicko(sConfigMgr->GetOption<float>("Glicko2.Tau", 0.5f));
            Glicko2Rating newRating = glicko.UpdateRating(oldRating, { opponent });

            data.rating = newRating.rating;
            data.ratingDeviation = newRating.ratingDeviation;
            data.volatility = newRating.volatility;
            data.matchesPlayed++;
            if (won)
                data.wins++;
            else
                data.losses++;

            sGlicko2Storage->SetRating(guid, data);

            LOG_DEBUG("module.glicko2", "Player GUID {} rating updated: {:.1f} -> {:.1f} ({})",
                guid.ToString(), oldRating.rating, newRating.rating, won ? "WIN" : "LOSS");
        }
    }

    /// @brief Check if group is queued for an arena
    bool IsArenaGroup(GroupQueueInfo* group) const
    {
        if (!group)
            return false;

        BattlegroundTypeId bgTypeId = group->BgTypeId;
        return bgTypeId == BATTLEGROUND_AA || // All arenas use BATTLEGROUND_AA
               bgTypeId == BATTLEGROUND_NA ||
               bgTypeId == BATTLEGROUND_BE ||
               bgTypeId == BATTLEGROUND_RL ||
               bgTypeId == BATTLEGROUND_DS ||
               bgTypeId == BATTLEGROUND_RV;
    }

    /// @brief Calculate average arena rating for a group
    float CalculateGroupArenaRating(GroupQueueInfo* group, ArenaBracket bracket)
    {
        if (!group || group->Players.empty())
            return sArenaMMRMgr->GetInitialRating();

        float totalRating = 0.0f;
        for (ObjectGuid guid : group->Players)
        {
            ArenaRatingData data = sArenaRatingStorage->GetRating(guid, bracket);
            totalRating += data.rating;
        }

        return totalRating / static_cast<float>(group->Players.size());
    }

    /// @brief Calculate average arena rating for players in pool
    float CalculatePoolArenaRating(std::unordered_set<ObjectGuid> const& players, ArenaBracket bracket)
    {
        if (players.empty())
            return sArenaMMRMgr->GetInitialRating();

        float totalRating = 0.0f;
        for (ObjectGuid guid : players)
        {
            ArenaRatingData data = sArenaRatingStorage->GetRating(guid, bracket);
            totalRating += data.rating;
        }

        return totalRating / static_cast<float>(players.size());
    }
};

void AddGlicko2BGScripts()
{
    LOG_INFO("module.glicko2", "[Glicko2] Registering BG PlayerScript...");
    new Glicko2BGScript();
}
