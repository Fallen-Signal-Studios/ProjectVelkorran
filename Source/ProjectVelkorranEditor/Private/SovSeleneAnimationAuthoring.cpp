#include "SovBlueprintAuthoringLibrary.h"
#include "AnimGraphNode_SovSelenePosture.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimInstance.h"

FSovBlueprintAuthoringResult USovBlueprintAuthoringLibrary::PreviewSeleneFemininePosture(UObject* Instance, bool bEnabled)
{
    FSovBlueprintAuthoringResult Result;
    auto* Anim = Cast<UAnimInstance>(Instance);
    if (!GEditor || !GEditor->PlayWorld || !Anim || Anim->GetWorld() != GEditor->PlayWorld ||
        Anim->GetClass()->GetPathName() != TEXT("/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Base/ABP_Biped.ABP_Biped_C"))
    { Result.Report = TEXT("Requires a live PIE instance of ABP_Biped."); return Result; }
    auto* Property = FindFProperty<FStructProperty>(Anim->GetClass(), TEXT("SeleneFemininePosture"));
    if (!Property || Property->Struct != FAnimNode_SovSelenePosture::StaticStruct())
    { Result.Report = TEXT("Selene posture node is absent."); return Result; }
    auto* Node = Property->ContainerPtrToValuePtr<FAnimNode_SovSelenePosture>(Anim);
    Result.Report = Node->DescribePosture();
    auto* Blend = bEnabled ? LoadObject<UBlendSpace>(nullptr,
        TEXT("/Game/Characters/Animation/SeleneGASPALS/BS_SeleneFemininePostureDelta.BS_SeleneFemininePostureDelta")) : nullptr;
    if (bEnabled && !Blend) { Result.Report = TEXT("Posture blend is missing."); return Result; }
    Node->Posture = Blend;
    Result.bSucceeded = true;
    return Result;
}

FSovBlueprintAuthoringResult USovBlueprintAuthoringLibrary::ConfigureSeleneFemininePosture(UObject* Asset, UObject* Blend)
{
    FSovBlueprintAuthoringResult Result;
    auto* BP = Cast<UAnimBlueprint>(Asset);
    auto* BS = Cast<UBlendSpace>(Blend);
    if (!GEditor || GEditor->PlayWorld || !BP || !BS ||
        BP->GetPathName() != TEXT("/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Base/ABP_Biped.ABP_Biped") ||
        BS->GetPathName() != TEXT("/Game/Characters/Animation/SeleneGASPALS/BS_SeleneFemininePostureDelta.BS_SeleneFemininePostureDelta") ||
        BP->TargetSkeleton != BS->GetSkeleton() || !BS->IsValidAdditive())
    { Result.Report = TEXT("Requires stopped PIE, exact base Blueprint and compatible additive Selene blend."); return Result; }
    TArray<UObject*> Objects;
    GetObjectsWithOuter(BP, Objects, true);
    UEdGraphNode* Source = nullptr;
    UEdGraphNode* Destination = nullptr;
    for (auto* Object : Objects)
    {
        auto* Node = Cast<UEdGraphNode>(Object);
        if (!Node || Node->GetOuter()->GetPathName() != BP->GetPathName() + TEXT(":AnimGraph")) { continue; }
        if (Cast<UAnimGraphNode_SovSelenePosture>(Node))
        { Result.Report = TEXT("Posture already exists; refusing duplicate insertion."); return Result; }
        if (Node->GetFName() == TEXT("AnimGraphNode_ApplyMeshSpaceAdditive_5")) { Source = Node; }
        if (Node->GetFName() == TEXT("AnimGraphNode_Slot_3")) { Destination = Node; }
    }
    UEdGraphPin* Out = Source ? Source->FindPin(TEXT("Pose"), EGPD_Output) : nullptr;
    UEdGraphPin* In = Destination ? Destination->FindPin(TEXT("Source"), EGPD_Input) : nullptr;
    if (!Out || !In || Out->LinkedTo.Num() != 1 || Out->LinkedTo[0] != In || In->LinkedTo.Num() != 1)
    { Result.Report = TEXT("Expected exact lean-to-PreLookAt edge; graph was not changed."); return Result; }
    BP->Modify();
    UEdGraph* Graph = Source->GetGraph();
    Graph->Modify();
    auto* Node = NewObject<UAnimGraphNode_SovSelenePosture>(Graph, TEXT("SeleneFemininePosture"), RF_Transactional);
    Node->Node.Posture = BS;
    Node->CreateNewGuid();
    Graph->AddNode(Node, false, false);
    Node->AllocateDefaultPins();
    Node->NodePosX = Destination->NodePosX - 220;
    Node->NodePosY = Destination->NodePosY + 180;
    auto* BasePin = Node->FindPinChecked(TEXT("BasePose"), EGPD_Input);
    auto* PosePin = Node->FindPinChecked(TEXT("Pose"), EGPD_Output);
    Out->BreakLinkTo(In);
    Out->MakeLinkTo(BasePin);
    PosePin->MakeLinkTo(In);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
    Result.bSucceeded = Log.NumErrors == 0;
    Result.Report = FString::Printf(TEXT("Inserted Selene posture; compile errors=%d warnings=%d. Asset not saved."), Log.NumErrors, Log.NumWarnings);
    return Result;
}
