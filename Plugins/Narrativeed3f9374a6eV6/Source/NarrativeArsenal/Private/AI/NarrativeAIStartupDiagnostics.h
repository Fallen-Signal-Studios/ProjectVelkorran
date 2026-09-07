// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class ANarrativeNPCController;
struct FAIStimulus;

/** Opt-in observation only. Arm before creating a game world; never repairs AI. */
struct FNarrativeAIStartupDiagnostics
{
#if !UE_BUILD_SHIPPING
    static void Startup();
    static void Shutdown();
    static void Record(const UObject* Context, const TCHAR* Event, const FString& Detail = FString());
    static void Snapshot(ANarrativeNPCController* Controller, bool bForce = false);
    static void Perception(ANarrativeNPCController* Controller, AActor* Target, const FAIStimulus& Stimulus);
#else
    static void Startup() {}
    static void Shutdown() {}
    static void Record(const UObject*, const TCHAR*, const FString& = FString()) {}
    static void Snapshot(ANarrativeNPCController*, bool = false) {}
    static void Perception(ANarrativeNPCController*, AActor*, const FAIStimulus&) {}
#endif
};
