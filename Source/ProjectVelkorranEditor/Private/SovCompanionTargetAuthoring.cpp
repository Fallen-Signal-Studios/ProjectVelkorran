// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovBlueprintAuthoringLibrary.h"
#include "Companions/SovCompanionComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"

FSovBlueprintAuthoringResult USovBlueprintAuthoringLibrary::AddCompanionAttackTargetFallback(UObject* Asset)
{
    FSovBlueprintAuthoringResult Result;
    auto* BP = Cast<UBlueprint>(Asset);
    if (!GEditor || GEditor->PlayWorld || !BP || BP->GetPathName() !=
        TEXT("/NarrativePro/Pro/Core/Abilities/GameplayAbilities/Attacks/GA_CombatAbilityBase.GA_CombatAbilityBase"))
    { Result.Report = TEXT("Requires the exact shared combat Blueprint and stopped PIE."); return Result; }
    UEdGraph* Graph = nullptr;
    for (UEdGraph* Candidate : BP->FunctionGraphs)
    { if (Candidate && Candidate->GetFName() == TEXT("GetBotAttackTarget")) { Graph = Candidate; break; } }
    if (!Graph) { Result.Report = TEXT("Missing existing target function."); return Result; }
    UK2Node_DynamicCast* GoalCast = nullptr;
    UK2Node_FunctionResult* EmptyReturn = nullptr;
    UK2Node_VariableGet* Owner = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node->GetFName() == TEXT("K2Node_DynamicCast_2")) { GoalCast = Cast<UK2Node_DynamicCast>(Node); }
        if (Node->GetFName() == TEXT("K2Node_FunctionResult_0")) { EmptyReturn = Cast<UK2Node_FunctionResult>(Node); }
        if (Node->GetFName() == TEXT("K2Node_VariableGet_0")) { Owner = Cast<UK2Node_VariableGet>(Node); }
        if (Node->GetFName() == TEXT("SovCompanionCommandTarget"))
        { Result.Report = TEXT("Fallback already authored; inspect before changing again."); return Result; }
    }
    if (!GoalCast || !GoalCast->TargetType || GoalCast->TargetType->GetPathName() !=
        TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C") || !EmptyReturn || !Owner)
    { Result.Report = TEXT("Original graph contract changed."); return Result; }
    auto* Failure = GoalCast->FindPin(TEXT("CastFailed"));
    auto* ReturnExec = EmptyReturn->FindPin(UEdGraphSchema_K2::PN_Execute);
    auto* ReturnTarget = EmptyReturn->FindPin(TEXT("Target"));
    auto* ReturnFound = EmptyReturn->FindPin(TEXT("Has Target?"));
    auto* OwnerPin = Owner->FindPin(TEXT("CharacterOwner"));
    if (!Failure || !ReturnExec || !ReturnTarget || !ReturnFound || !OwnerPin ||
        !Failure->LinkedTo.IsEmpty() || !ReturnTarget->LinkedTo.IsEmpty() || !ReturnFound->LinkedTo.IsEmpty())
    { Result.Report = TEXT("Expected unconnected legacy failure and empty return pins."); return Result; }
    BP->Modify(); Graph->Modify(); EmptyReturn->Modify(); GoalCast->Modify();
    const auto* Schema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());
    auto* Target = NewObject<UK2Node_CallFunction>(Graph, TEXT("SovCompanionCommandTarget"), RF_Transactional);
    Graph->AddNode(Target, false, false); Target->CreateNewGuid();
    Target->SetFromFunction(USovCompanionComponent::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(USovCompanionComponent, ResolveCommandAttackTarget)));
    Target->NodePosX = 900; Target->NodePosY = 400; Target->AllocateDefaultPins();
    auto* Valid = NewObject<UK2Node_CallFunction>(Graph, TEXT("SovCompanionCommandTargetValid"), RF_Transactional);
    Graph->AddNode(Valid, false, false); Valid->CreateNewGuid();
    Valid->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("IsValid")));
    Valid->NodePosX = 1200; Valid->NodePosY = 400; Valid->AllocateDefaultPins();
    const bool Connected = Schema->TryCreateConnection(OwnerPin, Target->FindPin(TEXT("Character")))
        && Schema->TryCreateConnection(Target->GetReturnValuePin(), ReturnTarget)
        && Schema->TryCreateConnection(Target->GetReturnValuePin(), Valid->FindPin(TEXT("Object")))
        && Schema->TryCreateConnection(Valid->GetReturnValuePin(), ReturnFound)
        && Schema->TryCreateConnection(Failure, ReturnExec);
    if (!Connected)
    {
        Target->BreakAllNodeLinks(); Valid->BreakAllNodeLinks(); Failure->BreakLinkTo(ReturnExec);
        Graph->RemoveNode(Target); Graph->RemoveNode(Valid);
        Result.Report = TEXT("Connection failed; added links and nodes removed. Asset not saved."); return Result;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
    Result.bSucceeded = Log.NumErrors == 0 && BP->Status != BS_Error;
    Result.Report = FString::Printf(TEXT("Companion fallback compiled: errors=%d warnings=%d. Legacy Goal_Attack success unchanged; no save performed."), Log.NumErrors, Log.NumWarnings);
    return Result;
}
