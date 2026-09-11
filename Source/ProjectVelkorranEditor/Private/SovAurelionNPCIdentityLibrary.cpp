// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionNPCIdentityLibrary.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "UObject/Package.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AI/NPCDefinition.h"
#include "NarrativeStableActor.h"

namespace SovAurelionNPCIdentity
{
    const TCHAR* const Roles[] = { TEXT("SecurityDrone"), TEXT("Enforcer"), TEXT("ContaminatedDrone"),
        TEXT("Linkbound"), TEXT("WallRunner"), TEXT("Weaver"), TEXT("Elite") };
    FString RolePackage(const TCHAR* Role) { return FString(TEXT("/Game/Aurelion/Enemies/BP_Aurelion")) + Role; }
    FSovBlueprintAuthoringResult Fail(const TCHAR* Error)
    { FSovBlueprintAuthoringResult Result; Result.Report = Error; return Result; }
    template<class T> T* Named(UEdGraph* Graph, FName Name)
    {
        T* Result = nullptr;
        if (Graph) { for (UEdGraphNode* Node : Graph->Nodes)
        { if (Node && Node->GetFName() == Name) { if (Result) { return nullptr; } Result = Cast<T>(Node); } } }
        return Result;
    }
    UEdGraphPin* Pin(UEdGraphNode* Node, FName Name) { return Node ? Node->FindPin(Name) : nullptr; }
    bool OnlyLinked(const UEdGraphPin* From, const UEdGraphPin* To)
    { return From && To && From->LinkedTo.Num() == 1 && From->LinkedTo[0] == To && To->LinkedTo.Contains(From); }
    struct FBranch
    {
        UEdGraph* Graph = nullptr;
        UEdGraphPin* Return = nullptr;
        UEdGraphPin* Zero = nullptr;
        UEdGraphPin* Assigned = nullptr;
        bool bRepaired = false;
    };
    bool Inspect(UBlueprint* Blueprint, FBranch& Out)
    {
        if (!IsValid(Blueprint) || !Blueprint->GeneratedClass
            || !Blueprint->GeneratedClass->IsChildOf(ASovNPCCharacterBase::StaticClass())) { return false; }
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        { if (Graph && Graph->GetFName() == TEXT("GetActorGUID")) { if (Out.Graph) { return false; } Out.Graph = Graph; } }
        auto* Entry = Named<UK2Node_FunctionEntry>(Out.Graph, TEXT("K2Node_FunctionEntry_0"));
        auto* Validate = Named<UK2Node_MacroInstance>(Out.Graph, TEXT("K2Node_MacroInstance_0"));
        auto* Definition = Named<UK2Node_VariableGet>(Out.Graph, TEXT("K2Node_VariableGet_5"));
        auto* InvalidResult = Named<UK2Node_FunctionResult>(Out.Graph, TEXT("K2Node_FunctionResult_2"));
        auto* RepeatableResult = Named<UK2Node_FunctionResult>(Out.Graph, TEXT("K2Node_FunctionResult_4"));
        auto* Empty = Named<UK2Node_MakeStruct>(Out.Graph, TEXT("K2Node_MakeStruct_0"));
        auto* Spawn = Named<UK2Node_VariableGet>(Out.Graph, TEXT("K2Node_VariableGet_1"));
        auto* Break = Named<UK2Node_BreakStruct>(Out.Graph, TEXT("K2Node_BreakStruct_0"));
        if (!Entry || Entry->FunctionReference.GetMemberName() != TEXT("GetActorGUID")
            || !Validate || !Validate->GetMacroGraph()
            || Validate->GetMacroGraph()->GetPathName() != TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:IsValid")
            || !Definition || Definition->GetVarName() != TEXT("NPCDefinition") || !Definition->VariableReference.IsSelfContext()
            || !InvalidResult || InvalidResult->FunctionReference.GetMemberName() != TEXT("GetActorGUID")
            || !RepeatableResult || RepeatableResult->FunctionReference.GetMemberName() != TEXT("GetActorGUID")
            || !Empty || Empty->StructType != TBaseStructure<FGuid>::Get()
            || !Spawn || Spawn->GetVarName() != TEXT("SpawnInfo") || !Spawn->VariableReference.IsSelfContext()
            || !Break || Break->StructType != FNPCSpawnInfo::StaticStruct()) { return false; }
        Out.Return = Pin(InvalidResult, UEdGraphSchema_K2::PN_ReturnValue);
        Out.Zero = Pin(Empty, TEXT("Guid"));
        Out.Assigned = Pin(Break, TEXT("SpawnAssignedSaveGUID"));
        if (!OnlyLinked(Pin(Entry, UEdGraphSchema_K2::PN_Then), Pin(Validate, TEXT("exec")))
            || !OnlyLinked(Pin(Validate, TEXT("InputObject")), Pin(Definition, TEXT("NPCDefinition")))
            || !OnlyLinked(Pin(InvalidResult, UEdGraphSchema_K2::PN_Execute), Pin(Validate, TEXT("Is Not Valid")))
            || !OnlyLinked(Pin(Break, TEXT("NPCSpawnInfo")), Pin(Spawn, TEXT("SpawnInfo")))
            || !OnlyLinked(Pin(RepeatableResult, UEdGraphSchema_K2::PN_ReturnValue), Out.Assigned)
            || !Out.Return || !Out.Zero || !Out.Assigned) { return false; }
        // The original invalid branch is an unconfigured, unconnected-input zero GUID.
        for (const UEdGraphPin* Input : Empty->Pins)
        {
            if (Input && Input->Direction == EGPD_Input
                && (!Input->LinkedTo.IsEmpty() || Input->DefaultObject || !Input->DefaultTextValue.IsEmpty()
                    || (!Input->DefaultValue.IsEmpty() && Input->DefaultValue != TEXT("0")))) { return false; }
        }
        Out.bRepaired = OnlyLinked(Out.Return, Out.Assigned);
        return Out.bRepaired ? Out.Zero->LinkedTo.IsEmpty() && Out.Assigned->LinkedTo.Num() == 2
            : OnlyLinked(Out.Return, Out.Zero) && Out.Zero->LinkedTo.Num() == 1 && Out.Assigned->LinkedTo.Num() == 1;
    }
    FSovBlueprintAuthoringResult RepairGraph(UBlueprint* Blueprint)
    {
        FBranch Before;
        if (!Inspect(Blueprint, Before)) { return Fail(TEXT("Unknown GetActorGUID invalid-definition branch; no repair attempted.")); }
        if (!Before.bRepaired)
        {
            const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
            Blueprint->Modify(); Before.Graph->Modify();
            Before.Return->GetOwningNode()->Modify(); Before.Zero->GetOwningNode()->Modify(); Before.Assigned->GetOwningNode()->Modify();
            Before.Return->BreakLinkTo(Before.Zero);
            if (!Schema->TryCreateConnection(Before.Assigned, Before.Return))
            {
                Before.Return->BreakLinkTo(Before.Assigned); Before.Return->MakeLinkTo(Before.Zero);
                return Fail(TEXT("GUID pin connection was refused; original edge restored, nothing saved."));
            }
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Log);
        FBranch After;
        FSovBlueprintAuthoringResult Result;
        Result.bSucceeded = Log.NumErrors == 0 && Blueprint->Status != BS_Error && Inspect(Blueprint, After) && After.bRepaired;
        Result.Report = FString::Printf(TEXT("%s invalid-definition return only; compile errors=%d warnings=%d. No assets saved."),
            Before.bRepaired ? TEXT("Verified existing") : TEXT("Connected explicit spawn identity to"), Log.NumErrors, Log.NumWarnings);
        return Result;
    }
}

FSovBlueprintAuthoringResult USovAurelionNPCIdentityLibrary::RepairOwnedEarlyNPCIdentity(UBlueprint* Blueprint)
{
    using namespace SovAurelionNPCIdentity;
    bool bOwned = false;
    if (IsValid(Blueprint)) { for (const TCHAR* Role : Roles) { bOwned |= Blueprint->GetOutermost()->GetName() == RolePackage(Role); } }
    if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || !bOwned
        || Blueprint->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject))
    { return Fail(TEXT("Only the seven persistent /Game/Aurelion/Enemies/BP_Aurelion role copies may be repaired outside PIE.")); }
    return RepairGraph(Blueprint);
}


namespace SovAurelionNPCIdentity
{
    // Exact SHA256(namespace + LF + M12 package + LF + semantic label)[0:16].
    // Fixed reviewed census avoids adding a platform crypto implementation to this editor helper.
    struct FPlacedIdentitySpec { const TCHAR* Label; const TCHAR* Role; bool bStory; const TCHAR* GUID; };
    const FPlacedIdentitySpec PlacedIdentities[] = {
        { TEXT("Aurelion_E1.Drone1"), TEXT("SecurityDrone"), false, TEXT("8CED06724CBAA977DC8115095D6AA07E") },
        { TEXT("Aurelion_E1.Drone2"), TEXT("SecurityDrone"), false, TEXT("FC3D100D5047E4454681F8F287E313A4") },
        { TEXT("Aurelion_E1.Drone3"), TEXT("SecurityDrone"), false, TEXT("7A01977BBAB5FADBC53802423FE99079") },
        { TEXT("Aurelion_E1.Drone4"), TEXT("SecurityDrone"), false, TEXT("1143EA85C8561EA74496A1FE380C644E") },
        { TEXT("Aurelion_E1.Drone5"), TEXT("SecurityDrone"), false, TEXT("EE48EC2FB34D5983E4C88C2545F072E9") },
        { TEXT("Aurelion_E1.Drone6"), TEXT("SecurityDrone"), false, TEXT("D20FAE6E30E3A9A9C04FACC8E68F08BB") },
        { TEXT("Aurelion_E2.Enforcer1"), TEXT("Enforcer"), false, TEXT("08E8113BE53B87FB35BA183AE9693B0A") },
        { TEXT("Aurelion_E2.Enforcer2"), TEXT("Enforcer"), false, TEXT("AAE32ECF408616B1C5DF04CFA06E5488") },
        { TEXT("Aurelion_E2.Enforcer3"), TEXT("Enforcer"), false, TEXT("3F5B16923A6CD0A58DB1EE93E3438490") },
        { TEXT("Aurelion_E2.Enforcer4"), TEXT("Enforcer"), false, TEXT("FB95D87C66E508F88B5542C13F9E4909") },
        { TEXT("Aurelion_E2.Drone1"), TEXT("ContaminatedDrone"), false, TEXT("EA5F5B0EBAC1E6E975E5BE4507FAEC72") },
        { TEXT("Aurelion_E2.Drone2"), TEXT("ContaminatedDrone"), false, TEXT("D37208EEAD044B5592A91E2C96A27650") },
        { TEXT("Aurelion_E3.Linkbound1"), TEXT("Linkbound"), false, TEXT("DA956E6340BD2DC2922C45953666140B") },
        { TEXT("Aurelion_E3.Linkbound2"), TEXT("Linkbound"), false, TEXT("FB9C36D4F688D2CE7D44ADBA18F01CB1") },
        { TEXT("Aurelion_E3.Linkbound3"), TEXT("Linkbound"), false, TEXT("9C4000BD62B52F06DEF79FDB2851E87E") },
        { TEXT("Aurelion_E3.WallRunner"), TEXT("WallRunner"), false, TEXT("461CA1C660B1B4A89B859B7CA045B0E7") },
        { TEXT("Aurelion_E3.Linkbound4"), TEXT("Linkbound"), false, TEXT("F44E1FC02792C36684715219DC533656") },
        { TEXT("Aurelion_E3.Linkbound5"), TEXT("Linkbound"), false, TEXT("B4AD5F739A0B51EF4F1D573121893FD5") },
        { TEXT("Aurelion_E3.Weaver"), TEXT("Weaver"), false, TEXT("F5C07F9615CF9CA349E963DB2716D4C6") },
        { TEXT("Aurelion_E4.Linkbound1"), TEXT("Linkbound"), false, TEXT("F09CCDC5CA9F9B01F33AF4DF2256A35D") },
        { TEXT("Aurelion_E4.Linkbound2"), TEXT("Linkbound"), false, TEXT("DACE4229C469F182A5BFCC31703F24AC") },
        { TEXT("Aurelion_E4.Weaver"), TEXT("Weaver"), false, TEXT("68A083C407D77AD038CDD08CA6F63BEA") },
        { TEXT("Aurelion_E4.Elite"), TEXT("Elite"), false, TEXT("E029CA3EA28FD9704ECF29713CC5D6C2") },
        { TEXT("Aurelion_E4.WallRunner"), TEXT("WallRunner"), false, TEXT("099B5F6953466FEEE90D0CFB86F92772") },
        { TEXT("Aurelion_Cast_MeetingTarrik"), TEXT("MeetingTarrik"), true, TEXT("BF925EB7DA38FD7AEEB298DAA45E0192") },
        { TEXT("Aurelion_Cast_TrappedMarine"), TEXT("TrappedMarine"), true, TEXT("9A16833C29AF8C9584A0F0B59C42E206") },
        { TEXT("Aurelion_Cast_Lyric"), TEXT("Lyric"), true, TEXT("3E7768FA80067F39F447BC1BD2A151E1") },
        { TEXT("Aurelion_Cast_Malik"), TEXT("Malik"), true, TEXT("49E02D6D26FADBBDD44014AA06BAA5EF") },
        { TEXT("Aurelion_Cast_Tharne"), TEXT("Tharne"), true, TEXT("A82B5227F3ED874FDFD359C521E91FDD") },
        { TEXT("Aurelion_Cast_Lyessa"), TEXT("Lyessa"), true, TEXT("9B2AB1F1846B36CF8B62E593264B9E45") },
        { TEXT("Aurelion_Cast_WestDominionStretcher"), TEXT("WestDominionStretcher"), true, TEXT("A406BA667102DA35E15ABA563ECC7C88") },
        { TEXT("Aurelion_Cast_WestReformationStretcher"), TEXT("WestReformationStretcher"), true, TEXT("894D07893190DE33EAFC5FBC230BA3C7") },
        { TEXT("Aurelion_Cast_EastDominionWalker"), TEXT("EastDominionWalker"), true, TEXT("3CFC6014F645E89A8D8B57C716888911") },
        { TEXT("Aurelion_Cast_EastReformationWalker"), TEXT("EastReformationWalker"), true, TEXT("75C48BF5D020A19CA538AB5414E3743F") },
    };
    const FPlacedIdentitySpec* FindPlacedIdentity(const FString& Label)
    {
        for (const FPlacedIdentitySpec& Spec : PlacedIdentities) { if (Label == Spec.Label) { return &Spec; } }
        return nullptr;
    }
    bool OwnedIdentityAdmission(ASovNPCCharacterBase* NPC, const FGuid& ExpectedGUID)
    {
        if (!IsValid(NPC) || NPC->IsActorBeingDestroyed() || NPC->HasActorBegunPlay() || !NPC->GetWorld()
            || NPC->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
            || !NPC->ActorHasTag(TEXT("Sov.Aurelion.WorkPC.20260907")) || NPC->GetNPCDefinition()) { return false; }
        const FPlacedIdentitySpec* Spec = FindPlacedIdentity(NPC->GetActorLabel());
        FGuid RequiredGUID;
        if (!Spec || !FGuid::ParseExact(Spec->GUID, EGuidFormats::Digits, RequiredGUID)
            || !ExpectedGUID.IsValid() || ExpectedGUID != RequiredGUID) { return false; }
        const FString ClassPath = Spec->bStory ? TEXT("/Game/Aurelion/Characters/BP_AurelionStoryNPC.BP_AurelionStoryNPC_C")
            : RolePackage(Spec->Role) + TEXT(".BP_Aurelion") + Spec->Role + TEXT("_C");
        const FString DefinitionPackage = FString(Spec->bStory ? TEXT("/Game/Aurelion/Characters/NPC_Aurelion")
            : TEXT("/Game/Aurelion/Enemies/NPC_Aurelion")) + Spec->Role;
        const UNPCDefinition* Definition = NPC->GetAuthoredPlacedDefinition();
        if (NPC->GetClass()->GetPathName() != ClassPath || !IsValid(Definition) || !Definition->bAllowMultipleInstances
            || Definition->NPCClassPath.ToSoftObjectPath().ToString() != ClassPath
            || Definition->GetPathName() != DefinitionPackage + TEXT(".NPC_Aurelion") + Spec->Role) { return false; }
        const FNPCSpawnInfo& Info = NPC->GetEncounterSpawnInfo();
        if (Info.OwningSpawnerGUID.IsValid() || !Info.SpawnName.IsNone() || Info.OwningSpawn.IsValid()
            || (Info.SpawnAssignedSaveGUID.IsValid() && Info.SpawnAssignedSaveGUID != ExpectedGUID)) { return false; }
        // Even an exact label cannot authorize two actors in this world.
        for (TActorIterator<ASovNPCCharacterBase> It(NPC->GetWorld()); It; ++It)
        { if (*It != NPC && It->GetActorLabel() == Spec->Label) { return false; } }
        return true;
    }
}

FSovBlueprintAuthoringResult USovAurelionNPCIdentityLibrary::AssignOwnedPlacedNPCIdentity(ASovNPCCharacterBase* NPC, const FGuid& ExpectedGUID)
{
    using namespace SovAurelionNPCIdentity;
    UWorld* World = IsValid(NPC) ? NPC->GetWorld() : nullptr;
    if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || !World || World->WorldType != EWorldType::Editor
        || World != GEditor->GetEditorWorldContext().World()
        || World->GetOutermost()->GetName() != TEXT("/Game/Aurelion/Maps/L_Aurelion_M12")
        || NPC->GetLevel() != World->PersistentLevel || NPC->HasAnyFlags(RF_Transient))
    { return Fail(TEXT("Placed identity assignment requires an exact persistent actor in the current stopped-editor Aurelion M12 world.")); }
    return AssignIdentityToOwnedActor(NPC, ExpectedGUID);
}

FSovBlueprintAuthoringResult USovAurelionNPCIdentityLibrary::AssignIdentityToOwnedActor(ASovNPCCharacterBase* NPC, const FGuid& ExpectedGUID)
{
    using namespace SovAurelionNPCIdentity;
    if (!OwnedIdentityAdmission(NPC, ExpectedGUID)
        || (NPC->NativeSaveGuid.IsValid() && NPC->NativeSaveGuid != ExpectedGUID))
    { return Fail(TEXT("Placed identity refused: exact semantic label, class, definition, stamp, lifecycle, uniqueness or external ownership differs.")); }
    const FNPCSpawnInfo Before = NPC->SpawnInfo;
    const FGuid NativeBefore = NPC->NativeSaveGuid;
    if (!Before.SpawnAssignedSaveGUID.IsValid())
    {
        NPC->Modify();
        NPC->SpawnInfo.SpawnAssignedSaveGUID = ExpectedGUID;
    }
    FNPCSpawnInfo OtherAfter = NPC->SpawnInfo;
    OtherAfter.SpawnAssignedSaveGUID = Before.SpawnAssignedSaveGUID;
    FSovBlueprintAuthoringResult Result;
    Result.bSucceeded = OwnedIdentityAdmission(NPC, ExpectedGUID)
        && NPC->SpawnInfo.SpawnAssignedSaveGUID == ExpectedGUID && NPC->NativeSaveGuid == NativeBefore
        && FNPCSpawnInfo::StaticStruct()->CompareScriptStruct(&Before, &OtherAfter, PPF_None);
    Result.Report = Result.bSucceeded
        ? TEXT("Exact owned placed GUID assigned or already matching; all other SpawnInfo and native fallback identity preserved. No definition initialized or asset saved.")
        : TEXT("Placed identity readback or metadata preservation failed; do not save the authoring world.");
    return Result;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AI/NPCDefinition.h"
#include "NarrativeStableActor.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include "UObject/GarbageCollection.h"

namespace SovAurelionNPCIdentity
{
    FString Connections(UBlueprint* Blueprint, const FBranch* Ignore = nullptr)
    {
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        TArray<FString> Rows;
        for (UEdGraph* Graph : Graphs) { for (UEdGraphNode* Node : Graph->Nodes)
        {
            Rows.Add(Graph->GetName() + TEXT("/node/") + Node->GetName() + TEXT("/") + Node->GetClass()->GetPathName());
            for (UEdGraphPin* P : Node->Pins)
            {
                const FString Name = Graph->GetName() + TEXT("/") + Node->GetName() + TEXT("/") + P->PinName.ToString();
                Rows.Add(Name + TEXT("/default/") + P->DefaultValue + TEXT("/") + P->DefaultTextValue.ToString()
                    + TEXT("/") + GetPathNameSafe(P->DefaultObject));
                for (UEdGraphPin* Other : P->LinkedTo)
                {
                    if (Ignore && ((P == Ignore->Return && (Other == Ignore->Zero || Other == Ignore->Assigned))
                        || (Other == Ignore->Return && (P == Ignore->Zero || P == Ignore->Assigned)))) { continue; }
                    Rows.Add(Name + TEXT(" -> ") + Other->GetOwningNode()->GetName() + TEXT("/") + Other->PinName.ToString());
                }
            }
        } }
        Rows.Sort(); return FString::Join(Rows, TEXT("\n"));
    }
    bool StableSourceBaseline(FAutomationTestBase& Test, UBlueprint* Source, FString& Baseline)
    {
        // Compilation performs ordinary GC. A just-loaded Blueprint can still own
        // unreferenced node objects left by its load-time reconstruction. Prove
        // these are outside every saved graph before settling the fixture census.
        TArray<UObject*> Objects;
        GetObjectsWithOuter(Source->GetOutermost(), Objects, true);
        TSet<FString> UnreferencedNodePaths;
        for (UObject* Object : Objects)
        {
            auto* Node = Cast<UEdGraphNode>(Object);
            auto* Graph = Node ? Cast<UEdGraph>(Node->GetOuter()) : nullptr;
            if (!Graph) { continue; }
            const bool bKnownLoadOrphan = (Graph->GetFName() == TEXT("Check Can Play Tagged Dialogue")
                && (Node->GetFName() == TEXT("K2Node_BreakStruct_2") || Node->GetFName() == TEXT("K2Node_BreakStruct_3")))
                || (Graph->GetFName() == TEXT("EventGraph")
                    && (Node->GetFName() == TEXT("K2Node_CallFunction_3") || Node->GetFName() == TEXT("K2Node_CallFunction_4")));
            if (!bKnownLoadOrphan) { continue; }
            if (!Test.TestFalse(TEXT("Known reconstructed load node is absent from its persistent graph Nodes array"), Graph->Nodes.Contains(Node))) { return false; }
            UnreferencedNodePaths.Add(Node->GetPathName());
        }
        const FString GraphsBefore = Connections(Source);
        const FString BeforeGC = USovBlueprintAuthoringLibrary::FingerprintBlueprint(Source);
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        Baseline = USovBlueprintAuthoringLibrary::FingerprintBlueprint(Source);
        if (!Test.TestEqual(TEXT("Settling the source census leaves every reachable graph edge/default unchanged"), Connections(Source), GraphsBefore)) { return false; }
        // Preserve all source properties/CDO rows as well: the only permitted
        // baseline settlement is collection of the exact proved-unreferenced nodes.
        TArray<FString> BeforeRows, AfterRows;
        BeforeGC.ParseIntoArrayLines(BeforeRows, false); Baseline.ParseIntoArrayLines(AfterRows, false);
        TArray<FString> ExpectedRows;
        for (const FString& Row : BeforeRows)
        {
            FString Path, Remainder;
            const bool bCollectedKnownOrphan = Row.Split(TEXT("|"), &Path, &Remainder)
                && UnreferencedNodePaths.Contains(Path) && !AfterRows.Contains(Row);
            if (!bCollectedKnownOrphan) { ExpectedRows.Add(Row); }
        }
        return Test.TestEqual(TEXT("Pre-mutation GC preserves dirty flag and every reachable source/default property"),
            FString::Join(AfterRows, TEXT("\n")), FString::Join(ExpectedRows, TEXT("\n")));
    }
    struct FIdentityWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        FIdentityWorld()
        {
            const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
                .CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        }
        ~FIdentityWorld()
        { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
        ASovNPCCharacterBase* Spawn(UClass* Class)
        {
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            return World ? World->SpawnActor<ASovNPCCharacterBase>(Class, FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params) : nullptr;
        }
    };
    bool WriteFixtureMetadata(ASovNPCCharacterBase* NPC, FGuid Identity, UNPCDefinition* Definition = nullptr)
    {
        auto* Spawn = FindFProperty<FStructProperty>(ANarrativeNPCCharacter::StaticClass(), TEXT("SpawnInfo"));
        auto* Def = FindFProperty<FObjectPropertyBase>(ANarrativeNPCCharacter::StaticClass(), TEXT("NPCDefinition"));
        if (!NPC || !Spawn || Spawn->Struct != FNPCSpawnInfo::StaticStruct() || !Def) { return false; }
        Spawn->ContainerPtrToValuePtr<FNPCSpawnInfo>(NPC)->SpawnAssignedSaveGUID = Identity;
        // Test-only branch arrangement; deliberately does not start definition/appearance callbacks.
        Def->SetObjectPropertyValue_InContainer(NPC, Definition);
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEarlyNPCIdentityScopeTest,
    "ProjectVelkorran.Campaign.PlacedNPC.EarlyIdentityScopeAndUnknownGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEarlyNPCIdentityScopeTest::RunTest(const FString& Parameters)
{
    using namespace SovAurelionNPCIdentity;
    auto* Source = LoadObject<UBlueprint>(nullptr, *(RolePackage(TEXT("SecurityDrone")) + TEXT(".BP_AurelionSecurityDrone")));
    if (!TestNotNull(TEXT("Actual owned source-layout Blueprint"), Source)) { return false; }
    TStrongObjectPtr<UBlueprint> KeepSource(Source);
    FString SourceBefore;
    if (!StableSourceBaseline(*this, Source, SourceBefore)) { return false; }
    TStrongObjectPtr<UPackage> Package(CreatePackage(*FString::Printf(TEXT("/Temp/SovEarlyNPCIdentity/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
    TStrongObjectPtr<UBlueprint> Copy(DuplicateObject<UBlueprint>(Source, Package.Get(), TEXT("RejectedCopy")));
    if (!TestNotNull(TEXT("Unpersisted source duplicate"), Copy.Get())) { return false; }
    Copy->SetFlags(RF_Transient);
    TestFalse(TEXT("Public helper rejects a copy outside the exact owned whitelist"), USovAurelionNPCIdentityLibrary::RepairOwnedEarlyNPCIdentity(Copy.Get()).bSucceeded);
    FBranch Original;
    if (!TestTrue(TEXT("Actual original or idempotently repaired graph"), Inspect(Copy.Get(), Original))) { return false; }
    Original.Return->BreakAllPinLinks();
    const FString Broken = Connections(Copy.Get());
    TestFalse(TEXT("Unknown partial graph is rejected without guessing"), RepairGraph(Copy.Get()).bSucceeded);
    TestEqual(TEXT("Rejected graph has no additional changes"), Connections(Copy.Get()), Broken);
    TestEqual(TEXT("Source persistent state is unchanged"), USovBlueprintAuthoringLibrary::FingerprintBlueprint(Source), SourceBefore);
    auto* Story = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Aurelion/Characters/BP_AurelionStoryNPC.BP_AurelionStoryNPC"));
    if (!TestNotNull(TEXT("Actual shared story Blueprint"), Story)
        || !TestTrue(TEXT("Story class directly inherits the native stable NPC"), Story->ParentClass == ASovNPCCharacterBase::StaticClass())) { return false; }
    bool bStoryOverridesIdentity = false;
    for (UEdGraph* Graph : Story->FunctionGraphs) { bStoryOverridesIdentity |= Graph && Graph->GetFName() == TEXT("GetActorGUID"); }
    TestFalse(TEXT("Story class has no Blueprint identity override"), bStoryOverridesIdentity);
    FIdentityWorld F;
    auto* StoryNPC = F.Spawn(Story->GeneratedClass);
    const FGuid StoryIdentity = FGuid::NewGuid();
    if (!TestNotNull(TEXT("Uninitialized actual story instance"), StoryNPC)
        || !TestTrue(TEXT("Story explicit metadata fixture"), WriteFixtureMetadata(StoryNPC, StoryIdentity))) { return false; }
    TestFalse(TEXT("Story instance has not begun play"), StoryNPC->HasActorBegunPlay());
    TestNull(TEXT("Story definition remains uninitialized"), StoryNPC->GetNPCDefinition());
    TestEqual(TEXT("Unmodified native story getter already exposes explicit identity"), INarrativeStableActor::Execute_GetActorGUID(StoryNPC), StoryIdentity);
    StoryNPC->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEarlyNPCIdentityLookupTest,
    "ProjectVelkorran.Campaign.PlacedNPC.EarlyIdentityCompiledRolesAndNativeLookup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEarlyNPCIdentityLookupTest::RunTest(const FString& Parameters)
{
    using namespace SovAurelionNPCIdentity;
    for (const TCHAR* Role : Roles)
    {
        const FString PackageName = RolePackage(Role);
        auto* Source = LoadObject<UBlueprint>(nullptr, *(PackageName + TEXT(".BP_Aurelion") + Role));
        if (!TestNotNull(Role, Source)) { return false; }
        TStrongObjectPtr<UBlueprint> KeepSource(Source);
        FString SourceBefore;
        if (!StableSourceBaseline(*this, Source, SourceBefore)) { return false; }
        TStrongObjectPtr<UPackage> Package(CreatePackage(*FString::Printf(TEXT("/Temp/SovEarlyNPCIdentity/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
        TStrongObjectPtr<UBlueprint> Copy(DuplicateObject<UBlueprint>(Source, Package.Get(), TEXT("RoleCopy")));
        if (!TestNotNull(TEXT("Actual role copy"), Copy.Get())) { return false; }
        Copy->SetFlags(RF_Transient);
        FBranch Original;
        if (!TestTrue(TEXT("Known copied role branch"), Inspect(Copy.Get(), Original))) { return false; }
        // Always reproduce the previous graph in this transient copy, including after real assets were repaired.
        if (Original.bRepaired) { Original.Return->BreakLinkTo(Original.Assigned); Original.Return->MakeLinkTo(Original.Zero); }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy.Get());
        FCompilerResultsLog InitialCompile;
        FKismetEditorUtilities::CompileBlueprint(Copy.Get(), EBlueprintCompileOptions::SkipSave, &InitialCompile);
        if (!TestEqual(TEXT("Original copied graph compiles"), InitialCompile.NumErrors, 0)) { return false; }
        FIdentityWorld F;
        if (!TestNotNull(TEXT("Actual pre-BeginPlay world"), F.World)) { return false; }
        const FGuid Identity = FGuid::NewGuid();
        auto* BeforeNPC = F.Spawn(Copy->GeneratedClass);
        if (!TestNotNull(TEXT("Original compiled role instance"), BeforeNPC)
            || !TestTrue(TEXT("Fixture supplied explicit spawn identity"), WriteFixtureMetadata(BeforeNPC, Identity))) { return false; }
        TestFalse(TEXT("Original role has not begun play"), BeforeNPC->HasActorBegunPlay());
        TestNull(TEXT("No early NPC definition"), BeforeNPC->GetNPCDefinition());
        TestFalse(TEXT("Original Blueprint ignores valid spawn GUID before definition initialization"), INarrativeStableActor::Execute_GetActorGUID(BeforeNPC).IsValid());
        BeforeNPC->Destroy();
        FBranch Before;
        if (!TestTrue(TEXT("Original exact edges remain"), Inspect(Copy.Get(), Before))) { return false; }
        const FString OtherConnections = Connections(Copy.Get(), &Before);
        const auto Repair = RepairGraph(Copy.Get());
        if (!TestTrue(*Repair.Report, Repair.bSucceeded)) { return false; }
        FBranch After;
        if (!TestTrue(TEXT("Explicit identity is the sole invalid-definition return"), Inspect(Copy.Get(), After) && After.bRepaired)) { return false; }
        TestEqual(TEXT("Every other graph edge, node identity and pin default is unchanged"), Connections(Copy.Get(), &After), OtherConnections);
        const FString RepairedConnections = Connections(Copy.Get());
        TestTrue(TEXT("Repeated helper succeeds"), RepairGraph(Copy.Get()).bSucceeded);
        TestEqual(TEXT("Repeated repair changes no graph edge or node"), Connections(Copy.Get()), RepairedConnections);
        auto* NPC = F.Spawn(Copy->GeneratedClass);
        if (!TestNotNull(TEXT("Repaired compiled role instance"), NPC)
            || !TestTrue(TEXT("Explicit authored-style GUID supplied"), WriteFixtureMetadata(NPC, Identity))) { return false; }
        TestFalse(TEXT("Repaired role has not begun play"), NPC->HasActorBegunPlay());
        TestNull(TEXT("Repair does not publish an NPC definition"), NPC->GetNPCDefinition());
        TestEqual(TEXT("Actual compiled Blueprint returns explicit identity before BeginPlay"), INarrativeStableActor::Execute_GetActorGUID(NPC), Identity);
        auto* Saves = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
        if (!TestNotNull(TEXT("Actual native save owner"), Saves)) { return false; }
        // Same native interface/cache implementation used by the initial world scan; no gameplay startup.
        Saves->RefreshStableActorIdentity(NPC);
        TestTrue(TEXT("Native GUID lookup resolves the uninitialized actual role"), Saves->LookupActorByGUID(Identity) == NPC);
        TStrongObjectPtr<UNPCDefinition> Definition(NewObject<UNPCDefinition>());
        Definition->bAllowMultipleInstances = true;
        if (!TestTrue(TEXT("Arrange valid repeatable branch only"), WriteFixtureMetadata(NPC, Identity, Definition.Get()))) { return false; }
        TestEqual(TEXT("Existing valid repeatable branch still returns spawner GUID"), INarrativeStableActor::Execute_GetActorGUID(NPC), Identity);
        Definition->bAllowMultipleInstances = false;
        Definition->UniqueNPCGUID = FGuid::NewGuid();
        TestEqual(TEXT("Existing valid unique branch keeps definition identity"), INarrativeStableActor::Execute_GetActorGUID(NPC), Definition->UniqueNPCGUID);
        if (!TestTrue(TEXT("Arrange absent metadata"), WriteFixtureMetadata(NPC, FGuid()))) { return false; }
        TestFalse(TEXT("Missing definition and missing metadata still return invalid, never invented identity"), INarrativeStableActor::Execute_GetActorGUID(NPC).IsValid());
        WriteFixtureMetadata(NPC, Identity);
        NPC->Destroy();
        TestEqual(TEXT("Actual source asset stays unchanged"), USovBlueprintAuthoringLibrary::FingerprintBlueprint(Source), SourceBefore);
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCIdentityAssignmentTest,
    "ProjectVelkorran.Campaign.PlacedNPC.OwnedEditorAssignmentPreservesMetadataAndRefusesForeignIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPlacedNPCIdentityAssignmentTest::RunTest(const FString& Parameters)
{
    using namespace SovAurelionNPCIdentity;
    TStrongObjectPtr<UBlueprint> Story(LoadObject<UBlueprint>(nullptr,
        TEXT("/Game/Aurelion/Characters/BP_AurelionStoryNPC.BP_AurelionStoryNPC")));
    TStrongObjectPtr<UNPCDefinition> Definition(LoadObject<UNPCDefinition>(nullptr,
        TEXT("/Game/Aurelion/Characters/NPC_AurelionMeetingTarrik.NPC_AurelionMeetingTarrik")));
    if (!TestNotNull(TEXT("Actual owned story class"), Story.Get()) || !TestNotNull(TEXT("Actual owned repeatable definition"), Definition.Get())) { return false; }
    FIdentityWorld F;
    ASovNPCCharacterBase* NPC = F.Spawn(Story->GeneratedClass);
    if (!TestNotNull(TEXT("Actual isolated pre-BeginPlay story actor"), NPC)) { return false; }
    NPC->SetActorLabel(TEXT("Aurelion_Cast_MeetingTarrik"));
    NPC->Tags.AddUnique(TEXT("Sov.Aurelion.WorkPC.20260907"));
    NPC->AuthoredPlacedDefinition = Definition.Get();
    FGuid Expected;
    FGuid::ParseExact(TEXT("BF925EB7DA38FD7AEEB298DAA45E0192"), EGuidFormats::Digits, Expected);
    auto* Prop = FindFProperty<FStructProperty>(ANarrativeNPCCharacter::StaticClass(), TEXT("SpawnInfo"));
    if (!TestNotNull(TEXT("Actual native spawn metadata"), Prop)) { return false; }
    FNPCSpawnInfo& Info = *Prop->ContainerPtrToValuePtr<FNPCSpawnInfo>(NPC);
    Info.SpawnTransform = FTransform(FRotator(0.f, 31.f, 0.f), FVector(113.f, 227.f, 349.f), FVector(1.2f));
    Info.SpawnParams.bOverride_LevelRange = true;
    Info.SpawnParams.MinLevel = 3; Info.SpawnParams.MaxLevel = 7;
    const FNPCSpawnInfo Original = Info;
    TestFalse(TEXT("Public entry refuses a game fixture world without changing its identity"),
        USovAurelionNPCIdentityLibrary::AssignOwnedPlacedNPCIdentity(NPC, Expected).bSucceeded);
    TestTrue(TEXT("Public refusal preserves the entire struct"), FNPCSpawnInfo::StaticStruct()->CompareScriptStruct(&Original, &Info, PPF_None));
    const auto Assigned = USovAurelionNPCIdentityLibrary::AssignIdentityToOwnedActor(NPC, Expected);
    if (!TestTrue(*Assigned.Report, Assigned.bSucceeded)) { return false; }
    TestFalse(TEXT("Assignment starts no gameplay"), NPC->HasActorBegunPlay());
    TestNull(TEXT("Definition is still unpublished"), NPC->GetNPCDefinition());
    TestEqual(TEXT("Real native interface sees the authored identity before BeginPlay"), INarrativeStableActor::Execute_GetActorGUID(NPC), Expected);
    FNPCSpawnInfo Other = Info; Other.SpawnAssignedSaveGUID = Original.SpawnAssignedSaveGUID;
    TestTrue(TEXT("Complete nonidentity metadata survives, including transform and override data"),
        FNPCSpawnInfo::StaticStruct()->CompareScriptStruct(&Original, &Other, PPF_None));
    const FNPCSpawnInfo Once = Info;
    TestTrue(TEXT("Exact idempotent reauthoring succeeds"), USovAurelionNPCIdentityLibrary::AssignIdentityToOwnedActor(NPC, Expected).bSucceeded);
    TestTrue(TEXT("Idempotence preserves all metadata"), FNPCSpawnInfo::StaticStruct()->CompareScriptStruct(&Once, &Info, PPF_None));
    auto RefusesUnchanged = [&](const TCHAR* Reason)
    {
        const FNPCSpawnInfo Before = Info;
        TestFalse(Reason, USovAurelionNPCIdentityLibrary::AssignIdentityToOwnedActor(NPC, Expected).bSucceeded);
        TestTrue(TEXT("Refusal changes no spawn metadata"), FNPCSpawnInfo::StaticStruct()->CompareScriptStruct(&Before, &Info, PPF_None));
    };
    Info.SpawnAssignedSaveGUID = FGuid::NewGuid(); RefusesUnchanged(TEXT("Foreign supplied save identity is preserved"));
    Info.SpawnAssignedSaveGUID = FGuid();
    auto* NativeProp = FindFProperty<FStructProperty>(ASovNPCCharacterBase::StaticClass(), TEXT("NativeSaveGuid"));
    if (!TestNotNull(TEXT("Existing native fallback identity"), NativeProp)) { return false; }
    FGuid& NativeGUID = *NativeProp->ContainerPtrToValuePtr<FGuid>(NPC);
    const FGuid ExternalNative = FGuid::NewGuid(); NativeGUID = ExternalNative;
    RefusesUnchanged(TEXT("Foreign native fallback identity is preserved even when spawn identity is absent"));
    TestEqual(TEXT("Rejected native fallback was not rewritten"), NativeGUID, ExternalNative);
    NativeGUID = Expected;
    TestTrue(TEXT("Already matching native identity accepts the corresponding explicit spawn identity"),
        USovAurelionNPCIdentityLibrary::AssignIdentityToOwnedActor(NPC, Expected).bSucceeded);
    TestEqual(TEXT("Matching native fallback remains unchanged"), NativeGUID, Expected);
    TestEqual(TEXT("Matching spawn identity is actually assigned"), Info.SpawnAssignedSaveGUID, Expected);
    NativeGUID.Invalidate(); Info.SpawnAssignedSaveGUID.Invalidate();
    Info.OwningSpawnerGUID = FGuid::NewGuid(); RefusesUnchanged(TEXT("A real spawner owns its own metadata"));
    Info.OwningSpawnerGUID.Invalidate(); Info.SpawnName = TEXT("ExternalSpawn"); RefusesUnchanged(TEXT("Named spawner metadata is not overwritten"));
    Info.SpawnName = NAME_None; NPC->Tags.Remove(TEXT("Sov.Aurelion.WorkPC.20260907")); RefusesUnchanged(TEXT("Semantic label without ownership stamp is insufficient"));
    NPC->Tags.Add(TEXT("Sov.Aurelion.WorkPC.20260907")); NPC->SetActorLabel(TEXT("Aurelion_Cast_Lyric")); RefusesUnchanged(TEXT("Wrong label/definition/GUID tuple is refused"));
    NPC->SetActorLabel(TEXT("Aurelion_Cast_MeetingTarrik"));
    if (!TestTrue(TEXT("Arrange already initialized definition without asynchronous callbacks"), WriteFixtureMetadata(NPC, FGuid(), Definition.Get()))) { return false; }
    RefusesUnchanged(TEXT("Initialized actors cannot be assigned authoring identity"));
    WriteFixtureMetadata(NPC, Expected);
    NPC->Destroy();
    return true;
}
#endif
