// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionStandOffAuthoringLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Tasks/BTTask_RunEQSQuery.h"
#include "Editor.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_OnCircle.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Pathfinding.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace SovStandOff
{
constexpr TCHAR QueryPackage[] = TEXT("/Game/Aurelion/Enemies/EQS_AurelionSecurityStandOff");
constexpr TCHAR StandOffQueryPath[] = TEXT("/Game/Aurelion/Enemies/EQS_AurelionSecurityStandOff.EQS_AurelionSecurityStandOff");
constexpr TCHAR StandOffTreePath[] = TEXT("/Game/Aurelion/Enemies/BT_AurelionSecurityCrossfire.BT_AurelionSecurityCrossfire");
constexpr TCHAR SourcePath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/EQS/EQS_Move_ToAttackTarget.EQS_Move_ToAttackTarget");
constexpr TCHAR ContextPath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/EQS/EQSContext_AttackTarget.EQSContext_AttackTarget_C");
constexpr TCHAR BusyPath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/EQS/EQS_Move_LookBusy.EQS_Move_LookBusy");
constexpr TCHAR CrossfirePath[] = TEXT("/Game/Aurelion/Enemies/EQS_AurelionSecurityCrossfire.EQS_AurelionSecurityCrossfire");
constexpr TCHAR StandOffStrafePath[] = TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/EQS/EQS_Move_RangedStrafe_GetClose.EQS_Move_RangedStrafe_GetClose");
constexpr TCHAR GuardName[] = TEXT("StandOffTargetRange");
constexpr float Radius = 1200.f, Spacing = 240.f, Minimum = 1000.f, Maximum = 3000.f;

FString Normalize(FString Value, const UObject* Root, const TCHAR* Token)
{
    Value.ReplaceInline(*Root->GetPathName(), Token);
    Value.ReplaceInline(*(Root->GetName() + TEXT(":")), *(FString(Token) + TEXT(":")));
    return Value;
}

FString Fields(const UObject* Object, const UObject* Root, const TCHAR* Token,
    const TFunction<bool(const FProperty*, int32, FString&)>& Override = {})
{
    FString Result;
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient)) { continue; }
        for (int32 Index = 0; Index < It->ArrayDim; ++Index)
        {
            FString Value;
            if (!Override || !Override(*It, Index, Value))
            { It->ExportText_InContainer(Index, Value, Object, nullptr, const_cast<UObject*>(Object), PPF_None); }
            Result += FString::Printf(TEXT("%s[%d]="), *It->GetName(), Index) + Normalize(Value, Root, Token) + TEXT("\n");
        }
    }
    return Result;
}

UEnvQueryGenerator_OnCircle* Circle(const UEnvQuery* Query)
{
    const auto& Options = Query->GetOptions();
    return Options.Num() == 1 && Options[0] ? Cast<UEnvQueryGenerator_OnCircle>(Options[0]->Generator) : nullptr;
}

int32 Candidates(const UEnvQueryGenerator_OnCircle* Gen)
{
    return FMath::CeilToInt(2.f * PI * Gen->CircleRadius.DefaultValue * Gen->ArcAngle.DefaultValue / 360.f
        / Gen->SpaceBetween.DefaultValue) + 1;
}

bool SourceShape(const UEnvQuery* Query, FString& Error)
{
    auto* Gen = Query ? Circle(Query) : nullptr;
    if (!Gen || Gen->GetClass() != UEnvQueryGenerator_OnCircle::StaticClass() || !Gen->IsIn(Query)
        || Query->GetQueryName() != Query->GetFName() || Gen->CircleRadius.DefaultValue != 250.f
        || Gen->SpaceBetween.DefaultValue != 50.f || Gen->ArcAngle.DefaultValue != 250.f || !Gen->bDefineArc
        || Gen->PointOnCircleSpacingMethod != EPointOnCircleSpacingMethod::BySpaceBetween
        || Gen->CircleRadius.DataBinding || Gen->SpaceBetween.DataBinding || Gen->ArcAngle.DataBinding
        || GetPathNameSafe(Gen->CircleCenter) != ContextPath || Candidates(Gen) != 23)
    { Error = TEXT("Stock radius250cm/spacing50cm/arc250degrees/context or generator identity changed."); return false; }
    const auto* Option = Query->GetOptions()[0];
    if (!Option->IsIn(Query) || Option->Tests.Num() != 2 || !Option->Tests[0] || !Option->Tests[1]
        || Option->Tests[0]->GetClass() != UEnvQueryTest_Pathfinding::StaticClass()
        || Option->Tests[0]->TestPurpose != EEnvTestPurpose::Filter
        || Option->Tests[1]->GetClass() != UEnvQueryTest_Distance::StaticClass()
        || Option->Tests[1]->TestPurpose != EEnvTestPurpose::Score
        || Option->Tests[1]->ScoringEquation != EEnvTestScoreEquation::InverseLinear)
    { Error = TEXT("Stock two-test path/filter and distance/score structure changed."); return false; }
    return true;
}

// Snapshot every persistent runtime property. EdGraph is the explicit authoring-only removal;
// QueryName is required to match the owning asset and is represented symbolically across copies.
FString QuerySnapshot(const UEnvQuery* Query, bool bPreserveSource)
{
    FString Result;
    const auto* Gen = Circle(Query);
    for (const UObject* Object : {static_cast<const UObject*>(Query), static_cast<const UObject*>(Query->GetOptions()[0]),
        static_cast<const UObject*>(Gen)})
    {
        Result += Normalize(Object->GetPathName(), Query, TEXT("$QUERY")) + TEXT("|") + Object->GetClass()->GetPathName() + TEXT("\n");
        Result += Fields(Object, Query, TEXT("$QUERY"), [&](const FProperty* Property, int32 Index, FString& Value)
        {
            const FName Name = Property->GetFName();
            if (Object == Query && Name == TEXT("EdGraph")) { Value = TEXT("$EDITOR_GRAPH_EXCLUDED"); return true; }
            if (Object == Query && Name == TEXT("QueryName")) { Value = TEXT("$QUERY_NAME"); return true; }
            if (Object == Query->GetOptions()[0] && Name == TEXT("Tests") && bPreserveSource)
            {
                // Same order and exact names for every original test; omit only the admitted new final guard.
                Value = TEXT("(");
                for (int32 I = 0; I < 2; ++I)
                { if (I) { Value += TEXT(","); } Value += Query->GetOptions()[0]->Tests[I]->GetPathName(); }
                Value += TEXT(")"); return true;
            }
            if (Object == Gen && bPreserveSource && (Name == TEXT("CircleRadius") || Name == TEXT("SpaceBetween")))
            {
                FAIDataProviderFloatValue Copy = Name == TEXT("CircleRadius") ? Gen->CircleRadius : Gen->SpaceBetween;
                Copy.DefaultValue = Name == TEXT("CircleRadius") ? 250.f : 50.f;
                Property->ExportTextItem_Direct(Value, &Copy, nullptr, const_cast<UObject*>(Object), PPF_None);
                return true;
            }
            return false;
        });
    }
    const auto& Tests = Query->GetOptions()[0]->Tests;
    for (int32 I = 0; I < Tests.Num(); ++I)
    {
        if (bPreserveSource && I == 2) { continue; }
        Result += Normalize(Tests[I]->GetPathName(), Query, TEXT("$QUERY")) + TEXT("|") + Tests[I]->GetClass()->GetPathName() + TEXT("\n");
        Result += Fields(Tests[I], Query, TEXT("$QUERY"));
    }
    return Result;
}

void ConfigureGuard(UEnvQueryTest_Distance* Guard, TSubclassOf<UEnvQueryContext> Context)
{
    Guard->TestOrder = 2;
    Guard->TestMode = EEnvTestDistance::Distance2D;
    Guard->DistanceTo = Context;
    Guard->TestPurpose = EEnvTestPurpose::Filter;
    Guard->FilterType = EEnvTestFilterType::Range;
    Guard->FloatValueMin.DefaultValue = Minimum;
    Guard->FloatValueMax.DefaultValue = Maximum;
    // Match the installed EQS node lifecycle before first save and in the comparison object.
    Guard->UpdateNodeVersion();
}

bool DerivedShape(const UEnvQuery* Source, const UEnvQuery* Query, FString& Error)
{
    const auto* Gen = Query ? Circle(Query) : nullptr;
    if (!Gen || Query->GetQueryName() != Query->GetFName() || Query->EdGraph || !Gen->IsIn(Query)
        || Gen->CircleRadius.DefaultValue != Radius || Gen->SpaceBetween.DefaultValue != Spacing
        || Gen->ArcAngle.DefaultValue != 250.f || !Gen->bDefineArc
        || Gen->PointOnCircleSpacingMethod != EPointOnCircleSpacingMethod::BySpaceBetween || Candidates(Gen) != 23
        || Query->GetOptions()[0]->Tests.Num() != 3)
    { Error = TEXT("Owned query radius/spacing/arc/count/editor graph differs."); return false; }
    const auto* Guard = Cast<UEnvQueryTest_Distance>(Query->GetOptions()[0]->Tests[2]);
    if (!Guard || Guard->GetClass() != UEnvQueryTest_Distance::StaticClass() || !Guard->IsIn(Query)
        || Guard->GetName() != GuardName)
    { Error = TEXT("Owned final range guard class/owner/name differs."); return false; }
    for (const UEnvQueryTest* Test : Query->GetOptions()[0]->Tests)
    { if (!Test || !Test->IsIn(Query)) { Error = TEXT("Query retains an external/null test."); return false; } }
    // Transient comparison object only. No setters are used on a loaded asset during inspection.
    auto* ExpectedGuard = NewObject<UEnvQueryTest_Distance>(GetTransientPackage());
    ConfigureGuard(ExpectedGuard, Circle(Source)->CircleCenter);
    if (Fields(Guard, Guard, TEXT("$GUARD")) != Fields(ExpectedGuard, ExpectedGuard, TEXT("$GUARD")))
    { Error = TEXT("Owned guard has an unapproved persistent property."); return false; }
    if (QuerySnapshot(Source, true) != QuerySnapshot(Query, true))
    { Error = TEXT("Original query properties changed beyond radius/spacing and one admitted guard."); return false; }
    return true;
}

UBTTask_RunEQSQuery* Fallback(UBehaviorTree* Tree, bool bInstalled, FString& Error)
{
    if (!Tree || !Tree->RootNode || Tree->BTGraph || !Tree->LastEditedDocuments.IsEmpty())
    { Error = TEXT("Requires the saved runtime-only owned tree; refusing an unexpected editable graph."); return nullptr; }
    UBTTask_RunEQSQuery* Found = nullptr; int32 Count = 0;
    TFunction<void(UBTCompositeNode*)> Visit = [&](UBTCompositeNode* Node)
    {
        if (!Node) { return; }
        if (Node->Children.Num() == 4)
        {
            const TCHAR* Paths[] = {BusyPath, CrossfirePath, StandOffStrafePath, bInstalled ? StandOffQueryPath : SourcePath};
            bool Match = true;
            for (int32 I = 0; I < 4; ++I)
            {
                const auto& Child = Node->Children[I]; const auto* Task = Cast<UBTTask_RunEQSQuery>(Child.ChildTask);
                Match &= Task && Task->GetClass() == UBTTask_RunEQSQuery::StaticClass()
                    && GetPathNameSafe(Task->EQSRequest.QueryTemplate) == Paths[I];
                if (I > 0) { Match &= Child.Decorators.IsEmpty() && Child.DecoratorOps.IsEmpty(); }
            }
            if (Match) { ++Count; Found = Cast<UBTTask_RunEQSQuery>(Node->Children[3].ChildTask); }
        }
        for (const auto& Child : Node->Children) { Visit(Child.ChildComposite); }
    };
    Visit(Tree->RootNode);
    if (Count != 1 || !Found || Found->GetName() != TEXT("BTTask_RunEQSQuery_3") || !Found->IsIn(Tree)
        || Found->EQSRequest.RunMode != EEnvQueryRunMode::RandomBest25Pct || !Found->EQSRequest.QueryConfig.IsEmpty()
        || Found->bUseBBKey || Found->EQSRequest.bUseBBKeyForQueryTemplate || !Found->bUpdateBBOnFail
        || Found->GetSelectedBlackboardKey() != TEXT("TargetLocation"))
    { Error = TEXT("Owned four-branch chooser or exact native fallback task contract differs."); return nullptr; }
    return Found;
}

FString TreeSnapshot(const UBehaviorTree* Tree, const UBTTask_RunEQSQuery* Changed, UEnvQuery* Source)
{
    FString Result = Fields(Tree, Tree, TEXT("$TREE"));
    TFunction<void(const UBTNode*)> Visit = [&](const UBTNode* Node)
    {
        if (!Node) { return; }
        Result += Normalize(Node->GetPathName(), Tree, TEXT("$TREE")) + TEXT("|") + Node->GetClass()->GetPathName() + TEXT("\n");
        Result += Fields(Node, Tree, TEXT("$TREE"), [&](const FProperty* Property, int32 Index, FString& Value)
        {
            if (Node == Changed && Property->GetFName() == TEXT("EQSRequest"))
            {
                FEQSParametrizedQueryExecutionRequest Copy = Changed->EQSRequest;
                Copy.QueryTemplate = Source;
                Property->ExportTextItem_Direct(Value, &Copy, nullptr, const_cast<UBTNode*>(Node), PPF_None);
                return true;
            }
            return false;
        });
        if (const auto* Composite = Cast<UBTCompositeNode>(Node))
        {
            for (const UBTService* Service : Composite->Services) { Visit(Service); }
            for (const auto& Child : Composite->Children)
            {
                for (const UBTDecorator* Decorator : Child.Decorators) { Visit(Decorator); }
                if (Child.ChildComposite) { Visit(Child.ChildComposite); } else { Visit(Child.ChildTask); }
            }
        }
        else if (const auto* Task = Cast<UBTTaskNode>(Node))
        { for (const UBTService* Service : Task->Services) { Visit(Service); } }
    };
    Visit(Tree->RootNode);
    for (const UBTDecorator* Decorator : Tree->RootDecorators) { Visit(Decorator); }
    return Result;
}
}

FSovAurelionStandOffInspection USovAurelionStandOffAuthoringLibrary::InspectStandOff(bool bRequireInstalled)
{
    using namespace SovStandOff;
    FSovAurelionStandOffInspection R; R.bInstalled = bRequireInstalled;
    if (!GEditor || GEditor->PlayWorld) { R.Error = TEXT("Requires editor outside PIE."); return R; }
    auto* Source = LoadObject<UEnvQuery>(nullptr, SourcePath);
    auto* Tree = LoadObject<UBehaviorTree>(nullptr, StandOffTreePath); R.Tree = Tree;
    if (!SourceShape(Source, R.Error)) { return R; }
    R.SourceQuerySnapshot = QuerySnapshot(Source, true);
    auto* Task = Fallback(Tree, bRequireInstalled, R.Error);
    if (!Task) { return R; }
    R.TreeSnapshot = TreeSnapshot(Tree, nullptr, Source);
    R.TreePreservedSnapshot = TreeSnapshot(Tree, bRequireInstalled ? Task : nullptr, Source);
    UEnvQuery* Query = FindObject<UEnvQuery>(nullptr, StandOffQueryPath);
    if (!Query && FPackageName::DoesPackageExist(QueryPackage)) { Query = LoadObject<UEnvQuery>(nullptr, StandOffQueryPath); }
    R.Query = Query;
    if (bRequireInstalled || Query)
    {
        if (!Query || !Circle(Query) || Query->GetOptions()[0]->Tests.Num() != 3)
        { R.Error = TEXT("Owned query missing or malformed."); return R; }
        for (const UEnvQueryTest* Test : Query->GetOptions()[0]->Tests)
        { if (!Test) { R.Error = TEXT("Owned query contains a null test."); return R; } }
        // Persist raw evidence before evaluating preservation.
        R.QuerySnapshot = QuerySnapshot(Query, false);
        R.QueryPreservedSnapshot = QuerySnapshot(Query, true);
        if (!DerivedShape(Source, Query, R.Error)) { return R; }
        auto* Gen = Circle(Query); R.RadiusCm = Gen->CircleRadius.DefaultValue;
        R.SpacingCm = Gen->SpaceBetween.DefaultValue; R.ArcDegrees = Gen->ArcAngle.DefaultValue;
        R.CandidatesPerContext = Candidates(Gen);
    }
    R.bSucceeded = true; return R;
}

FSovAurelionStandOffInspection USovAurelionStandOffAuthoringLibrary::AuthorStandOff()
{
    using namespace SovStandOff;
    FSovAurelionStandOffInspection Before = InspectStandOff(false);
    if (!Before.bSucceeded) { return Before; }
    if (Before.Query || FPackageName::DoesPackageExist(QueryPackage) || FindObject<UPackage>(nullptr, QueryPackage))
    { Before.bSucceeded = false; Before.Error = TEXT("Owned query destination must be absent in memory and on disk."); return Before; }
    auto* Source = LoadObject<UEnvQuery>(nullptr, SourcePath);
    auto* Query = Cast<UEnvQuery>(StaticDuplicateObject(Source, CreatePackage(QueryPackage), FPackageName::GetShortFName(QueryPackage)));
    Before.Query = Query;
    if (!Query) { Before.bSucceeded = false; Before.Error = TEXT("Query duplication failed."); return Before; }
    Query->EdGraph = nullptr; // Editor reconstructs its graph from the exact saved runtime option/tests.
    auto* Gen = Circle(Query); Gen->CircleRadius.DefaultValue = Radius; Gen->SpaceBetween.DefaultValue = Spacing;
    auto* Guard = NewObject<UEnvQueryTest_Distance>(Query, GuardName, RF_Transactional);
    ConfigureGuard(Guard, Gen->CircleCenter); Query->GetOptionsMutable()[0]->Tests.Add(Guard);
    if (!DerivedShape(Source, Query, Before.Error)) { Before.bSucceeded = false; return Before; }
    auto* Tree = Before.Tree.Get(); auto* Task = Fallback(Tree, false, Before.Error);
    if (!Task || QuerySnapshot(Source, true) != Before.SourceQuerySnapshot)
    { Before.bSucceeded = false; return Before; }
    const bool bOldDirty = Tree->GetOutermost()->IsDirty();
    Tree->Modify(false); Task->Modify(false); Task->EQSRequest.QueryTemplate = Query;
    auto After = InspectStandOff(true);
    if (!After.bSucceeded || After.TreePreservedSnapshot != Before.TreeSnapshot
        || After.SourceQuerySnapshot != Before.SourceQuerySnapshot)
    {
        Task->EQSRequest.QueryTemplate = Source;
        if (TreeSnapshot(Tree, nullptr, Source) == Before.TreeSnapshot) { Tree->GetOutermost()->SetDirtyFlag(bOldDirty); }
        else { After.Error += TEXT(" Original tree rollback equality failed; retain editor for review."); }
        After.bSucceeded = false;
        if (After.Error.IsEmpty()) { After.Error = TEXT("Unexpected original query/tree mutation during authoring."); }
        return After;
    }
    FAssetRegistryModule::AssetCreated(Query); Query->MarkPackageDirty(); Tree->MarkPackageDirty();
    return After;
}

FSovAurelionStandOffInspection USovAurelionStandOffAuthoringLibrary::RestoreStandOffBinding(const FString& ExpectedOriginalTreeSnapshot)
{
    using namespace SovStandOff;
    auto R = InspectStandOff(true);
    if (!R.bSucceeded || ExpectedOriginalTreeSnapshot.IsEmpty() || R.TreePreservedSnapshot != ExpectedOriginalTreeSnapshot)
    { R.bSucceeded = false; R.Error += TEXT(" Rollback requires exact recorded original tree equivalence."); return R; }
    auto* Source = LoadObject<UEnvQuery>(nullptr, SourcePath); auto* Task = Fallback(R.Tree, true, R.Error);
    if (!Task) { R.bSucceeded = false; return R; }
    Task->Modify(); Task->EQSRequest.QueryTemplate = Source; R.Tree->MarkPackageDirty();
    auto Restored = InspectStandOff(false);
    if (!Restored.bSucceeded || Restored.TreeSnapshot != ExpectedOriginalTreeSnapshot)
    { Restored.bSucceeded = false; Restored.Error += TEXT(" Restored full tree differs from original."); }
    return Restored;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStandOffPreservationTest,
    "ProjectVelkorran.Campaign.Aurelion.SecurityStandOff.QueryPreservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovStandOffPreservationTest::RunTest(const FString& Parameters)
{
    using namespace SovStandOff;
    auto* Source = LoadObject<UEnvQuery>(nullptr, SourcePath); FString Error;
    if (!TestTrue(TEXT("Exact installed stock query"), SourceShape(Source, Error))) { AddError(Error); return false; }
    const FString SourceBefore = QuerySnapshot(Source, true);
    auto* Query = Cast<UEnvQuery>(StaticDuplicateObject(Source, GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UEnvQuery::StaticClass())));
    Query->EdGraph = nullptr; auto* Gen = Circle(Query);
    Gen->CircleRadius.DefaultValue = Radius; Gen->SpaceBetween.DefaultValue = Spacing;
    auto* Guard = NewObject<UEnvQueryTest_Distance>(Query, GuardName); ConfigureGuard(Guard, Gen->CircleCenter);
    Query->GetOptionsMutable()[0]->Tests.Add(Guard);
    TestTrue(TEXT("Exact owned derivation admits"), DerivedShape(Source, Query, Error));
    const FString AuthoredSnapshot = QuerySnapshot(Query, false);
    const int32 AuthoredVersion = Guard->VerNum;
    Guard->SetFlags(RF_NeedPostLoad);
    Guard->ConditionalPostLoad();
    TestEqual(TEXT("Native post-load preserves the complete newly authored query"), QuerySnapshot(Query, false), AuthoredSnapshot);
    TestTrue(TEXT("Exact owned derivation still admits after native post-load"), DerivedShape(Source, Query, Error));
    // Reproduce the saved predecessor: its newly allocated guard used version zero.
    Guard->VerNum = 0;
    TestFalse(TEXT("Pre-migration guard version is not silently waived"), DerivedShape(Source, Query, Error));
    Guard->SetFlags(RF_NeedPostLoad);
    Guard->ConditionalPostLoad();
    TestEqual(TEXT("Legacy guard migrates through the installed node lifecycle"), Guard->VerNum, AuthoredVersion);
    TestEqual(TEXT("Legacy native migration changes no other query property"), QuerySnapshot(Query, false), AuthoredSnapshot);
    TestTrue(TEXT("Migrated guard admits under the same exact comparison"), DerivedShape(Source, Query, Error));
    Guard->VerNum = AuthoredVersion + 1;
    TestFalse(TEXT("Unapproved future guard version is rejected"), DerivedShape(Source, Query, Error));
    Guard->VerNum = AuthoredVersion;
    TestEqual(TEXT("23 candidates retained"), Candidates(Gen), 23);
    Gen->SpaceBetween.DefaultValue = 50.f;
    TestEqual(TEXT("Radius-only change would expand to106 candidates"), Candidates(Gen), 106);
    TestFalse(TEXT("Unbounded spacing drift rejected"), DerivedShape(Source, Query, Error));
    Gen->SpaceBetween.DefaultValue = Spacing;
    Query->GetOptionsMutable()[0]->Tests[1]->ScoringEquation = EEnvTestScoreEquation::Linear;
    TestFalse(TEXT("Changed old score rejected"), DerivedShape(Source, Query, Error));
    Query->GetOptionsMutable()[0]->Tests[1]->ScoringEquation = EEnvTestScoreEquation::InverseLinear;
    Guard->DistanceTo = nullptr;
    TestFalse(TEXT("Missing target context rejected"), DerivedShape(Source, Query, Error));
    Guard->DistanceTo = Gen->CircleCenter; Guard->FloatValueMin.DefaultValue = 999.f;
    TestFalse(TEXT("Reduced minimum rejected"), DerivedShape(Source, Query, Error));
    Guard->FloatValueMin.DefaultValue = Minimum; Guard->TestPurpose = EEnvTestPurpose::FilterAndScore;
    TestFalse(TEXT("Scoring rather than filter-only rejected"), DerivedShape(Source, Query, Error));
    Guard->TestPurpose = EEnvTestPurpose::Filter;
    TestTrue(TEXT("Restored derivation admits"), DerivedShape(Source, Query, Error));
    TestEqual(TEXT("Stock query remains exact"), QuerySnapshot(Source, true), SourceBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStandOffTreeDeltaTest,
    "ProjectVelkorran.Campaign.Aurelion.SecurityStandOff.TreeOnlyTemplateDelta", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovStandOffTreeDeltaTest::RunTest(const FString& Parameters)
{
    using namespace SovStandOff;
    auto* Actual = LoadObject<UBehaviorTree>(nullptr, StandOffTreePath);
    auto* Source = LoadObject<UEnvQuery>(nullptr, SourcePath);
    if (!TestNotNull(TEXT("Saved owned tree"), Actual) || !TestNotNull(TEXT("Stock query"), Source)) { return false; }
    auto* Tree = Cast<UBehaviorTree>(StaticDuplicateObject(Actual, GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBehaviorTree::StaticClass())));
    FString Error; auto* Task = Fallback(Tree, false, Error);
    if (!Task) { Error.Reset(); Task = Fallback(Tree, true, Error); }
    if (!TestNotNull(TEXT("Exact owned fallback in transient tree copy"), Task)) { AddError(Error); return false; }
    Task->EQSRequest.QueryTemplate = Source;
    const FString Before = TreeSnapshot(Tree, nullptr, Source);
    auto* OtherQuery = NewObject<UEnvQuery>(GetTransientPackage());
    Task->EQSRequest.QueryTemplate = OtherQuery;
    TestNotEqual(TEXT("Raw tree sees query binding change"), TreeSnapshot(Tree, nullptr, Source), Before);
    TestEqual(TEXT("Exactly query-template substitution normalizes"), TreeSnapshot(Tree, Task, Source), Before);
    Task->EQSRequest.RunMode = EEnvQueryRunMode::SingleResult;
    TestNotEqual(TEXT("Run-mode change is not waived"), TreeSnapshot(Tree, Task, Source), Before);
    Task->EQSRequest.RunMode = EEnvQueryRunMode::RandomBest25Pct;
    Task->bUpdateBBOnFail = false;
    TestNotEqual(TEXT("Failed-key policy change is not waived"), TreeSnapshot(Tree, Task, Source), Before);
    Task->bUpdateBBOnFail = true; Task->EQSRequest.QueryTemplate = Source;
    TestEqual(TEXT("Exact original tree restored in memory"), TreeSnapshot(Tree, nullptr, Source), Before);
    return true;
}
#endif
