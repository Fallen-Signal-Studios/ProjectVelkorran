// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionCrossfireAuthoringLibrary.h"
#include "AI/SovAurelionCrossfireQuery.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Tasks/BTTask_RunEQSQuery.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr TCHAR OwnedRoot[] = TEXT("/Game/Aurelion/Enemies/");
constexpr TCHAR SourceTreePath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/ShootAndStrafe/BT_Attack_Ranged.BT_Attack_Ranged");
constexpr TCHAR SourceActivityPath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/ShootAndStrafe/BPA_Attack_Ranged_Strafe.BPA_Attack_Ranged_Strafe_C");
constexpr TCHAR StrafePath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/EQS/EQS_Move_RangedStrafe_GetClose.EQS_Move_RangedStrafe_GetClose");
constexpr TCHAR LookBusyPath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/EQS/EQS_Move_LookBusy.EQS_Move_LookBusy");
bool Editing() { return GEditor && !GEditor->PlayWorld; }

FString RuntimeSnapshot(const UBehaviorTree* Tree, const UBTNode* Omit = nullptr)
{
    FString Result;
    const auto Normalize = [Tree](FString Text)
    {
        Text.ReplaceInline(*Tree->GetPathName(), TEXT("$TREE"));
        Text.ReplaceInline(*(Tree->GetName() + TEXT(":")), TEXT("$TREE:"));
        return Text;
    };
    TFunction<void(const UBTNode*)> Visit = [&](const UBTNode* Node)
    {
        if (!Node || Node == Omit) { return; }
        Result += Normalize(Node->GetPathName()) + TEXT("|") + Node->GetClass()->GetPathName() + TEXT("\n");
        for (TFieldIterator<FProperty> It(Node->GetClass()); It; ++It)
        {
            // Composite children are serialized in traversal order below; the one inserted
            // task is the only omission. Every old node's authored fields remain exact.
            if (It->GetFName() == TEXT("Children") || It->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient)) { continue; }
            FString Value; It->ExportText_InContainer(0,Value,Node,nullptr,const_cast<UBTNode*>(Node),PPF_None);
            Result += It->GetName() + TEXT("=") + Normalize(Value) + TEXT("\n");
        }
        if (const auto* Composite = Cast<UBTCompositeNode>(Node))
        {
            for (const UBTService* Service : Composite->Services) { Visit(Service); }
            for (const auto& Child : Composite->Children)
            {
                if (Omit && Child.ChildTask == Omit) { continue; }
                Result += TEXT("CHILD\n");
                for (const auto& Op : Child.DecoratorOps)
                { Result += FString::Printf(TEXT("OP %d %d\n"),static_cast<int32>(Op.Operation),Op.Number); }
                for (const UBTDecorator* Decorator : Child.Decorators) { Visit(Decorator); }
                if (Child.ChildComposite) { Visit(Child.ChildComposite); } else { Visit(Child.ChildTask); }
            }
        }
        else if (const auto* Task = Cast<UBTTaskNode>(Node))
        { for (const UBTService* Service : Task->Services) { Visit(Service); } }
    };
    Result += GetPathNameSafe(Tree->BlackboardAsset) + TEXT("\n");
    Visit(Tree->RootNode); return Result;
}

void FindChoosers(UBTCompositeNode* Node, TArray<UBTCompositeNode*>& Found)
{
    if (!Node) { return; }
    if (Node->Children.Num() == 3)
    {
        const auto* First = Cast<UBTTask_RunEQSQuery>(Node->Children[0].ChildTask);
        const auto* Second = Cast<UBTTask_RunEQSQuery>(Node->Children[1].ChildTask);
        const auto* Third = Cast<UBTTask_RunEQSQuery>(Node->Children[2].ChildTask);
        if (First && Second && Third && First->EQSRequest.QueryTemplate && Second->EQSRequest.QueryTemplate
            && First->EQSRequest.QueryTemplate->GetPathName() == LookBusyPath
            && Second->EQSRequest.QueryTemplate->GetPathName() == StrafePath
            && Node->Children[0].Decorators.Num() == 1 && Node->Children[1].Decorators.IsEmpty()
            && Node->Children[2].Decorators.IsEmpty()) { Found.Add(Node); }
    }
    for (auto& Child : Node->Children) { FindChoosers(Child.ChildComposite, Found); }
}
// Mirrors the serialized execution order. The native BT manager still duplicates and
// initializes its actual runtime nodes/memory; no custom task lifetime is introduced.
void IndexTree(UBTCompositeNode* Node, UBTCompositeNode* Parent, uint8 Depth, uint16& Index)
{
    Node->InitializeNode(Parent, Index++, 0, Depth);
    for (UBTService* Service : Node->Services) { Service->InitializeNode(Node, Index++, 0, Depth); }
    for (int32 ChildIndex = 0; ChildIndex < Node->Children.Num(); ++ChildIndex)
    {
        auto& Child = Node->Children[ChildIndex];
        for (UBTDecorator* Decorator : Child.Decorators)
        { Decorator->InitializeNode(Node, Index++, 0, Depth); Decorator->InitializeParentLink(ChildIndex); }
        if (Child.ChildComposite) { IndexTree(Child.ChildComposite, Node, Depth + 1, Index); }
        else if (Child.ChildTask)
        {
            for (UBTService* Service : Child.ChildTask->Services)
            { Service->InitializeNode(Node, Index++, 0, Depth); Service->InitializeParentLink(ChildIndex); }
            Child.ChildTask->InitializeNode(Node, Index++, 0, Depth + 1);
        }
    }
    Node->InitializeComposite(Index - 1);
}
}

FSovAurelionCrossfireAuthoringResult USovAurelionCrossfireAuthoringLibrary::CreateCrossfireAssets()
{
    FSovAurelionCrossfireAuthoringResult R;
    const FString QueryPackage = FString(OwnedRoot) + TEXT("EQS_AurelionSecurityCrossfire");
    const FString TreePackage = FString(OwnedRoot) + TEXT("BT_AurelionSecurityCrossfire");
    if (!Editing()) { R.Error = TEXT("Requires an editor world outside PIE."); return R; }
    if (FPackageName::DoesPackageExist(QueryPackage) || FPackageName::DoesPackageExist(TreePackage)
        || FindObject<UPackage>(nullptr, *QueryPackage) || FindObject<UPackage>(nullptr, *TreePackage))
    { R.Error = TEXT("Owned destinations already exist; refusing replacement."); return R; }
    auto* Source = LoadObject<UBehaviorTree>(nullptr, SourceTreePath);
    TArray<UBTCompositeNode*> SourceChoosers; FindChoosers(Source ? Source->RootNode.Get() : nullptr, SourceChoosers);
    if (!Source || !Source->BlackboardAsset || SourceChoosers.Num() != 1)
    { R.Error = TEXT("Installed stock ranged chooser differs from the reviewed three-branch structure."); return R; }
    R.SourceRuntimeNodes = RuntimeSnapshot(Source);
    // Actual Z01_Floor top is 0cm, XY [-8300,-5700] x [-17500,-11900].
    // The authored 0..2m combat elevation band admits Recast's small floor offset.
    FSovAurelionCrossfireBounds Bounds;
    Bounds.EncounterId = TEXT("M12_E1_PressureHall"); Bounds.Minimum = FVector(-8300,-17500,-25); Bounds.Maximum = FVector(-5700,-11900,225);
    auto* Query = NewObject<UEnvQuery>(CreatePackage(*QueryPackage), *FPackageName::GetShortName(QueryPackage), RF_Public | RF_Standalone | RF_Transactional);
    auto* Option = NewObject<UEnvQueryOption>(Query, TEXT("LinkedLateral"), RF_Transactional);
    auto* Generator = NewObject<UEnvQueryGenerator_SovCrossfire>(Option, TEXT("EightLateralPoints"), RF_Transactional);
    auto* Test = NewObject<UEnvQueryTest_SovCrossfire>(Option, TEXT("ClearSupportedCrossfire"), RF_Transactional);
    Generator->Bounds = Bounds; Test->Bounds = Bounds; Option->Generator = Generator; Option->Tests.Add(Test);
    Query->GetOptionsMutable().Add(Option);
    auto* Tree = Cast<UBehaviorTree>(StaticDuplicateObject(Source, CreatePackage(*TreePackage), *FPackageName::GetShortName(TreePackage)));
    if (!Tree) { R.Error = TEXT("Owned tree duplication failed."); return R; }
    TArray<UBTCompositeNode*> Choosers; FindChoosers(Tree->RootNode, Choosers);
    if (Choosers.Num() != 1) { R.Error = TEXT("Duplicated tree chooser mismatch."); return R; }
    auto* Chooser = Choosers[0]; auto* Stock = Cast<UBTTask_RunEQSQuery>(Chooser->Children[1].ChildTask);
    auto* Task = DuplicateObject<UBTTask_RunEQSQuery>(Stock, Tree, TEXT("TryLinkedCrossfire"));
    Task->EQSRequest.QueryTemplate = Query; Task->EQSRequest.QueryConfig.Reset();
    Task->EQSRequest.RunMode = EEnvQueryRunMode::SingleResult; Task->bUseBBKey = false; Task->EQSRequest.bUseBBKeyForQueryTemplate = false;
    Task->EQSRequest.bInitialized = false;
    Task->bUpdateBBOnFail = false;
    FBTCompositeChild Extra; Extra.ChildTask = Task; Chooser->Children.Insert(Extra, 1);
    // No stale editable graph may reconstruct away the inserted runtime node.
    // Stock BehaviorTree editor reconstructs a missing graph from runtime nodes.
    Tree->BTGraph = nullptr; Tree->LastEditedDocuments.Reset(); uint16 Index = 0; IndexTree(Tree->RootNode, nullptr, 0, Index);
    R.PreservedRuntimeNodes = RuntimeSnapshot(Tree,Task);
    if (R.SourceRuntimeNodes != R.PreservedRuntimeNodes || RuntimeSnapshot(Source) != R.SourceRuntimeNodes)
    { R.Error = TEXT("An original attack service, fallback, decorator, blackboard or MoveTo changed during duplication."); return R; }
    FAssetRegistryModule::AssetCreated(Query); FAssetRegistryModule::AssetCreated(Tree);
    Query->MarkPackageDirty(); Tree->MarkPackageDirty();
    R.Query = Query; R.Tree = Tree; R.bSucceeded = true; return R;
}

bool USovAurelionCrossfireAuthoringLibrary::BindCrossfireActivity(UBlueprint* OwnedActivity, UBehaviorTree* Tree)
{
    UClass* StockClass = LoadClass<UNPCActivity>(nullptr, SourceActivityPath);
    if (!Editing() || !IsValid(OwnedActivity) || OwnedActivity->GetOutermost()->GetName() != FString(OwnedRoot) + TEXT("BPA_AurelionSecurityCrossfire")
        || !StockClass || OwnedActivity->ParentClass != StockClass || !OwnedActivity->GeneratedClass || !IsValid(Tree)
        || Tree->GetOutermost()->GetName() != FString(OwnedRoot) + TEXT("BT_AurelionSecurityCrossfire")) { return false; }
    auto* Field = FindFProperty<FObjectPropertyBase>(UNPCActivity::StaticClass(), TEXT("BehaviourTree"));
    auto* Defaults = OwnedActivity->GeneratedClass->GetDefaultObject<UNPCActivity>();
    if (!Field || !Defaults) { return false; }
    Defaults->Modify(); Field->SetObjectPropertyValue_InContainer(Defaults, Tree); OwnedActivity->MarkPackageDirty();
    return Field->GetObjectPropertyValue_InContainer(Defaults) == Tree;
}
bool USovAurelionCrossfireAuthoringLibrary::InstallCrossfireActivity(UBlueprint* OwnedActivity)
{
    UClass* StockClass = LoadClass<UNPCActivity>(nullptr, SourceActivityPath);
    auto* Config = LoadObject<UNPCActivityConfiguration>(nullptr, TEXT("/Game/Aurelion/Enemies/AC_AurelionSecurityDrone.AC_AurelionSecurityDrone"));
    if (!Editing() || !IsValid(OwnedActivity) || !StockClass || !Config || !OwnedActivity->GeneratedClass
        || OwnedActivity->ParentClass != StockClass || OwnedActivity->GetOutermost()->GetName() != FString(OwnedRoot) + TEXT("BPA_AurelionSecurityCrossfire")) { return false; }
    int32 Index = INDEX_NONE;
    for (int32 I = 0; I < Config->DefaultActivities.Num(); ++I)
    { if (Config->DefaultActivities[I] == StockClass) { if (Index != INDEX_NONE) { return false; } Index = I; } }
    if (Index == INDEX_NONE) { return false; }
    Config->Modify(); Config->DefaultActivities[Index] = OwnedActivity->GeneratedClass; Config->MarkPackageDirty(); return true;
}
