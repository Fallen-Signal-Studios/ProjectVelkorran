// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
namespace SovProjectileDefense
{
enum class EOutcome { Explode, Absorb, Reflect };
inline EOutcome Resolve(bool Committed, bool Perfect, bool Deflected, bool Guarded, unsigned Reflections, unsigned Maximum)
{
    if (!Committed || !Perfect) { return EOutcome::Explode; }
    if (Deflected) { return Reflections < Maximum ? EOutcome::Reflect : EOutcome::Absorb; }
    return Guarded ? EOutcome::Absorb : EOutcome::Explode;
}
}
