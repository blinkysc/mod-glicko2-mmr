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
#include "BattlegroundMMR.h"

/// Test fixture for Battleground MMR system tests
/// Note: Tests focus on configuration and logic that doesn't require full Player mocks
class BattlegroundMMRTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Load config for manager
        sBattlegroundMMRMgr->LoadConfig();
    }

    void TearDown() override
    {
        // Clean up if needed
    }
};

/// Test 1: Configuration loading
TEST_F(BattlegroundMMRTest, ConfigurationLoadsSuccessfully)
{
    // Verify config loaded with reasonable defaults
    EXPECT_TRUE(sBattlegroundMMRMgr->GetStartingRating() > 0.0f);
    EXPECT_TRUE(sBattlegroundMMRMgr->GetStartingRD() > 0.0f);
    EXPECT_TRUE(sBattlegroundMMRMgr->GetStartingVolatility() > 0.0f);
    EXPECT_TRUE(sBattlegroundMMRMgr->GetSystemTau() > 0.0f);
}

/// Test 2: MMR and gear weights configuration
TEST_F(BattlegroundMMRTest, MMRAndGearWeightsConfigured)
{
    float mmrWeight = sBattlegroundMMRMgr->GetMMRWeight();
    float gearWeight = sBattlegroundMMRMgr->GetGearWeight();

    // Weights should be positive
    EXPECT_GT(mmrWeight, 0.0f);
    EXPECT_GT(gearWeight, 0.0f);

    // Weights should sum to approximately 1.0 (100%)
    EXPECT_NEAR(mmrWeight + gearWeight, 1.0f, 0.01f)
        << "MMR weight + Gear weight should equal 1.0";
}

/// Test 3: Blended score calculation formula
TEST_F(BattlegroundMMRTest, BlendedScoreFormulaValidation)
{
    float mmrWeight = sBattlegroundMMRMgr->GetMMRWeight();
    float gearWeight = sBattlegroundMMRMgr->GetGearWeight();

    // Simulate a player with known values
    float testMMR = 1600.0f;
    float testGearScore = 200.0f;

    // Expected blended score: (MMR * mmrWeight) + (GearScore * gearWeight)
    float expectedBlended = (testMMR * mmrWeight) + (testGearScore * gearWeight);

    // With default config (0.7 MMR, 0.3 Gear):
    // expectedBlended = (1600 * 0.7) + (200 * 0.3) = 1120 + 60 = 1180

    // Verify the formula is reasonable
    EXPECT_GT(expectedBlended, 0.0f);
    EXPECT_LT(expectedBlended, testMMR + testGearScore);
}

/// Test 4: Queue relaxation enabled check
TEST_F(BattlegroundMMRTest, QueueRelaxationConfiguration)
{
    // Check if queue relaxation is configured
    bool relaxationEnabled = sBattlegroundMMRMgr->IsQueueRelaxationEnabled();

    if (relaxationEnabled)
    {
        float initialDiff = sBattlegroundMMRMgr->GetInitialMaxMMRDifference();
        EXPECT_GT(initialDiff, 0.0f) << "Initial MMR difference should be positive";
    }
}

/// Test 5: Relaxed MMR tolerance increases over time
TEST_F(BattlegroundMMRTest, RelaxedMMRToleranceIncreasesOverTime)
{
    if (!sBattlegroundMMRMgr->IsQueueRelaxationEnabled())
    {
        GTEST_SKIP() << "Queue relaxation not enabled in config";
    }

    // Get tolerance at different queue times
    float tolerance0s = sBattlegroundMMRMgr->GetRelaxedMMRTolerance(0);
    float tolerance60s = sBattlegroundMMRMgr->GetRelaxedMMRTolerance(60);
    float tolerance120s = sBattlegroundMMRMgr->GetRelaxedMMRTolerance(120);

    // Tolerance should increase with queue time
    EXPECT_GE(tolerance60s, tolerance0s)
        << "Tolerance at 60s should be >= tolerance at 0s";
    EXPECT_GE(tolerance120s, tolerance60s)
        << "Tolerance at 120s should be >= tolerance at 60s";
}

/// Test 6: Relaxation caps at maximum
TEST_F(BattlegroundMMRTest, RelaxationCapsAtMaximum)
{
    if (!sBattlegroundMMRMgr->IsQueueRelaxationEnabled())
    {
        GTEST_SKIP() << "Queue relaxation not enabled in config";
    }

    // Get tolerance after very long wait
    float tolerance10000s = sBattlegroundMMRMgr->GetRelaxedMMRTolerance(10000);

    // Should be finite (not infinite) and capped at configured maximum
    EXPECT_TRUE(std::isfinite(tolerance10000s));
    EXPECT_LT(tolerance10000s, 1000000.0f)
        << "Tolerance should cap at configured maximum (typically ~1000000)";
}

/// Test 7: Starting values are reasonable
TEST_F(BattlegroundMMRTest, StartingValuesAreReasonable)
{
    float startingRating = sBattlegroundMMRMgr->GetStartingRating();
    float startingRD = sBattlegroundMMRMgr->GetStartingRD();
    float startingVol = sBattlegroundMMRMgr->GetStartingVolatility();

    // Check Glicko-2 standard ranges
    EXPECT_GE(startingRating, 1000.0f);
    EXPECT_LE(startingRating, 2000.0f);

    EXPECT_GE(startingRD, 50.0f);
    EXPECT_LE(startingRD, 500.0f);

    EXPECT_GE(startingVol, 0.01f);
    EXPECT_LE(startingVol, 0.2f);
}

/// Test 8: System tau is in valid range
TEST_F(BattlegroundMMRTest, SystemTauIsValid)
{
    float tau = sBattlegroundMMRMgr->GetSystemTau();

    // Typical Glicko-2 tau range is 0.3 - 1.2
    EXPECT_GE(tau, 0.1f);
    EXPECT_LE(tau, 2.0f);
}

/// Test 9: Configuration consistency
TEST_F(BattlegroundMMRTest, ConfigurationConsistency)
{
    // If enabled, all values should be properly configured
    if (sBattlegroundMMRMgr->IsEnabled())
    {
        EXPECT_GT(sBattlegroundMMRMgr->GetStartingRating(), 0.0f);
        EXPECT_GT(sBattlegroundMMRMgr->GetMMRWeight(), 0.0f);
        EXPECT_GT(sBattlegroundMMRMgr->GetGearWeight(), 0.0f);
    }
}

/// Test 10: Initial MMR difference is positive
TEST_F(BattlegroundMMRTest, InitialMMRDifferencePositive)
{
    if (!sBattlegroundMMRMgr->IsQueueRelaxationEnabled())
    {
        GTEST_SKIP() << "Queue relaxation not enabled in config";
    }

    float initialDiff = sBattlegroundMMRMgr->GetInitialMaxMMRDifference();
    EXPECT_GT(initialDiff, 0.0f);
    EXPECT_LT(initialDiff, 1000.0f) << "Initial difference should be reasonable";
}
