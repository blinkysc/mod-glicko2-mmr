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
#include "ArenaMMR.h"
#include "BattlegroundMMR.h"
#include "Log.h"

class Glicko2WorldScript : public WorldScript
{
public:
    Glicko2WorldScript() : WorldScript("Glicko2WorldScript") { }

    void OnStartup() override
    {
        LOG_INFO("module", ">> Loading Glicko-2 MMR System...");

        // Load battleground MMR configuration
        sBattlegroundMMRMgr->LoadConfig();

        // Load arena MMR configuration
        sArenaMMRMgr->LoadConfig();

        LOG_INFO("module", ">> Glicko-2 MMR System loaded successfully!");
    }
};

void AddGlicko2WorldScripts()
{
    new Glicko2WorldScript();
}
