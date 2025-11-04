-- Glicko-2 MMR Module - Extend existing character_arena_stats table
-- Description: Adds Glicko-2 rating fields to the existing arena stats table

-- Add Glicko-2 rating components
ALTER TABLE `character_arena_stats`
  ADD COLUMN `rating` FLOAT NOT NULL DEFAULT 1500 COMMENT 'Glicko-2 rating' AFTER `maxMMR`,
  ADD COLUMN `rating_deviation` FLOAT NOT NULL DEFAULT 350 COMMENT 'Rating deviation (RD)' AFTER `rating`,
  ADD COLUMN `volatility` FLOAT NOT NULL DEFAULT 0.06 COMMENT 'Rating volatility' AFTER `rating_deviation`,
  ADD COLUMN `matches_played` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Total matches in this bracket' AFTER `volatility`,
  ADD COLUMN `matches_won` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Total wins in this bracket' AFTER `matches_played`,
  ADD COLUMN `matches_lost` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Total losses in this bracket' AFTER `matches_won`,
  ADD COLUMN `last_match_time` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp of last match' AFTER `matches_lost`;

-- Note: Existing slot values are:
-- slot 0 = 2v2 arena
-- slot 1 = 3v3 arena
-- slot 2 = 5v5 arena
