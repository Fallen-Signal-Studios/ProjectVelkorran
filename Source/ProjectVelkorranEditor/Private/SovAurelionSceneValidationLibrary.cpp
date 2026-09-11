// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionSceneValidationLibrary.h"
#include "Recovery/SovRecoveryExclusionVolume.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"

FSovAurelionExitGeometryResult USovAurelionSceneValidationLibrary::ValidateAurelionExitGeometry(
    UWorld* World, FTransform ExitTransform, float Radius, float HalfHeight)
{
    FSovAurelionExitGeometryResult Result;
    const FString Package = World ? World->GetOutermost()->GetName() : FString();
    if (!GEditor || GEditor->PlayWorld || !IsValid(World) || World->WorldType != EWorldType::Editor
        || GEditor->GetEditorWorldContext().World() != World
        || (Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M12") && Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M13")))
    { Result.Report = TEXT("Exit geometry validation requires the exact stopped Aurelion mission wrapper."); return Result; }
    if (!ExitTransform.IsValid() || !ExitTransform.GetScale3D().Equals(FVector::OneVector)
        || !FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight)
        || Radius <= 0.f || Radius > 200.f || HalfHeight < Radius || HalfHeight > 400.f)
    { Result.Report = TEXT("Exit geometry requires a finite unit-scale transform and actual bounded character capsule."); return Result; }

    FCollisionQueryParams Query(SCENE_QUERY_STAT(SovAurelionAuthoringExit), false);
    for (TActorIterator<ANarrativeCharacter> It(World); It; ++It)
    {
        Query.AddIgnoredActor(*It);
        TArray<AActor*> Attached;
        It->GetAttachedActors(Attached, true, true);
        Query.AddIgnoredActors(Attached);
        Result.IgnoredCharacters.Add(It->GetPathName());
    }
    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByChannel(Overlaps, ExitTransform.GetLocation(), ExitTransform.GetRotation(), ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius, HalfHeight), Query);
    for (const auto& Overlap : Overlaps)
    {
        if (!Overlap.bBlockingHit) { continue; }
        const auto* Component = Overlap.GetComponent();
        Result.BlockingComponents.AddUnique(Component ? Component->GetPathName() : TEXT("Unresolved blocking component"));
    }
    Result.BlockingComponents.Sort(); Result.IgnoredCharacters.Sort();
    Result.bRecoveryExcluded = ASovRecoveryExclusionVolume::ExcludesCapsule(World, ExitTransform.GetLocation(), Radius, HalfHeight);
    Result.bSucceeded = true;
    Result.bClear = Result.BlockingComponents.IsEmpty() && !Result.bRecoveryExcluded;
    Result.Report = Result.bClear ? TEXT("Static exit geometry is clear; logical character and runtime navigation admission remain required.")
        : FString::Printf(TEXT("Exit geometry blocked by %d component(s); recovery exclusion: %s."),
            Result.BlockingComponents.Num(), Result.bRecoveryExcluded ? TEXT("yes") : TEXT("no"));
    return Result;
}
