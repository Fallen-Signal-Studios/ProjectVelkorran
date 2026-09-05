// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstdint>
namespace SovPlatformServicesPolicy
{
constexpr int MaximumEnvelopeBytes = 64 * 1024 * 1024;
constexpr int MaximumRevisionsPerSlot = 32;
constexpr double OperationTimeoutSeconds = 45.0;
inline bool ValidSlot(int Kind, int Index)
{ return Index >= 0 && ((Kind == 0 && Index < 10) || (Kind == 1 && Index < 3) || (Kind == 2 && Index == 0)); }
inline bool CanAdmit(bool OptIn, bool SignedIn, bool Available, bool SameAccount, bool Frontend, bool Busy)
{ return OptIn && SignedIn && Available && SameAccount && Frontend && !Busy; }
inline bool CanConsume(bool SameRequest, bool SameAccount, bool ExpectedPhase, bool OptIn)
{ return SameRequest && SameAccount && ExpectedPhase && OptIn; }
inline bool CanPublishRevision(int Count, int Bytes)
{ return Count >= 0 && Count < MaximumRevisionsPerSlot && Bytes > 0 && Bytes <= MaximumEnvelopeBytes; }
}
