// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionCrossfireInspectionLibrary.h"
#include "AI/SovAurelionCrossfireQuery.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCGoalItem.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "ArsenalSettings.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Tasks/BTTask_RunEQSQuery.h"
#include "Campaign/SovEncounterDirector.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "UObject/UnrealType.h"

namespace
{
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
constexpr TCHAR QueryPath[] = TEXT("/Game/Aurelion/Enemies/EQS_AurelionSecurityCrossfire.EQS_AurelionSecurityCrossfire");
constexpr TCHAR TreePath[] = TEXT("/Game/Aurelion/Enemies/BT_AurelionSecurityCrossfire.BT_AurelionSecurityCrossfire");
constexpr TCHAR ActivityPath[] = TEXT("/Game/Aurelion/Enemies/BPA_AurelionSecurityCrossfire.BPA_AurelionSecurityCrossfire");
constexpr TCHAR StockTreePath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/ShootAndStrafe/BT_Attack_Ranged.BT_Attack_Ranged");
constexpr TCHAR StockClassPath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/ShootAndStrafe/BPA_Attack_Ranged_Strafe.BPA_Attack_Ranged_Strafe_C");
}

FSovAurelionCrossfireAuthoringResult USovAurelionCrossfireInspectionLibrary::InspectSavedCrossfire()
{
    FSovAurelionCrossfireAuthoringResult R;
    if (!GEditor || GEditor->PlayWorld) { R.Error = TEXT("Cold asset inspection requires editor outside PIE."); return R; }
    auto* Stock = LoadObject<UBehaviorTree>(nullptr,StockTreePath);
    auto* Tree = LoadObject<UBehaviorTree>(nullptr,TreePath);
    auto* Query = LoadObject<UEnvQuery>(nullptr,QueryPath);
    auto* BP = LoadObject<UBlueprint>(nullptr,ActivityPath);
    auto* StockClass = LoadClass<UNPCActivity>(nullptr,StockClassPath);
    auto* Config = LoadObject<UNPCActivityConfiguration>(nullptr,TEXT("/Game/Aurelion/Enemies/AC_AurelionSecurityDrone.AC_AurelionSecurityDrone"));
    R.Query=Query; R.Tree=Tree;
    if (!Stock || !Tree || !Query || !BP || !Config || !StockClass || BP->ParentClass != StockClass || !BP->GeneratedClass)
    { R.Error=TEXT("Required current assets or inherited activity class are missing."); return R; }
    const auto& Options = Query->GetOptions();
    const auto* Gen = Options.Num()==1 && Options[0] ? Cast<UEnvQueryGenerator_SovCrossfire>(Options[0]->Generator) : nullptr;
    const auto* Test = Options.Num()==1 && Options[0] && Options[0]->Tests.Num()==1 ? Cast<UEnvQueryTest_SovCrossfire>(Options[0]->Tests[0]) : nullptr;
    const auto BoundsMatch = [](const FSovAurelionCrossfireBounds& B)
    { return B.EncounterId==TEXT("M12_E1_PressureHall") && B.Minimum==FVector(-8300,-17500,-25) && B.Maximum==FVector(-5700,-11900,225); };
    if (!Gen || Gen->GetClass()!=UEnvQueryGenerator_SovCrossfire::StaticClass() || !Test || Test->GetClass()!=UEnvQueryTest_SovCrossfire::StaticClass()
        || !BoundsMatch(Gen->Bounds) || !BoundsMatch(Test->Bounds) || Test->TestPurpose!=EEnvTestPurpose::FilterAndScore
        || Test->FilterType!=EEnvTestFilterType::Minimum || Test->ScoringEquation!=EEnvTestScoreEquation::Linear
        || Test->FloatValueMin.DefaultValue!=10.f || Test->ScoringFactor.DefaultValue!=1.f)
    { R.Error=TEXT("Cold query generator/test/bounds/scoring differs."); return R; }
    UBTTask_RunEQSQuery* Added=nullptr; int32 Count=0; bool CorrectPosition=false;
    TFunction<void(UBTCompositeNode*)> Visit = [&](UBTCompositeNode* Node)
    {
        if (!Node) { return; }
        for (int32 Index=0; Index<Node->Children.Num(); ++Index)
        {
            const auto& Child=Node->Children[Index]; auto* Task=Cast<UBTTask_RunEQSQuery>(Child.ChildTask);
            if (Task && Task->EQSRequest.QueryTemplate==Query)
            {
                ++Count; Added=Task;
                CorrectPosition=Index==1 && Node->Children.Num()==4 && Child.Decorators.IsEmpty() && Child.DecoratorOps.IsEmpty();
            }
            Visit(Child.ChildComposite);
        }
    };
    Visit(Tree->RootNode);
    if (Count!=1 || !CorrectPosition || !Added || Added->GetClass()!=UBTTask_RunEQSQuery::StaticClass()
        || Added->bUpdateBBOnFail || Added->bUseBBKey || Added->EQSRequest.bUseBBKeyForQueryTemplate
        || Added->EQSRequest.RunMode!=EEnvQueryRunMode::SingleResult || !Added->EQSRequest.QueryConfig.IsEmpty()
        || Added->GetSelectedBlackboardKey()!=GetDefault<UArsenalSettings>()->BBKey_TargetLocation)
    { R.Error=TEXT("Cold optional-query branch or its stock task/output/failure policy differs."); return R; }
    R.SourceRuntimeNodes=RuntimeSnapshot(Stock); R.PreservedRuntimeNodes=RuntimeSnapshot(Tree,Added);
    if (R.SourceRuntimeNodes!=R.PreservedRuntimeNodes)
    { R.Error=TEXT("Cold original stock nodes, fallbacks, services, decorators or movement differ."); return R; }
    auto* Field=FindFProperty<FObjectPropertyBase>(UNPCActivity::StaticClass(),TEXT("BehaviourTree"));
    int32 OwnCount=0, StockCount=0;
    for (const TSubclassOf<UNPCActivity>& Entry : Config->DefaultActivities)
    { OwnCount += Entry.Get()==BP->GeneratedClass; StockCount += Entry.Get()==StockClass; }
    if (!Field || Field->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject())!=Tree
        || OwnCount!=1 || StockCount!=0)
    { R.Error=TEXT("Cold activity tree default or single configuration entry differs."); return R; }
    R.bSucceeded=true; return R;
}

FSovCrossfireRuntimeReadback USovAurelionCrossfireInspectionLibrary::ReadCurrentCrossfire(ANarrativeNPCController* Controller)
{
    FSovCrossfireRuntimeReadback R;
    auto* Pawn = IsValid(Controller) ? Cast<ASovAurelionSecurityDrone>(Controller->GetPawn()) : nullptr;
    auto* World = IsValid(Pawn) ? Pawn->GetWorld() : nullptr;
    const auto* ASC = IsValid(Pawn) ? Pawn->GetNarrativeAbilitySystemComponent() : nullptr;
    if (!World || !World->IsGameWorld() || Pawn->IsActorBeingDestroyed() || Pawn->GetController()!=Controller
        || !IsValid(ASC) || ASC->GetAvatarActor()!=Pawn)
    { R.UnavailableReason=TEXT("No current native Security Drone controller/avatar in a game world."); return R; }
    const uint64 Generation=Controller->GetPawnAssignmentGeneration(), Epoch=ASC->GetCombatActorInfoEpoch();
    R.Pawn=Pawn->GetPathName(); R.Controller=Controller->GetPathName(); R.GameSeconds=World->GetTimeSeconds(); R.PawnLocation=Pawn->GetActorLocation();
    for (TActorIterator<ASovEncounterDirector> It(World); It; ++It)
    {
        if (!It->FindParticipantId(Pawn).IsNone())
        {
            if (!R.Encounter.IsEmpty() || It->EncounterId!=TEXT("M12_E1_PressureHall"))
            { R.UnavailableReason=TEXT("No unique current E1 encounter owner."); return R; }
            R.Encounter=It->GetPathName(); R.Attempt=It->GetAttemptId();
        }
    }
    if (R.Encounter.IsEmpty()) { R.UnavailableReason=TEXT("Security Drone is not registered to E1."); return R; }
    if (const auto* BB=Controller->GetBlackboardComponent())
    {
        const auto* Settings=GetDefault<UArsenalSettings>(); R.AttackTarget=GetPathNameSafe(BB->GetValueAsObject(Settings->BBKey_AttackTarget));
        R.bHasBlackboardDestination=BB->IsVectorValueSet(BB->GetKeyID(Settings->BBKey_TargetLocation));
        if (R.bHasBlackboardDestination) { R.BlackboardDestination=BB->GetValueAsVector(Settings->BBKey_TargetLocation); }
    }
    if (const auto* Activities=Controller->GetActivityComponent())
    { R.Activity=GetPathNameSafe(Activities->GetCurrentActivity()); R.Goal=GetPathNameSafe(Activities->GetCurrentActivityGoal()); }
    if (const auto* Paths=Controller->GetPathFollowingComponent())
    { R.MoveRequestId=Paths->GetCurrentRequestId().GetID(); R.MoveStatus=Paths->GetStatus(); R.PathDestination=Paths->GetPathDestination(); }
#if USE_EQS_DEBUGGER
    if (auto* Manager=UEnvQueryManager::GetCurrent(World))
    {
        TSet<int32> Seen;
        for (const UObject* Owner : {static_cast<UObject*>(Pawn),static_cast<UObject*>(Controller)})
        {
            // This stock debugger accessor may create an empty debug-cache owner entry.
            // It does not enable logging, execute a query or change gameplay state.
            for (const auto& Info : Manager->GetDebugger().GetAllQueriesForOwner(Owner))
            {
                const auto& Instance=Info.Instance;
                if (!Instance.IsValid() || Instance->World!=World || Instance->Owner.Get()!=Owner || Seen.Contains(Instance->QueryID)) { continue; }
                Seen.Add(Instance->QueryID); FSovCrossfireQueryReadback Row;
                Row.QueryId=Instance->QueryID; Row.QueryName=Instance->QueryName; Row.QueryOwner=GetPathNameSafe(Instance->Owner.Get());
                Row.CompletedAt=Info.Timestamp; Row.ExecutionMilliseconds=Instance->TotalExecutionTime*1000.;
                Row.bFinished=Instance->IsFinished(); Row.bSucceeded=Instance->IsSuccessful(); Row.bAborted=Instance->IsAborted(); Row.NativeStatus=Instance->GetRawStatus();
                if (Instance->ItemTypeVectorCDO)
                {
                    for (int32 Index=0; Index<Instance->Items.Num() && Row.ResultLocations.Num()<32; ++Index)
                    { if (Instance->Items[Index].IsValid()) { Row.ResultLocations.Add(Instance->GetItemAsLocation(Index)); Row.ResultScores.Add(Instance->GetItemScore(Index)); } }
                }
                R.Queries.Add(MoveTemp(Row));
            }
        }
        R.bQueryHistoryAvailable=!R.Queries.IsEmpty();
        if (!R.bQueryHistoryAvailable) { R.UnavailableReason=TEXT("Native EQS debugger has no retained result for this current owner."); }
    }
    else { R.UnavailableReason=TEXT("No native EQS manager in this world."); }
#else
    R.UnavailableReason=TEXT("USE_EQS_DEBUGGER is not compiled in this target.");
#endif
    R.bCurrentOwner=IsValid(Pawn) && IsValid(Controller) && Pawn->GetController()==Controller && Controller->GetPawn()==Pawn
        && Controller->GetPawnAssignmentGeneration()==Generation && Pawn->GetNarrativeAbilitySystemComponent()==ASC
        && ASC->GetAvatarActor()==Pawn && ASC->GetCombatActorInfoEpoch()==Epoch;
    if (!R.bCurrentOwner) { R.Queries.Reset(); R.bQueryHistoryAvailable=false; R.UnavailableReason=TEXT("Native owner changed during readback."); }
    return R;
}
