// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovBlueprintAuthoringLibrary.h"
#include "Editor.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

FSovBlueprintAuthoringResult USovBlueprintAuthoringLibrary::ConfigureVerityTwinLocomotion(
    UObject* Asset, UObject* Blend, const TArray<UObject*>& Clips)
{
    FSovBlueprintAuthoringResult Result;
    UAnimBlueprint* BP = Cast<UAnimBlueprint>(Asset);
    UBlendSpace* BS = Cast<UBlendSpace>(Blend);
    const FString Root(TEXT("/Game/Characters/Animation/VerityTwinBlades/"));
    if (!GEditor || GEditor->PlayWorld || !BP || !BS || !BP->GetPathName().StartsWith(Root) ||
        !BS->GetPathName().StartsWith(Root) || Clips.Num() != 3 ||
        (BS->GetNumberOfBlendSamples() != 0 && BS->GetNumberOfBlendSamples() != 9))
    { Result.Report = TEXT("Requires stopped PIE, explicit project overlay/empty blend space and three clips."); return Result; }
    for (UObject* Object : Clips)
    {
        UAnimSequence* Clip = Cast<UAnimSequence>(Object);
        if (!Clip || !Clip->GetPathName().StartsWith(Root) || Clip->GetSkeleton() != BP->TargetSkeleton)
        { Result.Report = TEXT("All stance clips must use the overlay skeleton."); return Result; }
    }
    TArray<UObject*> Objects;
    GetObjectsWithOuter(BP, Objects, true);
    UAnimGraphNode_SequencePlayer* Idle = nullptr;
    UAnimGraphNode_BlendSpacePlayer* Template = nullptr;
    for (UObject* Object : Objects)
    {
        if (UAnimGraphNode_SequencePlayer* Node = Cast<UAnimGraphNode_SequencePlayer>(Object))
        { if (Node->GetGraph()->GetFName() == TEXT("Idle_3P") &&
            Node->GetPathName().StartsWith(BP->GetPathName() + TEXT(":Overlay."))) { Idle = Node; } }
        if (UAnimGraphNode_BlendSpacePlayer* Node = Cast<UAnimGraphNode_BlendSpacePlayer>(Object))
        { if (Node->GetGraph()->GetFName() == TEXT("Idle_1P") &&
            Node->GetPathName().StartsWith(BP->GetPathName() + TEXT(":Overlay."))) { Template = Node; } }
    }
    if (!Idle || !Template)
    { Result.Report = TEXT("Expected existing third-person idle and first-person speed/direction blend nodes."); return Result; }
    BP->Modify(); BS->Modify();
    BS->SetSkeleton(BP->TargetSkeleton);
    FStructProperty* Params = FindFProperty<FStructProperty>(BS->GetClass(), TEXT("BlendParameters"));
    if (!Params || Params->ArrayDim != 3) { Result.Report = TEXT("Missing blend parameters."); return Result; }
    FBlendParameter* Direction = Params->ContainerPtrToValuePtr<FBlendParameter>(BS, 0);
    FBlendParameter* Speed = Params->ContainerPtrToValuePtr<FBlendParameter>(BS, 1);
    Direction->DisplayName = TEXT("Direction"); Direction->Min = -180.f; Direction->Max = 180.f; Direction->GridNum = 4;
    Speed->DisplayName = TEXT("Speed"); Speed->Min = 0.f; Speed->Max = 600.f; Speed->GridNum = 4;
    const float Speeds[] = {0.f, 150.f, 600.f};
    // The weapon overlay supplies the stance; base locomotion retains directional footwork.
    if (BS->GetNumberOfBlendSamples() == 0)
    { for (float Angle : {-180.f, 0.f, 180.f})
      { for (int32 I = 0; I < 3; ++I) { BS->AddSample(CastChecked<UAnimSequence>(Clips[I]), FVector(Angle, Speeds[I], 0.f)); } } }
    BS->ValidateSampleData(); BS->ResampleData(); BS->PostEditChange();
    UEdGraph* Graph = Idle->GetGraph();
    UAnimGraphNode_BlendSpacePlayer* Node = DuplicateObject<UAnimGraphNode_BlendSpacePlayer>(Template, Graph, TEXT("VerityTwinStance"));
    Node->CreateNewGuid();
    for (UEdGraphPin* Pin : Node->Pins) { Pin->LinkedTo.Reset(); }
    // Keep the copied thread-safe X/Y property bindings, without the first-person update callback.
    Node->Node = FAnimNode_BlendSpacePlayer();
    Node->Node.SetBlendSpace(BS);
    Node->NodePosX = Idle->NodePosX; Node->NodePosY = Idle->NodePosY;
    Graph->AddNode(Node, false, false);
    Node->ReconstructNode();
    Node->Node.SetBlendSpace(BS);
    if (UEdGraphPin* BlendPin = Node->FindPin(TEXT("BlendSpace"))) { BlendPin->DefaultObject = BS; }
    UEdGraphPin* OldPose = Idle->FindPin(TEXT("Pose"));
    UEdGraphPin* NewPose = Node->FindPin(TEXT("Pose"));
    if (!OldPose || !NewPose) { Result.Report = TEXT("Missing pose pins; save withheld."); return Result; }
    const TArray<UEdGraphPin*> Consumers = OldPose->LinkedTo;
    OldPose->BreakAllPinLinks();
    for (UEdGraphPin* Consumer : Consumers) { NewPose->MakeLinkTo(Consumer); }
    Idle->DestroyNode();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
    Result.bSucceeded = Log.NumErrors == 0;
    Result.Report = FString::Printf(TEXT("Verity stance blend: idle/walk/run, existing thread-safe direction/Speed2D bindings; %d errors, %d warnings."), Log.NumErrors, Log.NumWarnings);
    return Result;
}

FSovBlueprintAuthoringResult USovBlueprintAuthoringLibrary::ConfigureVerityTwinMontage(
    UObject* Asset, UObject* Sequence, float AttackStart, float AttackEnd)
{
    FSovBlueprintAuthoringResult Result;
    UAnimMontage* Montage = Cast<UAnimMontage>(Asset);
    UAnimSequence* Clip = Cast<UAnimSequence>(Sequence);
    const FString Root(TEXT("/Game/Characters/Animation/VerityTwinBlades/"));
    if (!GEditor || GEditor->PlayWorld || !Montage || !Clip ||
        !Montage->GetPathName().StartsWith(Root) || !Clip->GetPathName().StartsWith(Root) ||
        Montage->GetSkeleton() != Clip->GetSkeleton() || AttackStart <= 0.f ||
        AttackEnd <= AttackStart || AttackEnd >= Clip->GetPlayLength() ||
        Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].SlotName != TEXT("FullBody") ||
        Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1 || Montage->CompositeSections.Num() != 1)
    { Result.Report = TEXT("Requires stopped PIE, matching project clips, and the single FullBody montage template."); return Result; }
    int32 AttackNotifies = 0;
    for (const FAnimNotifyEvent& Event : Montage->Notifies)
    { if (Event.NotifyStateClass && Event.NotifyStateClass->GetClass()->GetName() == TEXT("ANS_AttackAnimation_C")) { ++AttackNotifies; } }
    if (AttackNotifies != 1)
    { Result.Report = TEXT("Expected exactly one existing native attack window."); return Result; }
    Montage->Modify();
    FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
    Segment.SetAnimReference(Clip);
    Segment.AnimStartTime = 0.f;
    Segment.AnimEndTime = Clip->GetPlayLength();
    Segment.AnimPlayRate = 1.f;
    Segment.LoopingCount = 1;
    Montage->SetCompositeLength(Clip->GetPlayLength());
    Montage->CompositeSections[0].Link(Montage, 0.f);
    for (FAnimNotifyEvent& Event : Montage->Notifies)
    {
        const bool bAttack = Event.NotifyStateClass && Event.NotifyStateClass->GetClass()->GetName() == TEXT("ANS_AttackAnimation_C");
        const bool bWarp = Event.NotifyStateClass && Event.NotifyStateClass->GetClass()->GetName().Contains(TEXT("MotionWarping"));
        const float Start = bWarp ? FMath::Max(0.01f, AttackStart * 0.25f) : AttackStart;
        const float End = bWarp ? AttackStart : AttackEnd;
        Event.Link(Montage, Start);
        if (Event.NotifyStateClass)
        {
            Event.SetDuration(End - Start);
            Event.EndLink.Link(Montage, End);
        }
        if (bAttack) { Event.MontageTickType = EMontageNotifyTickType::BranchingPoint; }
    }
    Montage->UpdateLinkableElements();
    Montage->RefreshCacheData();
    Montage->PostEditChange();
    Result.bSucceeded = true;
    Result.Report = FString::Printf(TEXT("FullBody montage: %.3fs, native attack window %.3f-%.3fs; existing warp/sound/trail classes retained."),
        Clip->GetPlayLength(), AttackStart, AttackEnd);
    return Result;
}
