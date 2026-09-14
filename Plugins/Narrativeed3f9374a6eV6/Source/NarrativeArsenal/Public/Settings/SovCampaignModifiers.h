#pragma once
#include "CoreMinimal.h"

/** Independent optional campaign challenges, layered after difficulty. Stable save bits. */
namespace SovCampaignModifiers
{
    constexpr uint8 Blackout = 1 << 0;
    constexpr uint8 Famine = 1 << 1;
    constexpr uint8 Frenzy = 1 << 2;
    constexpr uint8 Ascendant = 1 << 3;
    constexpr uint8 GlassCannon = 1 << 4;
    constexpr uint8 All = Blackout | Famine | Frenzy | Ascendant | GlassCannon;
    inline bool Has(uint8 Mask, uint8 Flag) { return (Mask & Flag) != 0; }
    inline int32 AttackTokens(uint8 Mask, int32 Base)
    { return Has(Mask, Frenzy) ? FMath::Clamp(Base + 2, 1, 8) : Base; }
    inline float AttackCooldown(uint8 Mask)
    { return Has(Mask, Frenzy) ? .65f : 1.f; }
    inline float BodyDamage(uint8 Mask, bool EnemyToAlly, bool AllyToEnemy, bool Fatal)
    {
        if (Fatal || (!EnemyToAlly && !AllyToEnemy)) { return 1.f; }
        float Scale = Has(Mask, GlassCannon) ? 2.f : 1.f;
        // Effective health/shield toughness: no maximum-health mutation or mid-fight healing.
        if (AllyToEnemy && Has(Mask, Ascendant)) { Scale /= 1.5f; }
        return Scale;
    }
}
