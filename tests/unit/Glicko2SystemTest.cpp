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
#include "Glicko2.h"
#include <cmath>

/// Test fixture for Glicko-2 algorithm tests
class Glicko2SystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        system = new Glicko2System(0.5f); // Default tau = 0.5
    }

    void TearDown() override
    {
        delete system;
    }

    Glicko2System* system;

    // Helper to check float equality with tolerance
    void ExpectNear(float actual, float expected, float tolerance = 0.1f)
    {
        EXPECT_NEAR(actual, expected, tolerance);
    }
};

/// Test 1: Initial rating should have default values
TEST_F(Glicko2SystemTest, InitialRatingDefaultValues)
{
    Glicko2Rating rating;

    EXPECT_FLOAT_EQ(rating.rating, 1500.0f);
    EXPECT_FLOAT_EQ(rating.ratingDeviation, 200.0f);  // Default is 200, not 350
    EXPECT_FLOAT_EQ(rating.volatility, 0.06f);
}

/// Test 2: Custom initial rating values
TEST_F(Glicko2SystemTest, CustomInitialRating)
{
    Glicko2Rating rating(1800.0f, 250.0f, 0.05f);

    EXPECT_FLOAT_EQ(rating.rating, 1800.0f);
    EXPECT_FLOAT_EQ(rating.ratingDeviation, 250.0f);
    EXPECT_FLOAT_EQ(rating.volatility, 0.05f);
}

/// Test 3: Winning against equal opponent increases rating
TEST_F(Glicko2SystemTest, WinAgainstEqualOpponentIncreasesRating)
{
    Glicko2Rating player(1500.0f, 200.0f, 0.06f);
    std::vector<Glicko2Opponent> opponents;
    opponents.emplace_back(1500.0f, 200.0f, 1.0f); // Win (score = 1.0)

    Glicko2Rating newRating = system->UpdateRating(player, opponents);

    EXPECT_GT(newRating.rating, player.rating) << "Rating should increase after winning";
    EXPECT_LT(newRating.ratingDeviation, player.ratingDeviation) << "RD should decrease after match";
}

/// Test 4: Losing against equal opponent decreases rating
TEST_F(Glicko2SystemTest, LossAgainstEqualOpponentDecreasesRating)
{
    Glicko2Rating player(1500.0f, 200.0f, 0.06f);
    std::vector<Glicko2Opponent> opponents;
    opponents.emplace_back(1500.0f, 200.0f, 0.0f); // Loss (score = 0.0)

    Glicko2Rating newRating = system->UpdateRating(player, opponents);

    EXPECT_LT(newRating.rating, player.rating) << "Rating should decrease after losing";
    EXPECT_LT(newRating.ratingDeviation, player.ratingDeviation) << "RD should decrease after match";
}

/// Test 5: Winning against higher-rated opponent gives bigger rating gain
TEST_F(Glicko2SystemTest, UpsetWinGivesBiggerGain)
{
    Glicko2Rating player(1500.0f, 200.0f, 0.06f);

    // Win against equal opponent
    std::vector<Glicko2Opponent> equalOpponent;
    equalOpponent.emplace_back(1500.0f, 200.0f, 1.0f);
    Glicko2Rating newRating1 = system->UpdateRating(player, equalOpponent);
    float equalGain = newRating1.rating - player.rating;

    // Win against higher-rated opponent
    std::vector<Glicko2Opponent> higherOpponent;
    higherOpponent.emplace_back(1800.0f, 200.0f, 1.0f);
    Glicko2Rating newRating2 = system->UpdateRating(player, higherOpponent);
    float higherGain = newRating2.rating - player.rating;

    EXPECT_GT(higherGain, equalGain) << "Upset win should give bigger rating gain";
}

/// Test 6: Winning against lower-rated opponent gives smaller rating gain
TEST_F(Glicko2SystemTest, ExpectedWinGivesSmallerGain)
{
    Glicko2Rating player(1800.0f, 200.0f, 0.06f);

    // Win against equal opponent
    std::vector<Glicko2Opponent> equalOpponent;
    equalOpponent.emplace_back(1800.0f, 200.0f, 1.0f);
    Glicko2Rating newRating1 = system->UpdateRating(player, equalOpponent);
    float equalGain = newRating1.rating - player.rating;

    // Win against lower-rated opponent
    std::vector<Glicko2Opponent> lowerOpponent;
    lowerOpponent.emplace_back(1500.0f, 200.0f, 1.0f);
    Glicko2Rating newRating2 = system->UpdateRating(player, lowerOpponent);
    float lowerGain = newRating2.rating - player.rating;

    EXPECT_LT(lowerGain, equalGain) << "Expected win should give smaller rating gain";
}

/// Test 7: Rating deviation decreases after matches
TEST_F(Glicko2SystemTest, RatingDeviationDecreasesAfterMatch)
{
    Glicko2Rating player(1500.0f, 350.0f, 0.06f);
    std::vector<Glicko2Opponent> opponents;
    opponents.emplace_back(1500.0f, 200.0f, 1.0f);

    Glicko2Rating newRating = system->UpdateRating(player, opponents);

    EXPECT_LT(newRating.ratingDeviation, player.ratingDeviation)
        << "Rating deviation should decrease as uncertainty reduces";
}

/// Test 8: High RD players have larger rating swings
TEST_F(Glicko2SystemTest, HighRDPlayersHaveLargerSwings)
{
    // Player with low RD (experienced)
    Glicko2Rating veteran(1500.0f, 50.0f, 0.055f);
    std::vector<Glicko2Opponent> opponents1;
    opponents1.emplace_back(1500.0f, 200.0f, 1.0f);
    Glicko2Rating newVeteran = system->UpdateRating(veteran, opponents1);
    float veteranGain = newVeteran.rating - veteran.rating;

    // Player with high RD (new)
    Glicko2Rating newbie(1500.0f, 350.0f, 0.06f);
    std::vector<Glicko2Opponent> opponents2;
    opponents2.emplace_back(1500.0f, 200.0f, 1.0f);
    Glicko2Rating newNewbie = system->UpdateRating(newbie, opponents2);
    float newbieGain = newNewbie.rating - newbie.rating;

    EXPECT_GT(newbieGain, veteranGain)
        << "New player with high RD should have larger rating changes";
}

/// Test 9: Multiple opponents calculation
TEST_F(Glicko2SystemTest, MultipleOpponentsCalculation)
{
    Glicko2Rating player(1500.0f, 200.0f, 0.06f);

    // Win against multiple opponents
    std::vector<Glicko2Opponent> opponents;
    opponents.emplace_back(1600.0f, 150.0f, 1.0f); // Win
    opponents.emplace_back(1550.0f, 180.0f, 1.0f); // Win
    opponents.emplace_back(1450.0f, 200.0f, 0.0f); // Loss

    Glicko2Rating newRating = system->UpdateRating(player, opponents);

    // Should increase rating (2 wins, 1 loss)
    EXPECT_GT(newRating.rating, player.rating);

    // RD should decrease with multiple matches
    EXPECT_LT(newRating.ratingDeviation, player.ratingDeviation);
}

/// Test 10: Volatility stays within reasonable bounds
TEST_F(Glicko2SystemTest, VolatilityStaysWithinBounds)
{
    Glicko2Rating player(1500.0f, 200.0f, 0.06f);
    std::vector<Glicko2Opponent> opponents;
    opponents.emplace_back(1500.0f, 200.0f, 1.0f);

    Glicko2Rating newRating = system->UpdateRating(player, opponents);

    // Volatility should stay within reasonable bounds (typically 0.04 - 0.08)
    EXPECT_GE(newRating.volatility, 0.03f);
    EXPECT_LE(newRating.volatility, 0.10f);
}

/// Test 11: Draw (0.5 score) results in minimal rating change
TEST_F(Glicko2SystemTest, DrawResultsInMinimalChange)
{
    Glicko2Rating player(1500.0f, 200.0f, 0.06f);
    std::vector<Glicko2Opponent> opponents;
    opponents.emplace_back(1500.0f, 200.0f, 0.5f); // Draw

    Glicko2Rating newRating = system->UpdateRating(player, opponents);

    // Rating should stay very close to original
    ExpectNear(newRating.rating, player.rating, 5.0f);
}

/// Test 12: System with different tau values
TEST_F(Glicko2SystemTest, DifferentTauValues)
{
    Glicko2System conservativeSystem(0.3f); // Low tau = conservative
    Glicko2System volatileSystem(1.0f);     // High tau = volatile

    Glicko2Rating player(1500.0f, 200.0f, 0.06f);
    std::vector<Glicko2Opponent> opponents;
    opponents.emplace_back(1800.0f, 150.0f, 1.0f); // Upset win

    Glicko2Rating conservativeResult = conservativeSystem.UpdateRating(player, opponents);
    Glicko2Rating volatileResult = volatileSystem.UpdateRating(player, opponents);

    // Volatile system should allow larger rating changes
    float conservativeGain = conservativeResult.rating - player.rating;
    float volatileGain = volatileResult.rating - player.rating;

    EXPECT_GT(volatileGain, conservativeGain)
        << "Higher tau should result in larger rating changes";
}

/// Test 13: Rating convergence over multiple matches
TEST_F(Glicko2SystemTest, RatingConvergenceOverTime)
{
    Glicko2Rating player(1500.0f, 350.0f, 0.06f);

    // Simulate 10 wins against 1600-rated opponents
    for (int i = 0; i < 10; ++i)
    {
        std::vector<Glicko2Opponent> opponents;
        opponents.emplace_back(1600.0f, 150.0f, 1.0f);
        player = system->UpdateRating(player, opponents);
    }

    // After consistent wins against higher-rated opponents:
    // 1. Rating should increase significantly
    EXPECT_GT(player.rating, 1600.0f);

    // 2. RD should decrease (more certain about skill), but not below ~140 after 10 matches
    EXPECT_LT(player.ratingDeviation, 200.0f) << "RD should decrease from starting value";
    EXPECT_GT(player.ratingDeviation, 100.0f) << "RD shouldn't drop too low after only 10 matches";

    // 3. Volatility should stabilize
    EXPECT_LT(player.volatility, 0.07f);
}

/// Test 14: No opponents increases RD (inactive period)
TEST_F(Glicko2SystemTest, NoOpponentsIncreasesRD)
{
    Glicko2Rating player(1500.0f, 200.0f, 0.06f);
    std::vector<Glicko2Opponent> opponents; // Empty

    Glicko2Rating newRating = system->UpdateRating(player, opponents);

    // In Glicko-2, an inactive rating period increases RD (uncertainty grows)
    EXPECT_FLOAT_EQ(newRating.rating, player.rating) << "Rating unchanged without matches";
    EXPECT_GT(newRating.ratingDeviation, player.ratingDeviation) << "RD should increase during inactivity";
    EXPECT_FLOAT_EQ(newRating.volatility, player.volatility) << "Volatility unchanged without matches";
}
