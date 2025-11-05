# Glicko-2 MMR System for AzerothCore

A skill-based matchmaking rating system for battlegrounds using the Glicko-2 algorithm.

## Features

- **Glicko-2 Rating Algorithm**: Industry-standard rating system with rating deviation and volatility
- **Automatic Rating Updates**: Ratings update automatically after battleground matches
- **MMR-Based Matchmaking**: Optional queue-based matchmaking with time-based relaxation
- **Persistent Storage**: Ratings stored in database and cached in memory for performance
- **GM Commands**: View and manage player ratings with `.bgmmr` commands
- **Configurable**: Adjust initial ratings, volatility, matchmaking ranges, and relaxation rates

## Installation

1. Clone this repository into your AzerothCore `modules` directory:
```bash
cd /path/to/azerothcore/modules
git clone https://github.com/yourusername/mod-glicko2-mmr.git
```

2. Re-run CMake and rebuild:
```bash
cd /path/to/azerothcore/build
cmake ../ -DCMAKE_INSTALL_PREFIX=$HOME/azerothcore/env/dist/
cmake --build . --target install -j$(nproc)
```

3. Import the SQL files:
```bash
# The module will automatically create the required database tables on first run
```

4. Copy and configure the module config:
```bash
cp /path/to/dist/etc/modules/mod_glicko2_mmr.conf.dist /path/to/dist/etc/modules/mod_glicko2_mmr.conf
```

5. Restart your worldserver

## Configuration

Edit `mod_glicko2_mmr.conf`:

### Battleground Rating System

- `BattleGround.MMR.Enable` - Enable/disable the BG MMR system (default: 0)
- `BattleGround.MMR.StartingRating` - Starting rating for new players (default: 1500.0)
- `BattleGround.MMR.StartingRD` - Starting rating deviation (default: 200.0)
- `BattleGround.MMR.StartingVolatility` - Starting volatility (default: 0.06)
- `BattleGround.MMR.SystemConstant` - System volatility constraint (default: 0.5)

### Arena Rating System

- `Glicko2.Arena.Enabled` - Enable/disable the arena MMR system (default: 0)
- `Glicko2.Arena.InitialRating` - Starting rating for new arena players (default: 1500.0)
- `Glicko2.Arena.InitialRatingDeviation` - Starting uncertainty (default: 350.0)
- `Glicko2.Arena.InitialVolatility` - Starting consistency measure (default: 0.06)
- `Glicko2.Arena.Tau` - System volatility constraint (default: 0.5)

### Battleground Queue Relaxation

- `BattleGround.MMR.QueueRelaxation.Enable` - Enable MMR range relaxation over time (default: 1)
- `BattleGround.MMR.QueueRelaxation.InitialTolerance` - Initial MMR range (default: 200.0)
- `BattleGround.MMR.QueueRelaxation.IntervalSeconds` - How often to expand range (default: 120)
- `BattleGround.MMR.QueueRelaxation.StepMMR` - Increase per interval (default: 100.0)
- `BattleGround.MMR.QueueRelaxation.MaxSeconds` - Max time before accepting any MMR (default: 600)

### Arena Matchmaking (Per Bracket)

Separate configuration for 2v2, 3v3, and 5v5 brackets:
- `Glicko2.Arena.{2v2|3v3|5v5}.Matchmaking.InitialRange` - Initial MMR range
- `Glicko2.Arena.{2v2|3v3|5v5}.Matchmaking.MaxRange` - Maximum MMR range
- `Glicko2.Arena.{2v2|3v3|5v5}.Matchmaking.RelaxationRate` - Increase per 30 seconds

## GM Commands

- `.bgmmr info [player]` - Display rating information for a player
- `.bgmmr set <player> <rating>` - Set a player's rating (requires SEC_ADMINISTRATOR)
- `.bgmmr reset [player]` - Reset a player's rating to default (requires SEC_ADMINISTRATOR)

## How It Works

### Rating System

The module uses the **Glicko-2** rating algorithm developed by Professor Mark Glickman. Each player has three values:

1. **Rating (r)**: Skill level (default: 1500, higher = better)
2. **Rating Deviation (RD)**: Uncertainty about the rating (decreases with more games)
3. **Volatility (σ)**: Consistency of performance

### Rating Updates

- Ratings update automatically when battlegrounds end
- New players start with high RD (350), causing larger initial rating swings
- After 5-10 games, RD decreases and rating changes become smaller
- System accounts for opponent strength and rating uncertainty

### Expected Rating Changes

**New Players (RD ≈ 350)**:
- First few matches: ±100-200 rating points
- Large swings are normal due to high uncertainty

**Experienced Players (RD ≈ 100-150)**:
- Typical match: ±20-40 rating points
- Changes stabilize as the system becomes confident in skill level

### Matchmaking System

When matchmaking is enabled (requires core hooks), players are matched based on their MMR with time-based relaxation:

**Initial Matching**:
- Players join queue and their MMR is calculated
- Initial MMR range: ±200 rating points (configurable)
- Only players within this range can be matched together initially

**Time-Based Relaxation**:
- For every second in queue, the acceptable MMR range expands
- Default expansion rate: 10 rating points per second
- Maximum range: 1000 rating points (configurable)
- Ensures matches form even with limited players

**Example**:
- Player with 1500 MMR joins queue
- 0 seconds: Will match with 1300-1700 MMR players
- 30 seconds: Will match with 1000-2000 MMR players
- 60 seconds: Will match with 700-2300 MMR players (capped at max range)

This system balances match quality with reasonable queue times.

## Database Schema

The module creates two tables in the `acore_characters` database:

### `character_battleground_rating`
Stores current rating data for each player.

### `character_battleground_rating_history`
Logs rating changes over time (currently unused, reserved for future features).

## Technical Details

- **Thread-Safe**: Uses `std::shared_mutex` for concurrent read/write access
- **Cached**: Ratings loaded on login, cached in memory, saved on logout
- **Efficient**: No database queries during active gameplay
- **Module-Based**: Completely separate from core, easy to enable/disable

## Algorithm Credits

### Glicko-2 Rating System
- **Author**: Professor Mark E. Glickman
- **Website**: http://www.glicko.net/glicko.html
- **Paper**: "Example of the Glicko-2 system" (March 22, 2022)
- **PDF**: http://www.glicko.net/glicko/glicko2.pdf
- **License**: Public Domain - The Glicko-2 algorithm is freely available for use

The Glicko-2 system is an improvement over the Elo rating system, providing:
- Rating Deviation (RD) to measure uncertainty
- Volatility to measure performance consistency
- Better handling of inactive players
- More accurate rating adjustments

### Illinois Algorithm
- **Purpose**: Used for finding optimal volatility in Glicko-2 calculations
- **Method**: Illinois variant of the regula falsi method
- **Advantage**: More numerically stable than Newton-Raphson iteration
- **Implementation**: Based on Glicko-2 specification section 5.5

The Illinois algorithm is a root-finding method that improves convergence stability when calculating the new volatility parameter in the Glicko-2 update step.

### Reference Implementations Consulted
- **glicko2 Python library** by Heungsub Lee (https://github.com/sublee/glicko2)
  - Copyright (c) 2012-2016, Heungsub Lee
  - License: BSD 3-Clause License

This C++ implementation is an original work for AzerothCore, adapted from the official Glicko-2 specification.

## License

Released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3

## Platform

Built for AzerothCore - https://www.azerothcore.org/
