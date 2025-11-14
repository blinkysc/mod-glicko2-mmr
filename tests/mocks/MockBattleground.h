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

#ifndef MOCK_BATTLEGROUND_H
#define MOCK_BATTLEGROUND_H

#include "gmock/gmock.h"
#include "Battleground.h"
#include "BattlegroundScore.h"
#include "ObjectGuid.h"
#include <map>
#include <vector>

/// Mock Battleground class for testing match simulation
class MockBattleground
{
public:
    MockBattleground(BattlegroundTypeId bgTypeId, bool isArena = false, uint8 arenaType = 0)
        : _bgTypeId(bgTypeId), _isArena(isArena), _arenaType(arenaType),
          _status(STATUS_WAIT_JOIN), _winner(PVP_TEAM_NEUTRAL) {}

    virtual ~MockBattleground() = default;

    // Battleground identification
    BattlegroundTypeId GetBgTypeID() const { return _bgTypeId; }
    bool isArena() const { return _isArena; }
    uint8 GetArenaType() const { return _arenaType; }

    // Match status
    BattlegroundStatus GetStatus() const { return _status; }
    void SetStatus(BattlegroundStatus status) { _status = status; }

    // Winner determination
    PvPTeamId GetWinner() const { return _winner; }
    void SetWinner(PvPTeamId winner) { _winner = winner; }

    // Player management
    void AddPlayer(ObjectGuid playerGuid, TeamId team)
    {
        _players[playerGuid] = team;
        if (team == TEAM_ALLIANCE)
            _alliancePlayers.push_back(playerGuid);
        else
            _hordePlayers.push_back(playerGuid);
    }

    std::map<ObjectGuid, TeamId> const& GetPlayers() const { return _players; }
    std::vector<ObjectGuid> const& GetAlliancePlayers() const { return _alliancePlayers; }
    std::vector<ObjectGuid> const& GetHordePlayers() const { return _hordePlayers; }

    TeamId GetPlayerTeam(ObjectGuid guid) const
    {
        auto it = _players.find(guid);
        return it != _players.end() ? it->second : TEAM_NEUTRAL;
    }

    // Score tracking
    void SetPlayerScore(ObjectGuid guid, uint32 score)
    {
        _playerScores[guid] = score;
    }

    uint32 GetPlayerScore(ObjectGuid guid) const
    {
        auto it = _playerScores.find(guid);
        return it != _playerScores.end() ? it->second : 0;
    }

    // Arena team retrieval (for hook testing)
    std::vector<ObjectGuid> GetWinnerGuids() const
    {
        return _winner == PVP_TEAM_ALLIANCE ? _alliancePlayers : _hordePlayers;
    }

    std::vector<ObjectGuid> GetLoserGuids() const
    {
        return _winner == PVP_TEAM_ALLIANCE ? _hordePlayers : _alliancePlayers;
    }

private:
    BattlegroundTypeId _bgTypeId;
    bool _isArena;
    uint8 _arenaType;
    BattlegroundStatus _status;
    PvPTeamId _winner;

    std::map<ObjectGuid, TeamId> _players;
    std::vector<ObjectGuid> _alliancePlayers;
    std::vector<ObjectGuid> _hordePlayers;
    std::map<ObjectGuid, uint32> _playerScores;
};

#endif // MOCK_BATTLEGROUND_H
