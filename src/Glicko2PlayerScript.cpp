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
#include "Config.h"
#include "Glicko2PlayerStorage.h"
#include "Log.h"

/// @brief Handles loading and saving of player BG ratings
class Glicko2PlayerScript : public PlayerScript
{
public:
    Glicko2PlayerScript() : PlayerScript("Glicko2PlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
            return;

        sGlicko2Storage->LoadRating(player->GetGUID());
        LOG_INFO("module.glicko2", "[Glicko2] Player {} logged in, BG rating loaded.", player->GetName());
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
            return;

        sGlicko2Storage->SaveRating(player->GetGUID());
        LOG_DEBUG("module.glicko2", "Player {} logged out, BG rating saved.", player->GetName());
    }

    void OnPlayerSave(Player* player) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
            return;

        sGlicko2Storage->SaveRating(player->GetGUID());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
            return;

        sGlicko2Storage->RemoveRating(guid);
        LOG_DEBUG("module.glicko2", "Player GUID {} deleted, BG rating removed from cache.", guid.ToString());
    }
};

void AddGlicko2PlayerScripts()
{
    LOG_INFO("module.glicko2", "[Glicko2] Registering PlayerScript...");
    new Glicko2PlayerScript();
}
