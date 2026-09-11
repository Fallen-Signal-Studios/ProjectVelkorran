// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionEnemyAuthoringLibrary.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "AI/SovAurelionRoleActivities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr TCHAR Folder[] = TEXT("/Game/Aurelion/Enemies/");
bool EditorOnly() { return GEditor && !GEditor->PlayWorld; }
FString TreePackage(bool bTraversal)
{ return FString(Folder) + (bTraversal ? TEXT("BT_AurelionWallTraversal") : TEXT("BT_AurelionWeaverSupport")); }
bool CorrectTree(const UBehaviorTree* Tree, bool bTraversal)
{
    const UBTCompositeNode* Root = IsValid(Tree) ? Tree->RootNode.Get() : nullptr;
    if (!Root || Root->GetClass() != UBTComposite_Selector::StaticClass() || Root->Children.Num() != 2 || !Tree->BlackboardAsset) { return false; }
    const UBTCompositeNode* Sequence = Root->Children[0].ChildComposite;
    const auto* RejectedWait = Cast<UBTTask_Wait>(Root->Children[1].ChildTask);
    if (!Sequence || Sequence->GetClass() != UBTComposite_Sequence::StaticClass() || Sequence->Children.Num() != 2 || !RejectedWait) { return false; }
    const UBTTaskNode* Task = Sequence->Children[0].ChildTask;
    const auto* AcceptedWait = Cast<UBTTask_Wait>(Sequence->Children[1].ChildTask);
    return Task && Task->GetClass() == (bTraversal ? UBTTask_SovAurelionTraverseWall::StaticClass() : UBTTask_SovAurelionWeaverSupport::StaticClass())
        && AcceptedWait && AcceptedWait->WaitTime.GetValue(static_cast<const UBlackboardComponent*>(nullptr)) == .5f
        && RejectedWait->WaitTime.GetValue(static_cast<const UBlackboardComponent*>(nullptr)) == .5f;
}
}

bool USovAurelionEnemyAuthoringLibrary::CompileOwnedBlueprint(UBlueprint* Blueprint, UClass* ExpectedParent)
{
    if (!EditorOnly() || !IsValid(Blueprint)) { return false; }
    const FString Package = Blueprint->GetOutermost()->GetName();
    const bool bAurelionFramework = Package == TEXT("/Game/Aurelion/Framework/BP_AurelionPlayerController")
        || Package == TEXT("/Game/Aurelion/Framework/BP_AurelionGameMode_M12")
        || Package == TEXT("/Game/Aurelion/Framework/BP_AurelionGameMode_M13");
    if (!Package.StartsWith(Folder) && !bAurelionFramework) { return false; }
    if (ExpectedParent && Blueprint->ParentClass != ExpectedParent) { return false; }
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Results);
    return Results.NumErrors == 0 && Blueprint->GeneratedClass;
}

FSovAurelionEnemyAuthoringResult USovAurelionEnemyAuthoringLibrary::CreateRoleTree(bool bTraversal)
{
    FSovAurelionEnemyAuthoringResult Result;
    if (!EditorOnly()) { Result.Error = TEXT("Role trees may be authored only outside PIE."); return Result; }
    const FString PackageName = TreePackage(bTraversal);
    const FString Name = FPackageName::GetShortName(PackageName);
    if (auto* Existing = LoadObject<UBehaviorTree>(nullptr, *(PackageName + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
    {
        Result.bSucceeded = CorrectTree(Existing, bTraversal); Result.Tree = Existing;
        if (!Result.bSucceeded) { Result.Error = TEXT("Existing role tree differs from the expected owned structure; refusing to overwrite it."); }
        return Result;
    }
    // Fixed names and no loaded object replacement. Runtime node data is what UE's manager consumes.
    // The stock BehaviorTree editor's OnCreated/SpawnMissingNodes reconstructs its editable graph when opened.
    UPackage* Package = CreatePackage(*PackageName);
    if (FindObject<UObject>(Package, *Name)) { Result.Error = TEXT("An incompatible object already occupies the role tree name."); return Result; }
    auto* Tree = NewObject<UBehaviorTree>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
    Tree->BlackboardAsset = NewObject<UBlackboardData>(Tree, TEXT("RoleBlackboard"), RF_Transactional);
    auto* Root = NewObject<UBTComposite_Selector>(Tree, TEXT("RoleOrWait"), RF_Transactional);
    auto* Sequence = NewObject<UBTComposite_Sequence>(Tree, TEXT("RoleThenWait"), RF_Transactional);
    UBTTaskNode* Role = bTraversal ? static_cast<UBTTaskNode*>(NewObject<UBTTask_SovAurelionTraverseWall>(Tree, TEXT("TraverseWall"), RF_Transactional))
        : static_cast<UBTTaskNode*>(NewObject<UBTTask_SovAurelionWeaverSupport>(Tree, TEXT("SupportLinkedAllies"), RF_Transactional));
    auto* AcceptedWait = NewObject<UBTTask_Wait>(Tree, TEXT("AcceptedWait"), RF_Transactional);
    auto* RejectedWait = NewObject<UBTTask_Wait>(Tree, TEXT("RejectedWait"), RF_Transactional);
    AcceptedWait->WaitTime = FValueOrBBKey_Float(.5f); RejectedWait->WaitTime = FValueOrBBKey_Float(.5f);
    AcceptedWait->RandomDeviation = FValueOrBBKey_Float(0.f); RejectedWait->RandomDeviation = FValueOrBBKey_Float(0.f);
    Root->Children.AddDefaulted_GetRef().ChildComposite = Sequence;
    Root->Children.AddDefaulted_GetRef().ChildTask = RejectedWait;
    Sequence->Children.AddDefaulted_GetRef().ChildTask = Role;
    Sequence->Children.AddDefaulted_GetRef().ChildTask = AcceptedWait;
    Root->InitializeNode(nullptr, 0, 0, 0); Sequence->InitializeNode(Root, 1, 0, 1);
    Role->InitializeNode(Sequence, 2, 0, 2); AcceptedWait->InitializeNode(Sequence, 3, 0, 2); RejectedWait->InitializeNode(Root, 4, 0, 1);
    Sequence->InitializeComposite(3); Root->InitializeComposite(4); Tree->RootNode = Root;
    Result.Tree = Tree; Result.bSucceeded = CorrectTree(Tree, bTraversal);
    if (Result.bSucceeded) { FAssetRegistryModule::AssetCreated(Tree); Tree->MarkPackageDirty(); }
    else { Result.Error = TEXT("Created tree failed its structural check."); }
    return Result;
}

bool USovAurelionEnemyAuthoringLibrary::ConfigureRoleActivity(UBlueprint* Blueprint, bool bTraversal)
{
    const FString Expected = FString(Folder) + (bTraversal ? TEXT("BPA_AurelionWallTraversal") : TEXT("BPA_AurelionWeaverSupport"));
    if (!EditorOnly() || !IsValid(Blueprint) || Blueprint->GetOutermost()->GetName() != Expected || !Blueprint->GeneratedClass
        || Blueprint->ParentClass != (bTraversal ? USovAurelionWallTraversalActivity::StaticClass() : USovAurelionWeaverSupportActivity::StaticClass())) { return false; }
    const auto Result = CreateRoleTree(bTraversal);
    if (!Result.bSucceeded) { return false; }
    auto* Property = FindFProperty<FObjectPropertyBase>(UNPCActivity::StaticClass(), TEXT("BehaviourTree"));
    auto* Defaults = Blueprint->GeneratedClass->GetDefaultObject<UNPCActivity>();
    if (!Property || !Defaults) { return false; }
    Defaults->Modify(); Property->SetObjectPropertyValue_InContainer(Defaults, Result.Tree); Blueprint->MarkPackageDirty();
    return Property->GetObjectPropertyValue_InContainer(Defaults) == Result.Tree;
}

bool USovAurelionEnemyAuthoringLibrary::ConfigureEliteCore(UBlueprint* Blueprint, USkeletalMesh* Mesh, FName Bone)
{
    if (!EditorOnly() || !IsValid(Blueprint) || Blueprint->GetOutermost()->GetName() != FString(Folder) + TEXT("BP_AurelionElite")
        || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(ASovAurelionElite::StaticClass())
        || !IsValid(Mesh) || Bone.IsNone() || Mesh->GetRefSkeleton().FindBoneIndex(Bone) == INDEX_NONE) { return false; }
    auto* Elite = Blueprint->GeneratedClass->GetDefaultObject<ASovAurelionElite>();
    auto* Core = Elite ? Elite->GetCoreWeakPoints() : nullptr;
    auto* Property = FindFProperty<FArrayProperty>(USovWeakPointComponent::StaticClass(), TEXT("WeakPointZones"));
    if (!Core || !Property) { return false; }
    auto* Zones = Property->ContainerPtrToValuePtr<TArray<FSovWeakPointZone>>(Core);
    Core->Modify(); Zones->Reset(); FSovWeakPointZone Zone; Zone.ZoneId = TEXT("Core"); Zone.HitBones.Add(Bone); Zones->Add(Zone);
    Blueprint->MarkPackageDirty(); return Core->HasValidWeakPointConfiguration();
}
