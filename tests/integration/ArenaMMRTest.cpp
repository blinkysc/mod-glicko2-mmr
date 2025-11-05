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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "ArenaMMR.h"
#include "ArenaRatingStorage.h"
#include "mocks/MockPlayer.h"
#include "mocks/MockBattleground.h"

/// Test fixture for Arena MMR integration tests
class ArenaMMRTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize test players
        player1Guid = ObjectGuid::Create<HighGuid::Player>(1);
        player2Guid = ObjectGuid::Create<HighGuid::Player>(2);
        player3Guid = ObjectGuid::Create<HighGuid::Player>(3);
        player4Guid = ObjectGuid::Create<HighGuid::Player>(4);

        // Clear any existing ratings
        ClearTestRatings();
    }

    void TearDown() override
    {
        ClearTestRatings();
    }

    void ClearTestRatings()
    {
        // Clear ratings from storage for all brackets
        for (uint8 slot = 0; slot < 3; ++slot)
        {
            ArenaBracket bracket = static_cast<ArenaBracket>(slot);
            sArenaRatingStorage->RemoveRating(player1Guid, bracket);
            sArenaRatingStorage->RemoveRating(player2Guid, bracket);
            sArenaRatingStorage->RemoveRating(player3Guid, bracket);
            sArenaRatingStorage->RemoveRating(player4Guid, bracket);
        }
    }

    ObjectGuid player1Guid;
    ObjectGuid player2Guid;
    ObjectGuid player3Guid;
    ObjectGuid player4Guid;
};

/// Test 1: Initialize new player with default rating
TEST_F(ArenaMMRTest, InitializeNewPlayerRating)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Initialize player rating
    sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket);

    // Check default values
    float rating = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket);
    float rd = sArenaMMRMgr->GetPlayerRatingDeviation(player1Guid, bracket);

    EXPECT_FLOAT_EQ(rating, 1500.0f);
    EXPECT_FLOAT_EQ(rd, 350.0f);  // ArenaMMR uses 350 for starting RD
}

/// Test 2: Winner rating increases after match
TEST_F(ArenaMMRTest, WinnerRatingIncreasesAfterMatch)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Initialize ratings
    sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket);
    sArenaMMRMgr->InitializePlayerRating(player2Guid, bracket);

    float initialRating = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket);

    // Player 1 wins against Player 2
    std::vector<ObjectGuid> opponents = {player2Guid};
    sArenaMMRMgr->UpdatePlayerRating(player1Guid, bracket, true, opponents);

    float newRating = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket);

    EXPECT_GT(newRating, initialRating) << "Winner's rating should increase";
}

/// Test 3: Loser rating decreases after match
TEST_F(ArenaMMRTest, LoserRatingDecreasesAfterMatch)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Initialize ratings
    sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket);
    sArenaMMRMgr->InitializePlayerRating(player2Guid, bracket);

    float initialRating = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket);

    // Player 1 loses against Player 2
    std::vector<ObjectGuid> opponents = {player2Guid};
    sArenaMMRMgr->UpdatePlayerRating(player1Guid, bracket, false, opponents);

    float newRating = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket);

    EXPECT_LT(newRating, initialRating) << "Loser's rating should decrease";
}

/// Test 4: Team vs Team rating update (2v2 scenario)
TEST_F(ArenaMMRTest, TeamVsTeamRatingUpdate)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Initialize all players
    sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket);
    sArenaMMRMgr->InitializePlayerRating(player2Guid, bracket);
    sArenaMMRMgr->InitializePlayerRating(player3Guid, bracket);
    sArenaMMRMgr->InitializePlayerRating(player4Guid, bracket);

    // Team 1 (winners): player1, player2
    std::vector<ObjectGuid> winners = {player1Guid, player2Guid};

    // Team 2 (losers): player3, player4
    std::vector<ObjectGuid> losers = {player3Guid, player4Guid};

    // Update match ratings (pass nullptr for bg parameter)
    sArenaMMRMgr->UpdateArenaMatch(nullptr, winners, losers, bracket);

    // Check winners' ratings increased
    float winner1Rating = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket);
    float winner2Rating = sArenaMMRMgr->GetPlayerRating(player2Guid, bracket);
    EXPECT_GT(winner1Rating, 1500.0f);
    EXPECT_GT(winner2Rating, 1500.0f);

    // Check losers' ratings decreased
    float loser1Rating = sArenaMMRMgr->GetPlayerRating(player3Guid, bracket);
    float loser2Rating = sArenaMMRMgr->GetPlayerRating(player4Guid, bracket);
    EXPECT_LT(loser1Rating, 1500.0f);
    EXPECT_LT(loser2Rating, 1500.0f);
}

/// Test 5: Separate bracket ratings are independent
TEST_F(ArenaMMRTest, SeparateBracketRatingsAreIndependent)
{
    // Initialize player in 2v2
    ArenaBracket bracket2v2 = ArenaBracket::SLOT_2v2;
    sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket2v2);

    // Win a match in 2v2
    std::vector<ObjectGuid> opponents = {player2Guid};
    sArenaMMRMgr->UpdatePlayerRating(player1Guid, bracket2v2, true, opponents);

    float rating2v2 = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket2v2);

    // Initialize same player in 3v3
    ArenaBracket bracket3v3 = ArenaBracket::SLOT_3v3;
    sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket3v3);

    float rating3v3 = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket3v3);

    // 3v3 rating should be default (unchanged by 2v2 match)
    EXPECT_FLOAT_EQ(rating3v3, 1500.0f);
    EXPECT_GT(rating2v2, 1500.0f);
    EXPECT_NE(rating2v2, rating3v3);
}

/// Test 6: Calculate average opponent rating
TEST_F(ArenaMMRTest, CalculateAverageOpponentRating)
{
    ArenaBracket bracket = ArenaBracket::SLOT_3v3;

    // Initialize players with different ratings
    sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket);
    sArenaMMRMgr->InitializePlayerRating(player2Guid, bracket);
    sArenaMMRMgr->InitializePlayerRating(player3Guid, bracket);

    // Manually set ratings (in real test, this would use storage directly)
    // Assuming: player1=1500, player2=1600, player3=1700

    std::vector<ObjectGuid> opponents = {player1Guid, player2Guid, player3Guid};
    float avgRating = sArenaMMRMgr->CalculateAverageRating(opponents, bracket);

    // Average should be close to 1500 (all start at 1500)
    EXPECT_NEAR(avgRating, 1500.0f, 50.0f);
}

/// Test 7: MMR range relaxation per bracket
TEST_F(ArenaMMRTest, MMRRangeRelaxationPerBracket)
{
    ArenaBracket bracket2v2 = ArenaBracket::SLOT_2v2;
    ArenaBracket bracket3v3 = ArenaBracket::SLOT_3v3;

    // Get initial ranges
    float initial2v2 = sArenaMMRMgr->GetInitialRange(bracket2v2);
    float initial3v3 = sArenaMMRMgr->GetInitialRange(bracket3v3);

    // Get relaxed range after 60 seconds
    float relaxed2v2_60s = sArenaMMRMgr->GetRelaxedMMRRange(60, bracket2v2);
    float relaxed3v3_60s = sArenaMMRMgr->GetRelaxedMMRRange(60, bracket3v3);

    // Ranges should expand
    EXPECT_GT(relaxed2v2_60s, initial2v2);
    EXPECT_GT(relaxed3v3_60s, initial3v3);

    // Get relaxed range after 300 seconds
    float relaxed2v2_300s = sArenaMMRMgr->GetRelaxedMMRRange(300, bracket2v2);

    // Should continue expanding
    EXPECT_GT(relaxed2v2_300s, relaxed2v2_60s);

    // But should cap at max range
    float maxRange = sArenaMMRMgr->GetMaxRange(bracket2v2);
    EXPECT_LE(relaxed2v2_300s, maxRange);
}

/// Test 8: Relaxation rate is per 30 seconds
TEST_F(ArenaMMRTest, RelaxationRateIsPer30Seconds)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    float initialRange = sArenaMMRMgr->GetInitialRange(bracket);
    float relaxationRate = sArenaMMRMgr->GetRelaxationRate(bracket);

    // After 30 seconds, range should increase by exactly the relaxation rate
    float range30s = sArenaMMRMgr->GetRelaxedMMRRange(30, bracket);
    float expectedRange = initialRange + relaxationRate;

    EXPECT_FLOAT_EQ(range30s, expectedRange);

    // After 60 seconds, should be 2x relaxation rate
    float range60s = sArenaMMRMgr->GetRelaxedMMRRange(60, bracket);
    expectedRange = initialRange + (relaxationRate * 2);

    EXPECT_FLOAT_EQ(range60s, expectedRange);
}

/// Test 9: Rating deviation decreases after match
TEST_F(ArenaMMRTest, RatingDeviationDecreasesAfterMatch)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket);
    float initialRD = sArenaMMRMgr->GetPlayerRatingDeviation(player1Guid, bracket);

    // Play a match
    std::vector<ObjectGuid> opponents = {player2Guid};
    sArenaMMRMgr->UpdatePlayerRating(player1Guid, bracket, true, opponents);

    float newRD = sArenaMMRMgr->GetPlayerRatingDeviation(player1Guid, bracket);

    EXPECT_LT(newRD, initialRD) << "Rating deviation should decrease after match";
}

/// Test 10: Disabled system doesn't update ratings
TEST_F(ArenaMMRTest, DisabledSystemDoesNotUpdateRatings)
{
    // Note: This test would require manipulating config or using a mock config manager
    // Placeholder test structure:

    // TODO: Implement config mock to disable system
    // EXPECT_FALSE(sArenaMMRMgr->IsEnabled());

    // Ratings should not change when disabled
}
