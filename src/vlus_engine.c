#include "global.h"
#include "main.h"
#include "string_util.h"
#include "rogue.h"
#include "rogue_controller.h"
#include "vlus_engine.h"

void Vlus_PreRivalBattle(const u8 *matchSeed, u8 primeCharId) {
    struct VLUS_MatchBlueprint blueprint;
    u16 victoryPoints;
    
    if (primeCharId >= VLUS_PRIME_CHAR_COUNT) return;
    
    victoryPoints = gRogueSaveBlock->vlusSeason.primeCharVictoryPoints[primeCharId];
    
    // Generate the deterministic blueprint
    VLUS_GenerateMatchBlueprint(&blueprint, matchSeed, 
        primeCharId, victoryPoints);
    
    // Store in run data for the battle system to use
    gRogueRun.vlusMatchSeedHash = blueprint.matchSeedHash;
    gRogueRun.vlusPrimeCharVictoryPoints[primeCharId] = victoryPoints;
    gRogueRun.vlusRivalEncounterCount++;
    
    // Reserve a dynamic trainer slot for the rival
    // (the actual party gets generated in Task 2c)
}
u8 Vlus_CreateDeterministicParty(u16 trainerNum, struct Pokemon* party, u8 monCapacity) {
    struct VLUS_MatchBlueprint blueprint;
    u8 partySize, i;
    
    VLUS_GenerateMatchBlueprint(&blueprint, 
        (const u8 *)"0xVLUS_RIVAL_SEED", // or derive from run state
        trainerNum,
        gRogueRun.vlusPrimeCharVictoryPoints[trainerNum % VLUS_PRIME_CHAR_COUNT]);
    
    partySize = blueprint.rosterSize;
    if (partySize > monCapacity) partySize = monCapacity;
    
    for (i = 0; i < partySize; i++) {
        // Use the deterministic RNG to pick species, moves, EVs
        u32 speciesRng = Vlus_RandomFromState(gRogueRun.vlusMatchSeedHash + i);
        u16 species = Vlus_GetRosterSpecies(speciesRng, blueprint.rank, i);
        
        CreateMon(&party[i], species, 
            Vlus_CalcMonLevel(blueprint.rank, i),
            32,  // fixed IVs for competitive
            TRUE, 0, 0, 0);
        
        // Apply AI flags to the party data
        // (stored in mon data or battle flags)
    }
    
    gRogueRun.vlusMatchSeedHash = blueprint.matchSeedHash;
    return partySize;
}
// Buffer for dynamic broadcast commentary
// GBA text engine uses gStringVar1-4, but we can use custom buffers
static u8 sVlusBroadcastBuffer[256];
static u8 sVlusTauntBuffer[128];

// Generate broadcast commentary text
const u8 *Vlus_GenerateBroadcastText(const struct VLUS_MatchBlueprint *blueprint) {
    const u8 *rankStr, *sponsorStr, *reputationStr;
    
    switch (blueprint->rank) {
        case RANK_RISING_STAR:     rankStr = _("RISING STAR"); break;
        case RANK_ACE_TIER:        rankStr = _("ACE TIER"); break;
        case RANK_REGIONAL_MASTER: rankStr = _("REGIONAL MASTER"); break;
        case RANK_GRAND_CHAMPION:  rankStr = _("GRAND CHAMPION"); break;
        default:                   rankStr = _("UNRANKED"); break;
    }
    
    sponsorStr = sCorporateSponsors[blueprint->sponsorId].name;
    reputationStr = blueprint->reputationTag;
    
    // Build commentary into buffer
    StringCopy(sVlusBroadcastBuffer, 
        _("BROADCAST DESK: Welcome to the Victory League!\n"));
    StringAppend(sVlusBroadcastBuffer, 
        _("Today's matchup features a "));
    StringAppend(sVlusBroadcastBuffer, rankStr);
    StringAppend(sVlusBroadcastBuffer, _(" contender,\n"));
    StringAppend(sVlusBroadcastBuffer, 
        _("proudly sponsored by "));
    StringAppend(sVlusBroadcastBuffer, sponsorStr);
    StringAppend(sVlusBroadcastBuffer, _(".\n"));
    StringAppend(sVlusBroadcastBuffer, 
        _("Our analysts have flagged this trainer as a "));
    StringAppend(sVlusBroadcastBuffer, reputationStr);
    StringAppend(sVlusBroadcastBuffer, _("."));
    
    return sVlusBroadcastBuffer;
}

// Generate pre-match taunt
const u8 *Vlus_GenerateTauntText(const struct VLUS_MatchBlueprint *blueprint) {
    StringCopy(sVlusTauntBuffer, 
        _("You think you can beat me? I'm backed by "));
    StringAppend(sVlusTauntBuffer, 
        sCorporateSponsors[blueprint->sponsorId].name);
    StringAppend(sVlusTauntBuffer, _("!\n"));
    StringAppend(sVlusTauntBuffer, 
        _("Let's see if you have what it takes."));
    
    return sVlusTauntBuffer;
}
// In src/vlus_engine.c

// The GBA text engine in pokeemerald uses a global buffer
// extern u8 gStringVar1[]; etc. are available as 256-byte buffers
// in src/string_util.c (or similar path). We can write directly:

extern u8 gStringVar1[];
extern u8 gStringVar2[];

void Vlus_PrepareBroadcastText(void) {
    struct VLUS_MatchBlueprint blueprint;
    
    // Reconstruct the blueprint from stored run data
    VLUS_GenerateMatchBlueprint(&blueprint,
        (const u8 *)"0xVLUS_CURRENT",
        gTrainerBattleOpponent_A,
        gRogueRun.vlusPrimeCharVictoryPoints[0]); // adjust as needed
    
    // Push broadcast into gStringVar1, taunt into gStringVar2
    const u8 *broadcast = Vlus_GenerateBroadcastText(&blueprint);
    const u8 *taunt = Vlus_GenerateTauntText(&blueprint);
    
    StringCopy(gStringVar1, broadcast);
    StringCopy(gStringVar2, taunt);
}
// In src/vlus_engine.c
void Vlus_StageMultiPageBroadcast(void) {
    // Stage 1: gStringVar1 = page 1
    // Stage 2: After player presses A, load gStringVar1 = page 2
    // This avoids malloc/free on the GBA
}
// Called at the end of each route/biome.
// Simulates that Wally, May, and Brendan have been battling off-screen.

void Vlus_AdvancePrimeCharacters(void) {
    u8 i;
    u16 vpGain;
    
    // Use the deterministic RNG seeded from the run
    sDeterministicState = gRogueRun.vlusMatchSeedHash ^ gRogueRun.currentRouteIndex;
    
    for (i = 0; i < VLUS_PRIME_CHAR_COUNT; i++) {
        // Each prime character has a chance to gain victory points.
        // Higher-ranked characters gain fewer points (catch-up mechanic).
        u16 currentVP = gRogueSaveBlock->vlusSeason.primeCharVictoryPoints[i];
        u8 rank = (currentVP >= 150) ? RANK_GRAND_CHAMPION :
                  (currentVP >= 100) ? RANK_REGIONAL_MASTER :
                  (currentVP >= 50)  ? RANK_ACE_TIER : RANK_RISING_STAR;
        
        // Rank determines chance and magnitude of VP gain
        switch (rank) {
            case RANK_RISING_STAR:
                // 80% chance, 3-8 VP gain
                if ((VLUS_Random() % 100) < 80) {
                    vpGain = 3 + (VLUS_Random() % 6);
                }
                break;
            case RANK_ACE_TIER:
                // 65% chance, 2-6 VP gain
                if ((VLUS_Random() % 100) < 65) {
                    vpGain = 2 + (VLUS_Random() % 5);
                }
                break;
            case RANK_REGIONAL_MASTER:
                // 50% chance, 1-4 VP gain
                if ((VLUS_Random() % 100) < 50) {
                    vpGain = 1 + (VLUS_Random() % 4);
                }
                break;
            case RANK_GRAND_CHAMPION:
                // 30% chance, 1-2 VP gain
                if ((VLUS_Random() % 100) < 30) {
                    vpGain = 1 + (VLUS_Random() % 2);
                }
                break;
        }
        
        // Cap at 255 (u16, but we use a ceiling for display)
        if (currentVP + vpGain > 255) {
            gRogueSaveBlock->vlusSeason.primeCharVictoryPoints[i] = 255;
        } else {
            gRogueSaveBlock->vlusSeason.primeCharVictoryPoints[i] += vpGain;
        }
        
        vpGain = 0; // reset for next iteration
    }
    
    // Update the run's cached copy
    for (i = 0; i < VLUS_PRIME_CHAR_COUNT; i++) {
        gRogueRun.vlusPrimeCharVictoryPoints[i] = 
            gRogueSaveBlock->vlusSeason.primeCharVictoryPoints[i];
    }
    
    // Advance route counter so next call uses fresh seed
    gRogueRun.currentRouteIndex++;
}
// Called when the player faces a rival to get the rival's current VP tier
u16 Vlus_GetPrimeCharVictoryPoints(u8 primeCharId) {
    if (primeCharId >= VLUS_PRIME_CHAR_COUNT) return 0;
    return gRogueSaveBlock->vlusSeason.primeCharVictoryPoints[primeCharId];
}
