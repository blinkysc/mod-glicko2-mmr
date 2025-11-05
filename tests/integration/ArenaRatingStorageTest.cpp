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
#include "ArenaRatingStorage.h"

/// Test fixture for Arena Rating Storage tests
/// Tests focus on cache operations and data consistency
class ArenaRatingStorageTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clear cache before each test
        sArenaRatingStorage->ClearCache();

        // Create test player GUIDs
        player1Guid = ObjectGuid::Create<HighGuid::Player>(100001);
        player2Guid = ObjectGuid::Create<HighGuid::Player>(100002);
        player3Guid = ObjectGuid::Create<HighGuid::Player>(100003);
    }

    void TearDown() override
    {
        // Clean up cache after each test
        sArenaRatingStorage->RemoveAllRatings(player1Guid);
        sArenaRatingStorage->RemoveAllRatings(player2Guid);
        sArenaRatingStorage->RemoveAllRatings(player3Guid);
    }

    ObjectGuid player1Guid;
    ObjectGuid player2Guid;
    ObjectGuid player3Guid;
};

/// Test 1: Cache starts empty
TEST_F(ArenaRatingStorageTest, CacheStartsEmpty)
{
    size_t cacheSize = sArenaRatingStorage->GetCacheSize();
    EXPECT_EQ(cacheSize, 0) << "Cache should be empty after clear";
}

/// Test 2: Set and get rating
TEST_F(ArenaRatingStorageTest, SetAndGetRating)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Create test rating
    ArenaRatingData testData(1650.0f, 150.0f, 0.055f, 10, 6, 4, bracket);

    // Set rating
    sArenaRatingStorage->SetRating(player1Guid, bracket, testData);

    // Get rating back
    ArenaRatingData retrieved = sArenaRatingStorage->GetRating(player1Guid, bracket);

    // Verify data matches
    EXPECT_FLOAT_EQ(retrieved.rating, 1650.0f);
    EXPECT_FLOAT_EQ(retrieved.ratingDeviation, 150.0f);
    EXPECT_FLOAT_EQ(retrieved.volatility, 0.055f);
    EXPECT_EQ(retrieved.matchesPlayed, 10);
    EXPECT_EQ(retrieved.wins, 6);
    EXPECT_EQ(retrieved.losses, 4);
}

/// Test 3: Has rating check
TEST_F(ArenaRatingStorageTest, HasRatingCheck)
{
    ArenaBracket bracket = ArenaBracket::SLOT_3v3;

    // Initially should not have rating
    EXPECT_FALSE(sArenaRatingStorage->HasRating(player1Guid, bracket));

    // Set a rating
    ArenaRatingData testData;
    sArenaRatingStorage->SetRating(player1Guid, bracket, testData);

    // Now should have rating
    EXPECT_TRUE(sArenaRatingStorage->HasRating(player1Guid, bracket));
}

/// Test 4: Separate brackets are independent
TEST_F(ArenaRatingStorageTest, SeparateBracketsIndependent)
{
    // Set different ratings for each bracket
    ArenaRatingData data2v2(1600.0f, 200.0f, 0.06f, 5, 3, 2, ArenaBracket::SLOT_2v2);
    ArenaRatingData data3v3(1700.0f, 180.0f, 0.055f, 10, 7, 3, ArenaBracket::SLOT_3v3);
    ArenaRatingData data5v5(1550.0f, 220.0f, 0.062f, 3, 1, 2, ArenaBracket::SLOT_5v5);

    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_2v2, data2v2);
    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_3v3, data3v3);
    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_5v5, data5v5);

    // Retrieve and verify each is independent
    ArenaRatingData retrieved2v2 = sArenaRatingStorage->GetRating(player1Guid, ArenaBracket::SLOT_2v2);
    ArenaRatingData retrieved3v3 = sArenaRatingStorage->GetRating(player1Guid, ArenaBracket::SLOT_3v3);
    ArenaRatingData retrieved5v5 = sArenaRatingStorage->GetRating(player1Guid, ArenaBracket::SLOT_5v5);

    EXPECT_FLOAT_EQ(retrieved2v2.rating, 1600.0f);
    EXPECT_FLOAT_EQ(retrieved3v3.rating, 1700.0f);
    EXPECT_FLOAT_EQ(retrieved5v5.rating, 1550.0f);
}

/// Test 5: Multiple players are independent
TEST_F(ArenaRatingStorageTest, MultiplePlayersIndependent)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Set different ratings for different players
    ArenaRatingData data1(1600.0f, 200.0f, 0.06f, 10, 6, 4, bracket);
    ArenaRatingData data2(1800.0f, 150.0f, 0.055f, 50, 30, 20, bracket);

    sArenaRatingStorage->SetRating(player1Guid, bracket, data1);
    sArenaRatingStorage->SetRating(player2Guid, bracket, data2);

    // Verify each player has their own rating
    ArenaRatingData retrieved1 = sArenaRatingStorage->GetRating(player1Guid, bracket);
    ArenaRatingData retrieved2 = sArenaRatingStorage->GetRating(player2Guid, bracket);

    EXPECT_FLOAT_EQ(retrieved1.rating, 1600.0f);
    EXPECT_FLOAT_EQ(retrieved2.rating, 1800.0f);
}

/// Test 6: Remove rating from specific bracket
TEST_F(ArenaRatingStorageTest, RemoveRatingFromBracket)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Set rating
    ArenaRatingData testData;
    sArenaRatingStorage->SetRating(player1Guid, bracket, testData);

    // Verify it exists
    EXPECT_TRUE(sArenaRatingStorage->HasRating(player1Guid, bracket));

    // Remove it
    sArenaRatingStorage->RemoveRating(player1Guid, bracket);

    // Verify it's gone
    EXPECT_FALSE(sArenaRatingStorage->HasRating(player1Guid, bracket));
}

/// Test 7: Remove all ratings for a player
TEST_F(ArenaRatingStorageTest, RemoveAllRatingsForPlayer)
{
    // Set ratings in all brackets
    ArenaRatingData testData;
    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_2v2, testData);
    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_3v3, testData);
    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_5v5, testData);

    // Verify all exist
    EXPECT_TRUE(sArenaRatingStorage->HasRating(player1Guid, ArenaBracket::SLOT_2v2));
    EXPECT_TRUE(sArenaRatingStorage->HasRating(player1Guid, ArenaBracket::SLOT_3v3));
    EXPECT_TRUE(sArenaRatingStorage->HasRating(player1Guid, ArenaBracket::SLOT_5v5));

    // Remove all
    sArenaRatingStorage->RemoveAllRatings(player1Guid);

    // Verify all gone
    EXPECT_FALSE(sArenaRatingStorage->HasRating(player1Guid, ArenaBracket::SLOT_2v2));
    EXPECT_FALSE(sArenaRatingStorage->HasRating(player1Guid, ArenaBracket::SLOT_3v3));
    EXPECT_FALSE(sArenaRatingStorage->HasRating(player1Guid, ArenaBracket::SLOT_5v5));
}

/// Test 8: Cache size tracking
TEST_F(ArenaRatingStorageTest, CacheSizeTracking)
{
    EXPECT_EQ(sArenaRatingStorage->GetCacheSize(), 0);

    // Add one rating
    ArenaRatingData testData;
    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_2v2, testData);

    size_t size1 = sArenaRatingStorage->GetCacheSize();
    EXPECT_GT(size1, 0) << "Cache size should increase";

    // Add another bracket for same player
    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_3v3, testData);

    size_t size2 = sArenaRatingStorage->GetCacheSize();
    EXPECT_GT(size2, size1) << "Cache size should increase with more ratings";

    // Add different player
    sArenaRatingStorage->SetRating(player2Guid, ArenaBracket::SLOT_2v2, testData);

    size_t size3 = sArenaRatingStorage->GetCacheSize();
    EXPECT_GT(size3, size2) << "Cache size should increase with more players";
}

/// Test 9: Default values on first access
TEST_F(ArenaRatingStorageTest, DefaultValuesOnFirstAccess)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Get rating without setting (should return defaults)
    ArenaRatingData retrieved = sArenaRatingStorage->GetRating(player1Guid, bracket);

    // Should have default Glicko-2 values
    EXPECT_FLOAT_EQ(retrieved.rating, 1500.0f);
    EXPECT_FLOAT_EQ(retrieved.ratingDeviation, 350.0f);
    EXPECT_FLOAT_EQ(retrieved.volatility, 0.06f);
    EXPECT_EQ(retrieved.matchesPlayed, 0);
    EXPECT_EQ(retrieved.wins, 0);
    EXPECT_EQ(retrieved.losses, 0);
}

/// Test 10: Update existing rating
TEST_F(ArenaRatingStorageTest, UpdateExistingRating)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Set initial rating
    ArenaRatingData initial(1600.0f, 200.0f, 0.06f, 5, 3, 2, bracket);
    sArenaRatingStorage->SetRating(player1Guid, bracket, initial);

    // Update with new values
    ArenaRatingData updated(1650.0f, 190.0f, 0.058f, 6, 4, 2, bracket);
    sArenaRatingStorage->SetRating(player1Guid, bracket, updated);

    // Retrieve and verify updated values
    ArenaRatingData retrieved = sArenaRatingStorage->GetRating(player1Guid, bracket);

    EXPECT_FLOAT_EQ(retrieved.rating, 1650.0f);
    EXPECT_FLOAT_EQ(retrieved.ratingDeviation, 190.0f);
    EXPECT_EQ(retrieved.matchesPlayed, 6);
    EXPECT_EQ(retrieved.wins, 4);
}

/// Test 11: Clear cache removes all entries
TEST_F(ArenaRatingStorageTest, ClearCacheRemovesAllEntries)
{
    // Add multiple ratings
    ArenaRatingData testData;
    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_2v2, testData);
    sArenaRatingStorage->SetRating(player1Guid, ArenaBracket::SLOT_3v3, testData);
    sArenaRatingStorage->SetRating(player2Guid, ArenaBracket::SLOT_2v2, testData);

    EXPECT_GT(sArenaRatingStorage->GetCacheSize(), 0);

    // Clear cache
    sArenaRatingStorage->ClearCache();

    // Verify empty
    EXPECT_EQ(sArenaRatingStorage->GetCacheSize(), 0);
}

/// Test 12: Win/Loss statistics accumulation
TEST_F(ArenaRatingStorageTest, WinLossStatisticsAccumulation)
{
    ArenaBracket bracket = ArenaBracket::SLOT_2v2;

    // Start with initial stats
    ArenaRatingData data(1500.0f, 200.0f, 0.06f, 0, 0, 0, bracket);
    sArenaRatingStorage->SetRating(player1Guid, bracket, data);

    // Simulate wins and losses
    for (int i = 0; i < 5; ++i)
    {
        ArenaRatingData current = sArenaRatingStorage->GetRating(player1Guid, bracket);
        current.matchesPlayed++;
        current.wins++;
        sArenaRatingStorage->SetRating(player1Guid, bracket, current);
    }

    for (int i = 0; i < 3; ++i)
    {
        ArenaRatingData current = sArenaRatingStorage->GetRating(player1Guid, bracket);
        current.matchesPlayed++;
        current.losses++;
        sArenaRatingStorage->SetRating(player1Guid, bracket, current);
    }

    // Verify final stats
    ArenaRatingData final = sArenaRatingStorage->GetRating(player1Guid, bracket);
    EXPECT_EQ(final.matchesPlayed, 8);
    EXPECT_EQ(final.wins, 5);
    EXPECT_EQ(final.losses, 3);
}
