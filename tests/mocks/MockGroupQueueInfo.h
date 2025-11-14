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

#ifndef MOCK_GROUP_QUEUE_INFO_H
#define MOCK_GROUP_QUEUE_INFO_H

#include "gmock/gmock.h"
#include "Battleground.h"
#include "ObjectGuid.h"
#include <vector>

/// Mock GroupQueueInfo for testing matchmaking logic
/// Simulates a group in the battleground/arena queue
class MockGroupQueueInfo
{
public:
    MockGroupQueueInfo(BattlegroundTypeId bgTypeId, TeamId team, uint32 joinTime = 0)
        : BgTypeId(bgTypeId), Team(team), JoinTime(joinTime),
          ArenaType(0), IsInvitedToBGInstanceGUID(0), ArenaTeamRating(1500) {}

    // Core queue info
    BattlegroundTypeId BgTypeId;
    TeamId Team;
    uint32 JoinTime;
    uint8 ArenaType;
    uint32 IsInvitedToBGInstanceGUID;
    uint32 ArenaTeamRating;

    // Players in group
    std::vector<ObjectGuid> Players;

    // Helper methods
    void AddPlayer(ObjectGuid guid)
    {
        Players.push_back(guid);
    }

    void SetArenaType(uint8 type)
    {
        ArenaType = type;
    }

    void SetJoinTime(uint32 time)
    {
        JoinTime = time;
    }

    void SetArenaTeamRating(uint32 rating)
    {
        ArenaTeamRating = rating;
    }

    uint32 GetQueueTimeSeconds(uint32 currentTime) const
    {
        return currentTime > JoinTime ? (currentTime - JoinTime) : 0;
    }

    bool IsInvited() const
    {
        return IsInvitedToBGInstanceGUID != 0;
    }
};

#endif // MOCK_GROUP_QUEUE_INFO_H
