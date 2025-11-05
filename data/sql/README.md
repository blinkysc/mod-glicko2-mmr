# Glicko-2 MMR Module - SQL Installation

## Overview

This module requires database changes in the `acore_characters` database:
- Two new tables for battleground ratings
- Seven new columns added to the existing `character_arena_stats` table

## Installation

### Manual Installation

#### Step 1: Create Battleground Rating Tables

```bash
mysql -u acore -pacore acore_characters < db-characters/base/character_battleground_rating.sql
```

Or import via MySQL client:

```sql
USE acore_characters;
SOURCE /path/to/modules/mod-glicko2-mmr/data/sql/db-characters/base/character_battleground_rating.sql;
```

#### Step 2: Extend Arena Stats Table

```bash
mysql -u acore -pacore acore_characters < db-characters/updates/extend_character_arena_stats_glicko2.sql
```

Or via MySQL client:

```sql
USE acore_characters;
SOURCE /path/to/modules/mod-glicko2-mmr/data/sql/db-characters/updates/extend_character_arena_stats_glicko2.sql;
```

### Database Changes

#### Battleground Tables Created

1. **`character_battleground_rating`** - Stores current rating for each player
   - `guid` - Character GUID (primary key)
   - `rating` - Current Glicko-2 rating (default: 1500)
   - `rating_deviation` - Rating uncertainty (default: 350)
   - `volatility` - Rating volatility (default: 0.06)
   - `matches_played` - Total battleground matches
   - `matches_won` - Total wins
   - `matches_lost` - Total losses
   - `last_match_time` - Unix timestamp of last match

2. **`character_battleground_rating_history`** - Tracks rating changes over time
   - `id` - Auto-increment primary key
   - `guid` - Character GUID
   - `rating` - Rating after match
   - `rating_deviation` - RD after match
   - `volatility` - Volatility after match
   - `match_time` - Unix timestamp of match
   - `match_result` - 1=win, 0=loss

#### Arena Table Extended

The existing **`character_arena_stats`** table is extended with these new columns:
   - `rating` - Glicko-2 rating for this bracket (default: 1500)
   - `rating_deviation` - Rating uncertainty (default: 350)
   - `volatility` - Rating volatility (default: 0.06)
   - `matches_played` - Total matches in this bracket
   - `matches_won` - Total wins in this bracket
   - `matches_lost` - Total losses in this bracket
   - `last_match_time` - Unix timestamp of last match

Note: Arena ratings are stored per bracket (2v2, 3v3, 5v5) using the existing `slot` column.

## Verification

### Verify Battleground Tables

```sql
USE acore_characters;
SHOW TABLES LIKE 'character_battleground_rating%';
```

Expected output:
```
+--------------------------------------------------+
| Tables_in_acore_characters                       |
+--------------------------------------------------+
| character_battleground_rating                    |
| character_battleground_rating_history            |
+--------------------------------------------------+
```

### Verify Arena Columns

```sql
USE acore_characters;
SHOW COLUMNS FROM character_arena_stats LIKE '%rating%';
```

Expected output should include:
```
+-------------------+--------------+------+-----+---------+-------+
| Field             | Type         | Null | Key | Default | Extra |
+-------------------+--------------+------+-----+---------+-------+
| matchMakerRating  | smallint     | NO   |     | 0       |       |
| rating            | float        | NO   |     | 1500    |       |
| rating_deviation  | float        | NO   |     | 350     |       |
+-------------------+--------------+------+-----+---------+-------+
```

## Uninstallation

To completely remove the module's database changes:

### Step 1: Remove Battleground Tables

```sql
USE acore_characters;
DROP TABLE IF EXISTS `character_battleground_rating_history`;
DROP TABLE IF EXISTS `character_battleground_rating`;
```

### Step 2: Remove Arena Glicko-2 Columns

```sql
USE acore_characters;
ALTER TABLE `character_arena_stats`
  DROP COLUMN IF EXISTS `last_match_time`,
  DROP COLUMN IF EXISTS `matches_lost`,
  DROP COLUMN IF EXISTS `matches_won`,
  DROP COLUMN IF EXISTS `matches_played`,
  DROP COLUMN IF EXISTS `volatility`,
  DROP COLUMN IF EXISTS `rating_deviation`,
  DROP COLUMN IF EXISTS `rating`;
```

**Warning**: This will permanently delete all battleground and arena Glicko-2 rating data! The original arena MMR data (matchMakerRating, maxMMR) will remain intact.

## Maintenance

### Clear All Rating Data

#### Clear Battleground Ratings Only

```sql
USE acore_characters;
TRUNCATE TABLE `character_battleground_rating_history`;
TRUNCATE TABLE `character_battleground_rating`;
```

#### Clear Arena Glicko-2 Data Only

```sql
USE acore_characters;
UPDATE `character_arena_stats` SET
  rating = 1500,
  rating_deviation = 350,
  volatility = 0.06,
  matches_played = 0,
  matches_won = 0,
  matches_lost = 0,
  last_match_time = 0;
```

Note: This resets Glicko-2 data but preserves the original arena MMR (matchMakerRating, maxMMR).

### View Top Players

#### Battleground Leaderboard

```sql
USE acore_characters;
SELECT
    guid,
    rating,
    matches_played,
    matches_won,
    matches_lost,
    ROUND((matches_won / matches_played * 100), 1) AS win_rate_pct
FROM character_battleground_rating
WHERE matches_played > 0
ORDER BY rating DESC
LIMIT 10;
```

#### Arena Leaderboard (by bracket)

```sql
USE acore_characters;
-- slot 0 = 2v2, slot 1 = 3v3, slot 2 = 5v5
SELECT
    guid,
    slot,
    CASE slot
        WHEN 0 THEN '2v2'
        WHEN 1 THEN '3v3'
        WHEN 2 THEN '5v5'
    END AS bracket,
    rating,
    matches_played,
    matches_won,
    matches_lost,
    ROUND((matches_won / matches_played * 100), 1) AS win_rate_pct
FROM character_arena_stats
WHERE matches_played > 0 AND slot = 0  -- Change slot for different brackets
ORDER BY rating DESC
LIMIT 10;
```

### Rating Distribution

#### Battleground Rating Distribution

```sql
USE acore_characters;
SELECT
    FLOOR(rating / 100) * 100 AS rating_bracket,
    COUNT(*) AS player_count
FROM character_battleground_rating
WHERE matches_played > 0
GROUP BY rating_bracket
ORDER BY rating_bracket;
```

#### Arena Rating Distribution (all brackets)

```sql
USE acore_characters;
SELECT
    CASE slot
        WHEN 0 THEN '2v2'
        WHEN 1 THEN '3v3'
        WHEN 2 THEN '5v5'
    END AS bracket,
    FLOOR(rating / 100) * 100 AS rating_bracket,
    COUNT(*) AS player_count
FROM character_arena_stats
WHERE matches_played > 0
GROUP BY slot, rating_bracket
ORDER BY slot, rating_bracket;
```
