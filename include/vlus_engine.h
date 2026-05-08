// Add after existing definitions in vlus_engine.h

#include "global.h"

#define VLUS_PRIME_CHAR_COUNT 3

enum VlusPrimeChar {
    VLUS_PRIME_WALLY = 0,
    VLUS_PRIME_MAY,
    VLUS_PRIME_BRENDAN,
};

// Persistent across runs — goes in RogueSaveBlock
struct VlusSeasonData {
    u32 seasonSeed;           // Master seed for the whole season
    u16 primeCharVictoryPoints[VLUS_PRIME_CHAR_COUNT]; // Wally, May, Brendan
    u8 seasonTier;            // 0-3, maps to VLUS_Rank thresholds
    u8 pad;                   // alignment
};
// Add to vlus_engine.h:

// Called from Poryscript BEFORE a rival battle.
// Generates the match blueprint and stores it in gRogueRun.
void Vlus_PreRivalBattle(const u8 *matchSeed, u8 primeCharId);
