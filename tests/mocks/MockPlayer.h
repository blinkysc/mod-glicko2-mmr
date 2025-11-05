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

#ifndef MOCK_PLAYER_H
#define MOCK_PLAYER_H

#include "gmock/gmock.h"
#include "ObjectGuid.h"
#include "Player.h"

/// Mock Player class for testing Glicko-2 MMR system
/// Provides minimal interface needed for rating calculations and matchmaking
class MockPlayer
{
public:
    MockPlayer(ObjectGuid guid, std::string name = "TestPlayer")
        : _guid(guid), _name(name), _teamId(TEAM_ALLIANCE), _avgItemLevel(200.0f) {}

    virtual ~MockPlayer() = default;

    // Core identity
    ObjectGuid GetGUID() const { return _guid; }
    std::string GetName() const { return _name; }
    TeamId GetTeamId() const { return _teamId; }

    // Arena-related
    uint32 GetArenaTeamId(uint8 slot) const { return _arenaTeamIds[slot]; }
    void SetArenaTeamId(uint8 slot, uint32 teamId) { _arenaTeamIds[slot] = teamId; }

    // Gear score (for battleground testing)
    float GetAverageItemLevel() const { return _avgItemLevel; }
    void SetAverageItemLevel(float ilvl) { _avgItemLevel = ilvl; }

    // Item queries (for gear score calculation)
    Item* GetItemByPos(uint8 bag, uint8 slot) const
    {
        auto key = std::make_pair(bag, slot);
        auto it = _items.find(key);
        return it != _items.end() ? it->second : nullptr;
    }

    void SetItemByPos(uint8 bag, uint8 slot, Item* item)
    {
        _items[std::make_pair(bag, slot)] = item;
    }

    // Setters for test configuration
    void SetTeamId(TeamId team) { _teamId = team; }
    void SetName(const std::string& name) { _name = name; }

private:
    ObjectGuid _guid;
    std::string _name;
    TeamId _teamId;
    float _avgItemLevel;
    uint32 _arenaTeamIds[3] = {0, 0, 0}; // 2v2, 3v3, 5v5
    std::map<std::pair<uint8, uint8>, Item*> _items;
};

/// Mock Item class for gear score testing
class MockItem
{
public:
    MockItem(uint32 itemLevel, uint32 quality = ITEM_QUALITY_RARE)
        : _itemLevel(itemLevel), _quality(quality) {}

    uint32 GetItemLevel() const { return _itemLevel; }
    uint32 GetQuality() const { return _quality; }

private:
    uint32 _itemLevel;
    uint32 _quality;
};

#endif // MOCK_PLAYER_H
