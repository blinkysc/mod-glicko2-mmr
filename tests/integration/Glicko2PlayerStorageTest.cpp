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
#include "Glicko2PlayerStorage.h"

/// Test fixture for Glicko2 Player Storage tests (Battleground MMR)
/// Tests focus on cache operations and data consistency
class Glicko2PlayerStorageTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clear cache before each test
        sGlicko2Storage->ClearCache();

        // Create test player GUIDs
        player1Guid = ObjectGuid::Create<HighGuid::Player>(200001);
        player2Guid = ObjectGuid::Create<HighGuid::Player>(200002);
        player3Guid = ObjectGuid::Create<HighGuid::Player>(200003);
    }

    void TearDown() override
    {
        // Clean up cache after each test
        sGlicko2Storage->RemoveRating(player1Guid);
        sGlicko2Storage->RemoveRating(player2Guid);
        sGlicko2Storage->RemoveRating(player3Guid);
    }

    ObjectGuid player1Guid;
    ObjectGuid player2Guid;
    ObjectGuid player3Guid;
};

/// Test 1: Cache starts empty
TEST_F(Glicko2PlayerStorageTest, CacheStartsEmpty)
{
    size_t cacheSize = sGlicko2Storage->GetCacheSize();
    EXPECT_EQ(cacheSize, 0) << "Cache should be empty after clear";
}

/// Test 2: Set and get rating
TEST_F(Glicko2PlayerStorageTest, SetAndGetRating)
{
    // Create test rating
    BattlegroundRatingData testRating(1650.0f, 180.0f, 0.055f, 10, 6, 4);

    // Set rating
    sGlicko2Storage->SetRating(player1Guid, testRating);

    // Get rating back
    BattlegroundRatingData retrieved = sGlicko2Storage->GetRating(player1Guid);

    // Verify data matches
    EXPECT_FLOAT_EQ(retrieved.rating, 1650.0f);
    EXPECT_FLOAT_EQ(retrieved.ratingDeviation, 180.0f);
    EXPECT_FLOAT_EQ(retrieved.volatility, 0.055f);
    EXPECT_EQ(retrieved.matchesPlayed, 10);
    EXPECT_EQ(retrieved.wins, 6);
    EXPECT_EQ(retrieved.losses, 4);
}

/// Test 3: Multiple players are independent
TEST_F(Glicko2PlayerStorageTest, MultiplePlayersIndependent)
{
    // Set different ratings for different players
    BattlegroundRatingData rating1(1600.0f, 200.0f, 0.06f, 10, 6, 4);
    BattlegroundRatingData rating2(1800.0f, 150.0f, 0.055f, 50, 30, 20);
    BattlegroundRatingData rating3(1400.0f, 220.0f, 0.062f, 5, 2, 3);

    sGlicko2Storage->SetRating(player1Guid, rating1);
    sGlicko2Storage->SetRating(player2Guid, rating2);
    sGlicko2Storage->SetRating(player3Guid, rating3);

    // Verify each player has their own rating
    BattlegroundRatingData retrieved1 = sGlicko2Storage->GetRating(player1Guid);
    BattlegroundRatingData retrieved2 = sGlicko2Storage->GetRating(player2Guid);
    BattlegroundRatingData retrieved3 = sGlicko2Storage->GetRating(player3Guid);

    EXPECT_FLOAT_EQ(retrieved1.rating, 1600.0f);
    EXPECT_FLOAT_EQ(retrieved2.rating, 1800.0f);
    EXPECT_FLOAT_EQ(retrieved3.rating, 1400.0f);
}

/// Test 4: Has rating check
TEST_F(Glicko2PlayerStorageTest, HasRatingCheck)
{
    // Initially should not have rating
    EXPECT_FALSE(sGlicko2Storage->HasRating(player1Guid));

    // Set a rating
    BattlegroundRatingData testRating;
    sGlicko2Storage->SetRating(player1Guid, testRating);

    // Now should have rating
    EXPECT_TRUE(sGlicko2Storage->HasRating(player1Guid));
}

/// Test 5: Remove rating
TEST_F(Glicko2PlayerStorageTest, RemoveRating)
{
    // Set rating
    BattlegroundRatingData testRating;
    sGlicko2Storage->SetRating(player1Guid, testRating);

    // Verify it exists
    EXPECT_TRUE(sGlicko2Storage->HasRating(player1Guid));

    // Remove it
    sGlicko2Storage->RemoveRating(player1Guid);

    // Verify it's gone
    EXPECT_FALSE(sGlicko2Storage->HasRating(player1Guid));
}

/// Test 6: Cache size tracking
TEST_F(Glicko2PlayerStorageTest, CacheSizeTracking)
{
    EXPECT_EQ(sGlicko2Storage->GetCacheSize(), 0);

    // Add one rating
    BattlegroundRatingData testRating;
    sGlicko2Storage->SetRating(player1Guid, testRating);

    size_t size1 = sGlicko2Storage->GetCacheSize();
    EXPECT_GT(size1, 0) << "Cache size should increase";

    // Add another player
    sGlicko2Storage->SetRating(player2Guid, testRating);

    size_t size2 = sGlicko2Storage->GetCacheSize();
    EXPECT_GT(size2, size1) << "Cache size should increase with more players";

    // Add third player
    sGlicko2Storage->SetRating(player3Guid, testRating);

    size_t size3 = sGlicko2Storage->GetCacheSize();
    EXPECT_GT(size3, size2) << "Cache size should increase with more players";
}

/// Test 7: Default values on first access
TEST_F(Glicko2PlayerStorageTest, DefaultValuesOnFirstAccess)
{
    // Get rating without setting (should return defaults)
    BattlegroundRatingData retrieved = sGlicko2Storage->GetRating(player1Guid);

    // Should have default battleground MMR values
    EXPECT_FLOAT_EQ(retrieved.rating, 1500.0f);
    EXPECT_FLOAT_EQ(retrieved.ratingDeviation, 350.0f);  // Battleground uses 350
    EXPECT_FLOAT_EQ(retrieved.volatility, 0.06f);
    EXPECT_EQ(retrieved.matchesPlayed, 0);
    EXPECT_EQ(retrieved.wins, 0);
    EXPECT_EQ(retrieved.losses, 0);
    EXPECT_FALSE(retrieved.loaded);  // Not loaded from DB
}

/// Test 8: Update existing rating
TEST_F(Glicko2PlayerStorageTest, UpdateExistingRating)
{
    // Set initial rating
    BattlegroundRatingData initial(1600.0f, 200.0f, 0.06f, 5, 3, 2);
    sGlicko2Storage->SetRating(player1Guid, initial);

    // Update with new values
    BattlegroundRatingData updated(1650.0f, 190.0f, 0.058f, 6, 4, 2);
    sGlicko2Storage->SetRating(player1Guid, updated);

    // Retrieve and verify updated values
    BattlegroundRatingData retrieved = sGlicko2Storage->GetRating(player1Guid);

    EXPECT_FLOAT_EQ(retrieved.rating, 1650.0f);
    EXPECT_FLOAT_EQ(retrieved.ratingDeviation, 190.0f);
    EXPECT_EQ(retrieved.matchesPlayed, 6);
    EXPECT_EQ(retrieved.wins, 4);
}

/// Test 9: Clear cache removes all entries
TEST_F(Glicko2PlayerStorageTest, ClearCacheRemovesAllEntries)
{
    // Add multiple ratings
    BattlegroundRatingData testRating;
    sGlicko2Storage->SetRating(player1Guid, testRating);
    sGlicko2Storage->SetRating(player2Guid, testRating);
    sGlicko2Storage->SetRating(player3Guid, testRating);

    EXPECT_GT(sGlicko2Storage->GetCacheSize(), 0);

    // Clear cache
    sGlicko2Storage->ClearCache();

    // Verify empty
    EXPECT_EQ(sGlicko2Storage->GetCacheSize(), 0);
}

/// Test 10: Win/Loss statistics accumulation
TEST_F(Glicko2PlayerStorageTest, WinLossStatisticsAccumulation)
{
    // Start with initial stats
    BattlegroundRatingData data(1500.0f, 350.0f, 0.06f, 0, 0, 0);
    sGlicko2Storage->SetRating(player1Guid, data);

    // Simulate wins and losses
    for (int i = 0; i < 5; ++i)
    {
        BattlegroundRatingData current = sGlicko2Storage->GetRating(player1Guid);
        current.matchesPlayed++;
        current.wins++;
        sGlicko2Storage->SetRating(player1Guid, current);
    }

    for (int i = 0; i < 3; ++i)
    {
        BattlegroundRatingData current = sGlicko2Storage->GetRating(player1Guid);
        current.matchesPlayed++;
        current.losses++;
        sGlicko2Storage->SetRating(player1Guid, current);
    }

    // Verify final stats
    BattlegroundRatingData final = sGlicko2Storage->GetRating(player1Guid);
    EXPECT_EQ(final.matchesPlayed, 8);
    EXPECT_EQ(final.wins, 5);
    EXPECT_EQ(final.losses, 3);
}

/// Test 11: Loaded flag tracking
TEST_F(Glicko2PlayerStorageTest, LoadedFlagTracking)
{
    // Default rating should not be marked as loaded
    BattlegroundRatingData defaultRating = sGlicko2Storage->GetRating(player1Guid);
    EXPECT_FALSE(defaultRating.loaded);

    // Explicitly set rating with loaded flag
    BattlegroundRatingData loadedRating(1600.0f, 200.0f, 0.06f, 10, 6, 4);
    loadedRating.loaded = true;
    sGlicko2Storage->SetRating(player1Guid, loadedRating);

    BattlegroundRatingData retrieved = sGlicko2Storage->GetRating(player1Guid);
    EXPECT_TRUE(retrieved.loaded);
}

/// Test 12: Rating values stay within reasonable bounds
TEST_F(Glicko2PlayerStorageTest, RatingValuesReasonableBounds)
{
    // Set extreme values
    BattlegroundRatingData extremeRating(2500.0f, 50.0f, 0.1f, 1000, 800, 200);
    sGlicko2Storage->SetRating(player1Guid, extremeRating);

    BattlegroundRatingData retrieved = sGlicko2Storage->GetRating(player1Guid);

    // Verify values are stored correctly (no clamping)
    EXPECT_FLOAT_EQ(retrieved.rating, 2500.0f);
    EXPECT_FLOAT_EQ(retrieved.ratingDeviation, 50.0f);
    EXPECT_FLOAT_EQ(retrieved.volatility, 0.1f);
    EXPECT_EQ(retrieved.matchesPlayed, 1000);
}
