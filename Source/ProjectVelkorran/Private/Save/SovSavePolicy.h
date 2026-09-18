// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstdint>
#include <limits>

namespace SovSavePolicy
{
constexpr int ManualSlots = 10;
constexpr int AutoSlots = 3;
constexpr int SchemaMajor = 1;
constexpr int SchemaMinor = 0;
enum class Kind : std::uint8_t { Manual, Auto, Checkpoint };
enum class Compatibility : std::uint8_t { Compatible, Legacy, NewerSchema, DifferentProduct, WrongAccount };
inline bool ValidSlot(Kind Type, int Index)
{
    switch (Type)
    {
    case Kind::Manual: return Index >= 0 && Index < ManualSlots;
    case Kind::Auto: return Index >= 0 && Index < AutoSlots;
    case Kind::Checkpoint: return Index == 0;
    }
    return false;
}
inline Compatibility CheckVersion(int Major, int Minor, bool ProductMatches, bool AccountMatches)
{
    if (!ProductMatches) { return Compatibility::DifferentProduct; }
    if (!AccountMatches) { return Compatibility::WrongAccount; }
    if (Major < SchemaMajor || Minor < 0) { return Compatibility::Legacy; }
    if (Major > SchemaMajor || Minor > SchemaMinor) { return Compatibility::NewerSchema; }
    return Compatibility::Compatible; // Patch/build strings do not invalidate shipped schema 1.0.
}
struct Bank { bool Valid = false; std::int64_t Generation = 0; };
/**
 * Redirects a chosen write target away from a bank that has to be preserved.
 *
 * A reserved bank holds a save a newer build wrote. Its bytes are real progress, and this build
 * simply cannot read them, so treating it as spare space is silent loss the player only discovers
 * after upgrading again. When both banks are reserved one of them has to be given up to let the
 * player save at all; the further-along save is the one kept.
 */
inline int PreserveReserved(int Target, bool ReservedA, bool ReservedB,
    std::int64_t GenerationA, std::int64_t GenerationB)
{
    if (Target != 0 && Target != 1) { return Target; }
    if (!(Target == 0 ? ReservedA : ReservedB)) { return Target; }
    if (!(Target == 0 ? ReservedB : ReservedA)) { return 1 - Target; }
    return GenerationA <= GenerationB ? 0 : 1;
}
inline int LatestBank(Bank A, Bank B)
{
    if (A.Valid && A.Generation <= 0) { A.Valid = false; }
    if (B.Valid && B.Generation <= 0) { B.Valid = false; }
    if (!A.Valid && !B.Valid) { return -1; }
    return !A.Valid ? 1 : (!B.Valid || A.Generation >= B.Generation ? 0 : 1);
}
inline bool NextWrite(Bank A, Bank B, int& BankIndex, std::int64_t& Generation)
{
    const int Current = LatestBank(A, B);
    const std::int64_t Previous = Current < 0 ? 0 : (Current == 0 ? A.Generation : B.Generation);
    if (Previous == std::numeric_limits<std::int64_t>::max()) { return false; }
    BankIndex = Current == 0 ? 1 : 0;
    Generation = Previous + 1;
    return true;
}
inline int OldestAuto(const Bank (&Slots)[AutoSlots])
{
    int Oldest = 0;
    for (int Index = 0; Index < AutoSlots; ++Index)
    {
        if (!Slots[Index].Valid) { return Index; }
        if (Slots[Index].Generation < Slots[Oldest].Generation) { Oldest = Index; }
    }
    return Oldest;
}
struct Admission
{
    bool Authority = false, Standalone = false, Ready = false, Alive = false;
    bool StableIdentity = false, Grounded = false, StateValid = false;
    bool InCombat = false, InCinematic = false, InTraversal = false, UnresolvedChoice = false;
    bool Mutating = false, SavingDisabled = false, NearbyThreat = false;
};
inline bool CanCapture(const Admission& A)
{
    return A.Authority && A.Standalone && A.Ready && A.Alive && A.StableIdentity && A.Grounded
        && A.StateValid && !A.InCombat && !A.InCinematic && !A.InTraversal && !A.UnresolvedChoice
        && !A.Mutating && !A.SavingDisabled && !A.NearbyThreat;
}
}
