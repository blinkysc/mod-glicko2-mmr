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
#include "Config.h"
#include "Glicko2PlayerStorage.h"
#include "BattlegroundMMR.h"
#include "Log.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>

struct MatchTracker
{
    std::unordered_set<ObjectGuid> alliancePlayers;
    std::unordered_set<ObjectGuid> hordePlayers;
    uint32 winnerTeam = 0;
    bool processed = false;
    time_t startTime = 0;
};

/// @brief Handles battleground rating updates using Glicko-2 algorithm
class Glicko2BGScript : public AllBattlegroundScript
{
    std::unordered_map<uint32, MatchTracker> _activeMatches;
    mutable std::mutex _matchMutex;

public:
    Glicko2BGScript() : AllBattlegroundScript("Glicko2BGScript")
    {
        LOG_INFO("module.glicko2", "[Glicko2] Glicko2BGScript initialized");
    }

    void OnBattlegroundAddPlayer(Battleground* bg, Player* player) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
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
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
            return;

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
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
            return;

        LOG_INFO("module.glicko2", "[Glicko2] Player {} leaving BG instance {}, status: {}",
            player->GetName(), bg->GetInstanceID(), bg->GetStatus());
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
};

void AddGlicko2BGScripts()
{
    LOG_INFO("module.glicko2", "[Glicko2] Registering BG PlayerScript...");
    new Glicko2BGScript();
}
