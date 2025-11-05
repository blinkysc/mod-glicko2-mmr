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
#include "mocks/MockPlayer.h"
#include "mocks/MockBattleground.h"
#include "mocks/MockGroupQueueInfo.h"

/// Test fixture for Arena hook integration tests
/// Tests arena MMR system functionality (hook layer testing)
/// NOTE: This tests the manager classes directly rather than the script hooks
class ArenaHooksTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize test player GUIDs
        player1Guid = ObjectGuid::Create<HighGuid::Player>(1);
        player2Guid = ObjectGuid::Create<HighGuid::Player>(2);
        player3Guid = ObjectGuid::Create<HighGuid::Player>(3);
        player4Guid = ObjectGuid::Create<HighGuid::Player>(4);

        // Initialize ratings for test players
        InitializeTestPlayers();
    }

    void TearDown() override
    {
        ClearTestData();
    }

    void InitializeTestPlayers()
    {
        // Initialize arena ratings for all test players
        for (uint8 slot = 0; slot < 3; ++slot)
        {
            ArenaBracket bracket = static_cast<ArenaBracket>(slot);
            sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket);
            sArenaMMRMgr->InitializePlayerRating(player2Guid, bracket);
            sArenaMMRMgr->InitializePlayerRating(player3Guid, bracket);
            sArenaMMRMgr->InitializePlayerRating(player4Guid, bracket);
        }
    }

    void ClearTestData()
    {
        // Clear test ratings from storage
        for (uint8 slot = 0; slot < 3; ++slot)
        {
            ArenaBracket bracket = static_cast<ArenaBracket>(slot);
            sArenaRatingStorage->RemoveRating(player1Guid, bracket);
            sArenaRatingStorage->RemoveRating(player2Guid, bracket);
            sArenaRatingStorage->RemoveRating(player3Guid, bracket);
            sArenaRatingStorage->RemoveRating(player4Guid, bracket);
        }
    }

    MockGroupQueueInfo* CreateTestQueue(BattlegroundTypeId bgTypeId, uint8 arenaType,
                                       std::vector<ObjectGuid> const& players,
                                       uint32 joinTime = 0)
    {
        MockGroupQueueInfo* queue = new MockGroupQueueInfo(bgTypeId, TEAM_ALLIANCE, joinTime);
        queue->SetArenaType(arenaType);
        for (auto guid : players)
        {
            queue->AddPlayer(guid);
        }
        return queue;
    }

    ObjectGuid player1Guid;
    ObjectGuid player2Guid;
    ObjectGuid player3Guid;
    ObjectGuid player4Guid;
};

/// Test 1: MMR range calculation for matchmaking
TEST_F(ArenaHooksTest, MMRRangeCalculationForMatchmaking)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Get initial range
    float initialRange = sArenaMMRMgr->GetInitialRange(bracket);
    EXPECT_GT(initialRange, 0.0f) << "Initial MMR range should be positive";

    // Get relaxed range after 60 seconds
    float relaxedRange = sArenaMMRMgr->GetRelaxedMMRRange(60, bracket);
    EXPECT_GT(relaxedRange, initialRange) << "MMR range should expand over time";
}

/// Test 2: Maximum MMR range cap
TEST_F(ArenaHooksTest, MaximumMMRRangeCap)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Get max range
    float maxRange = sArenaMMRMgr->GetMaxRange(bracket);
    EXPECT_GT(maxRange, 0.0f) << "Maximum MMR range should be positive";

    // Even after very long wait, range should cap at max
    float veryLongWaitRange = sArenaMMRMgr->GetRelaxedMMRRange(10000, bracket);
    EXPECT_LE(veryLongWaitRange, maxRange) << "MMR range should cap at maximum";
}

/// Test 3: Relaxation rate per 30 seconds
TEST_F(ArenaHooksTest, RelaxationRatePer30Seconds)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    float initialRange = sArenaMMRMgr->GetInitialRange(bracket);
    float relaxationRate = sArenaMMRMgr->GetRelaxationRate(bracket);

    // After 30 seconds, should increase by exactly relaxation rate
    float range30s = sArenaMMRMgr->GetRelaxedMMRRange(30, bracket);
    EXPECT_FLOAT_EQ(range30s, initialRange + relaxationRate);

    // After 90 seconds, should be 3x relaxation rate
    float range90s = sArenaMMRMgr->GetRelaxedMMRRange(90, bracket);
    EXPECT_FLOAT_EQ(range90s, initialRange + (relaxationRate * 3));
}

/// Test 4: Arena match updates winner ratings
TEST_F(ArenaHooksTest, ArenaMatchUpdatesWinnerRatings)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Get initial ratings
    float initialWinner1 = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket);
    float initialWinner2 = sArenaMMRMgr->GetPlayerRating(player2Guid, bracket);

    // Simulate match (winners vs losers)
    std::vector<ObjectGuid> winners = {player1Guid, player2Guid};
    std::vector<ObjectGuid> losers = {player3Guid, player4Guid};
    sArenaMMRMgr->UpdateArenaMatch(nullptr, winners, losers, bracket);

    // Check winner ratings increased
    float newWinner1 = sArenaMMRMgr->GetPlayerRating(player1Guid, bracket);
    float newWinner2 = sArenaMMRMgr->GetPlayerRating(player2Guid, bracket);

    EXPECT_GT(newWinner1, initialWinner1) << "Winner 1 rating should increase";
    EXPECT_GT(newWinner2, initialWinner2) << "Winner 2 rating should increase";
}

/// Test 5: Arena match updates loser ratings
TEST_F(ArenaHooksTest, ArenaMatchUpdatesLoserRatings)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Get initial ratings
    float initialLoser1 = sArenaMMRMgr->GetPlayerRating(player3Guid, bracket);
    float initialLoser2 = sArenaMMRMgr->GetPlayerRating(player4Guid, bracket);

    // Simulate match (winners vs losers)
    std::vector<ObjectGuid> winners = {player1Guid, player2Guid};
    std::vector<ObjectGuid> losers = {player3Guid, player4Guid};
    sArenaMMRMgr->UpdateArenaMatch(nullptr, winners, losers, bracket);

    // Check loser ratings decreased
    float newLoser1 = sArenaMMRMgr->GetPlayerRating(player3Guid, bracket);
    float newLoser2 = sArenaMMRMgr->GetPlayerRating(player4Guid, bracket);

    EXPECT_LT(newLoser1, initialLoser1) << "Loser 1 rating should decrease";
    EXPECT_LT(newLoser2, initialLoser2) << "Loser 2 rating should decrease";
}

/// Test 6: Multiple bracket ratings are independent
TEST_F(ArenaHooksTest, MultipleBracketRatingsIndependent)
{
    // Initialize all brackets
    sArenaMMRMgr->InitializePlayerRating(player1Guid, ArenaBracket::SLOT_2v2);
    sArenaMMRMgr->InitializePlayerRating(player1Guid, ArenaBracket::SLOT_3v3);
    sArenaMMRMgr->InitializePlayerRating(player1Guid, ArenaBracket::SLOT_5v5);

    // Win matches in 2v2 and 3v3, lose in 5v5
    std::vector<ObjectGuid> winners = {player1Guid};
    std::vector<ObjectGuid> losers = {player2Guid};

    sArenaMMRMgr->UpdateArenaMatch(nullptr, winners, losers, ArenaBracket::SLOT_2v2);
    sArenaMMRMgr->UpdateArenaMatch(nullptr, winners, losers, ArenaBracket::SLOT_3v3);
    sArenaMMRMgr->UpdateArenaMatch(nullptr, losers, winners, ArenaBracket::SLOT_5v5);

    // Check each bracket independently
    float rating2v2 = sArenaMMRMgr->GetPlayerRating(player1Guid, ArenaBracket::SLOT_2v2);
    float rating3v3 = sArenaMMRMgr->GetPlayerRating(player1Guid, ArenaBracket::SLOT_3v3);
    float rating5v5 = sArenaMMRMgr->GetPlayerRating(player1Guid, ArenaBracket::SLOT_5v5);

    EXPECT_GT(rating2v2, 1500.0f) << "2v2 should increase (won)";
    EXPECT_GT(rating3v3, 1500.0f) << "3v3 should increase (won)";
    EXPECT_LT(rating5v5, 1500.0f) << "5v5 should decrease (lost)";
}

/// Test 7: Different arena brackets use separate ratings
TEST_F(ArenaHooksTest, DifferentBracketsUseSeparateRatings)
{
    // Win a 2v2 match
    std::vector<ObjectGuid> winners = {player1Guid, player2Guid};
    std::vector<ObjectGuid> losers = {player3Guid, player4Guid};
    sArenaMMRMgr->UpdateArenaMatch(nullptr, winners, losers, ArenaBracket::SLOT_2v2);

    // Check 2v2 rating changed
    float rating2v2 = sArenaMMRMgr->GetPlayerRating(player1Guid, ArenaBracket::SLOT_2v2);
    EXPECT_GT(rating2v2, 1500.0f);

    // Check 3v3 rating unchanged
    float rating3v3 = sArenaMMRMgr->GetPlayerRating(player1Guid, ArenaBracket::SLOT_3v3);
    EXPECT_FLOAT_EQ(rating3v3, 1500.0f);
}

/// Test 8: Rating deviation convergence over multiple matches
TEST_F(ArenaHooksTest, RatingDeviationConvergenceOverMatches)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    float initialRD = sArenaMMRMgr->GetPlayerRatingDeviation(player1Guid, bracket);

    // Play 5 matches
    for (int i = 0; i < 5; ++i)
    {
        std::vector<ObjectGuid> winners = {player1Guid, player2Guid};
        std::vector<ObjectGuid> losers = {player3Guid, player4Guid};
        sArenaMMRMgr->UpdateArenaMatch(nullptr, winners, losers, bracket);
    }

    float finalRD = sArenaMMRMgr->GetPlayerRatingDeviation(player1Guid, bracket);

    // RD should decrease as we become more certain of skill
    EXPECT_LT(finalRD, initialRD) << "Rating deviation should decrease with more matches";
    EXPECT_GT(finalRD, 100.0f) << "RD shouldn't drop too low after only 5 matches";
}

/// Test 9: Bracket-specific relaxation rates work independently
TEST_F(ArenaHooksTest, BracketSpecificRelaxationRates)
{
    float rate2v2 = sArenaMMRMgr->GetRelaxationRate(ArenaBracket::SLOT_2v2);
    float rate3v3 = sArenaMMRMgr->GetRelaxationRate(ArenaBracket::SLOT_3v3);
    float rate5v5 = sArenaMMRMgr->GetRelaxationRate(ArenaBracket::SLOT_5v5);

    // Each bracket can have different rates (configured in .conf)
    EXPECT_GT(rate2v2, 0.0f);
    EXPECT_GT(rate3v3, 0.0f);
    EXPECT_GT(rate5v5, 0.0f);

    // Verify each bracket's relaxation is independent
    float range2v2_60s = sArenaMMRMgr->GetRelaxedMMRRange(60, ArenaBracket::SLOT_2v2);
    float range3v3_60s = sArenaMMRMgr->GetRelaxedMMRRange(60, ArenaBracket::SLOT_3v3);
    float range5v5_60s = sArenaMMRMgr->GetRelaxedMMRRange(60, ArenaBracket::SLOT_5v5);

    // All should expand from their initial values
    EXPECT_GT(range2v2_60s, sArenaMMRMgr->GetInitialRange(ArenaBracket::SLOT_2v2));
    EXPECT_GT(range3v3_60s, sArenaMMRMgr->GetInitialRange(ArenaBracket::SLOT_3v3));
    EXPECT_GT(range5v5_60s, sArenaMMRMgr->GetInitialRange(ArenaBracket::SLOT_5v5));
}

/// Test 10: Average rating calculation for team matchmaking
TEST_F(ArenaHooksTest, AverageRatingCalculationForTeam)
{
    ArenaBracket bracket = ArenaBracket::SLOT_3v3;

    // Initialize players with default ratings
    sArenaMMRMgr->InitializePlayerRating(player1Guid, bracket);
    sArenaMMRMgr->InitializePlayerRating(player2Guid, bracket);
    sArenaMMRMgr->InitializePlayerRating(player3Guid, bracket);

    // Calculate average (all at 1500)
    std::vector<ObjectGuid> team = {player1Guid, player2Guid, player3Guid};
    float avgRating = sArenaMMRMgr->CalculateAverageRating(team, bracket);

    EXPECT_FLOAT_EQ(avgRating, 1500.0f) << "Average of 3 players at 1500 should be 1500";

    // Give player1 a win to change their rating
    std::vector<ObjectGuid> winners = {player1Guid};
    std::vector<ObjectGuid> losers = {player4Guid};
    sArenaMMRMgr->UpdateArenaMatch(nullptr, winners, losers, bracket);

    // New average should be slightly higher
    float newAvgRating = sArenaMMRMgr->CalculateAverageRating(team, bracket);
    EXPECT_GT(newAvgRating, 1500.0f) << "Average should increase after one player wins";
}
