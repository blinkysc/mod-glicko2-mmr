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

#include "Chat.h"
#include "Language.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "BattlegroundMMR.h"
#include "Glicko2PlayerStorage.h"
#include "Config.h"

using namespace Acore::ChatCommands;

/// @brief GM commands for viewing and managing player battleground ratings
class battleground_mmr_commandscript : public CommandScript
{
public:
    battleground_mmr_commandscript() : CommandScript("battleground_mmr_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable bgMMRCommandTable =
        {
            { "info",    HandleBGMMRInfoCommand,    SEC_GAMEMASTER, Console::No },
            { "set",     HandleBGMMRSetCommand,     SEC_ADMINISTRATOR, Console::No },
            { "reset",   HandleBGMMRResetCommand,   SEC_ADMINISTRATOR, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "bgmmr", bgMMRCommandTable },
        };

        return commandTable;
    }

    static bool HandleBGMMRInfoCommand(ChatHandler* handler, Optional<PlayerIdentifier> player)
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
        {
            handler->SendSysMessage("Battleground MMR system is disabled.");
            return true;
        }

        Player* target = player ? player->GetConnectedPlayer() : handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            return false;
        }

        BattlegroundRatingData bgRating = sGlicko2Storage->GetRating(target->GetGUID());
        float gearScore = sBattlegroundMMRMgr->CalculateGearScore(target);

        float normalizedGear = (gearScore / 300.0f) * 1500.0f;
        float combinedScore = (bgRating.rating * sBattlegroundMMRMgr->GetMMRWeight()) +
                             (normalizedGear * sBattlegroundMMRMgr->GetGearWeight());

        handler->PSendSysMessage("Battleground MMR Info for {}:", target->GetName());
        handler->PSendSysMessage("Rating: {:.2f} (RD: {:.2f}, Volatility: {:.4f})",
                                 bgRating.rating, bgRating.ratingDeviation, bgRating.volatility);
        handler->PSendSysMessage("Record: {} wins, {} losses ({} total matches)",
                                 bgRating.wins, bgRating.losses, bgRating.matchesPlayed);
        handler->PSendSysMessage("Gear Score: {:.2f}", gearScore);
        handler->PSendSysMessage("Combined Score: {:.2f}", combinedScore);

        if (bgRating.matchesPlayed > 0)
        {
            float winRate = (static_cast<float>(bgRating.wins) / bgRating.matchesPlayed) * 100.0f;
            handler->PSendSysMessage("Win Rate: {:.1f}%", winRate);
        }

        return true;
    }

    static bool HandleBGMMRSetCommand(ChatHandler* handler, Optional<PlayerIdentifier> player, float rating)
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
        {
            handler->SendSysMessage("Battleground MMR system is disabled.");
            return true;
        }

        Player* target = player ? player->GetConnectedPlayer() : handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            return false;
        }

        BattlegroundRatingData bgRating = sGlicko2Storage->GetRating(target->GetGUID());
        bgRating.rating = rating;
        bgRating.ratingDeviation = 200.0f;
        bgRating.volatility = 0.06f;
        bgRating.loaded = true;

        sGlicko2Storage->SetRating(target->GetGUID(), bgRating);
        sGlicko2Storage->SaveRating(target->GetGUID(), bgRating);

        handler->PSendSysMessage("Set {}'s Battleground MMR to {:.2f}", target->GetName(), rating);

        return true;
    }

    static bool HandleBGMMRResetCommand(ChatHandler* handler, Optional<PlayerIdentifier> player)
    {
        if (!sConfigMgr->GetOption<bool>("Glicko2.Enabled", true))
        {
            handler->SendSysMessage("Battleground MMR system is disabled.");
            return true;
        }

        Player* target = player ? player->GetConnectedPlayer() : handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            return false;
        }

        BattlegroundRatingData bgRating;
        bgRating.rating = sConfigMgr->GetOption<float>("Glicko2.InitialRating", 1500.0f);
        bgRating.ratingDeviation = sConfigMgr->GetOption<float>("Glicko2.InitialRatingDeviation", 350.0f);
        bgRating.volatility = sConfigMgr->GetOption<float>("Glicko2.InitialVolatility", 0.06f);
        bgRating.matchesPlayed = 0;
        bgRating.wins = 0;
        bgRating.losses = 0;
        bgRating.loaded = true;

        sGlicko2Storage->SetRating(target->GetGUID(), bgRating);

        CharacterDatabase.Execute("DELETE FROM character_battleground_rating WHERE guid = {}",
                                   target->GetGUID().GetCounter());
        CharacterDatabase.Execute("DELETE FROM character_battleground_rating_history WHERE guid = {}",
                                   target->GetGUID().GetCounter());

        handler->PSendSysMessage("Reset {}'s Battleground MMR to default values", target->GetName());

        return true;
    }
};

void AddGlicko2CommandScripts()
{
    LOG_INFO("module.glicko2", "[Glicko2] Registering CommandScript...");
    new battleground_mmr_commandscript();
}
