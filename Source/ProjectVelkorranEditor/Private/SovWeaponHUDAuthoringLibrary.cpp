// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovWeaponHUDAuthoringLibrary.h"
#include "WidgetBlueprint.h"
#include "Editor.h"
#include "Blueprint/UserWidget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_Message.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_MakeArray.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Items/WeaponItem.h"

namespace
{
    constexpr const TCHAR* OwnedPackage = TEXT("/Game/Aurelion/UI/WBP_WeaponInfo");
    constexpr const TCHAR* ClearNames[] = { TEXT("SovClearMainhand"), TEXT("SovClearOffhand"), TEXT("SovClearWeapons"), TEXT("SovClearDualWielding") };
    constexpr const TCHAR* Fields[] = { TEXT("MainhandWeapon"), TEXT("OffhandWeapon"), TEXT("OurWeapons"), TEXT("DualWielding") };

    UEdGraphPin* Pin(UEdGraphNode* Node, FName Name)
    { return Node ? Node->FindPin(Name) : nullptr; }
    bool Linked(UEdGraphPin* A, UEdGraphPin* B)
    { return A && B && A->LinkedTo.Contains(B) && B->LinkedTo.Contains(A); }
    bool OnlyLinked(UEdGraphPin* A, UEdGraphPin* B)
    { return Linked(A, B) && A->LinkedTo.Num() == 1; }

    template<class T> T* Named(UEdGraph* Graph, const TCHAR* Name)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        { if (Node && Node->GetFName() == FName(Name)) { return Cast<T>(Node); } }
        return nullptr;
    }

    FSovBlueprintAuthoringResult RepairGraph(UWidgetBlueprint* Blueprint)
    {
        FSovBlueprintAuthoringResult Result;
        auto Fail = [&Result](const TCHAR* Error) { Result.Report = Error; return Result; };
        if (!Blueprint || Blueprint->ParentClass != UUserWidget::StaticClass() || !Blueprint->GeneratedClass)
        { return Fail(TEXT("Requires the compiled original-layout UUserWidget copy.")); }
        const FObjectPropertyBase* OwnerProperty = FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, TEXT("Owner"));
        const FObjectPropertyBase* MainProperty = FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, TEXT("MainhandWeapon"));
        const FObjectPropertyBase* OffProperty = FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, TEXT("OffhandWeapon"));
        const FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(Blueprint->GeneratedClass, TEXT("OurWeapons"));
        const FObjectPropertyBase* ElementProperty = ArrayProperty ? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
        if (!OwnerProperty || OwnerProperty->PropertyClass != ANarrativeCharacter::StaticClass()
            || !MainProperty || MainProperty->PropertyClass != UWeaponItem::StaticClass()
            || !OffProperty || OffProperty->PropertyClass != UWeaponItem::StaticClass()
            || !ElementProperty || ElementProperty->PropertyClass != UWeaponItem::StaticClass()
            || !FindFProperty<FBoolProperty>(Blueprint->GeneratedClass, TEXT("DualWielding")))
        { return Fail(TEXT("Expected exact cached character/weapon fields are absent or incompatible.")); }
        UEdGraph* Graph = nullptr;
        for (UEdGraph* Candidate : Blueprint->UbergraphPages)
        { if (Candidate && Candidate->GetFName() == TEXT("EventGraph")) { Graph = Candidate; break; } }
        if (!Graph) { return Fail(TEXT("Expected EventGraph is absent.")); }
        auto* Construct = Named<UK2Node_Event>(Graph, TEXT("K2Node_Event_1"));
        auto* Tick = Named<UK2Node_Event>(Graph, TEXT("K2Node_Event_3"));
        auto* Resolve = Named<UK2Node_Message>(Graph, TEXT("K2Node_Message_1"));
        auto* OwningPlayer = Named<UK2Node_CallFunction>(Graph, TEXT("K2Node_CallFunction_1"));
        auto* Assign = Named<UK2Node_VariableSet>(Graph, TEXT("K2Node_VariableSet_0"));
        auto* Validate = Named<UK2Node_VariableGet>(Graph, TEXT("K2Node_VariableGet_0"));
        auto* Mainhand = Named<UK2Node_VariableSet>(Graph, TEXT("K2Node_VariableSet_1"));
        auto* Offhand = Named<UK2Node_VariableSet>(Graph, TEXT("K2Node_VariableSet_4"));
        auto* Weapons = Named<UK2Node_VariableSet>(Graph, TEXT("K2Node_VariableSet_2"));
        auto* Dual = Named<UK2Node_VariableSet>(Graph, TEXT("K2Node_VariableSet_3"));
        if (!Construct || Construct->EventReference.GetMemberName() != TEXT("Construct")
            || !Tick || Tick->EventReference.GetMemberName() != TEXT("Tick")
            || !Resolve || Resolve->FunctionReference.GetMemberName() != TEXT("GetNarrativeCharacter")
            || !OwningPlayer || OwningPlayer->FunctionReference.GetMemberName() != TEXT("GetOwningPlayer")
            || !OwningPlayer->FunctionReference.IsSelfContext()
            || !Assign || Assign->GetVarName() != TEXT("Owner")
            || !Validate || Validate->GetVarName() != TEXT("Owner")
            || !Mainhand || Mainhand->GetVarName() != TEXT("MainhandWeapon")
            || !Offhand || Offhand->GetVarName() != TEXT("OffhandWeapon")
            || !Weapons || Weapons->GetVarName() != TEXT("OurWeapons")
            || !Dual || Dual->GetVarName() != TEXT("DualWielding"))
        { return Fail(TEXT("Original weapon-widget graph identities changed; refusing a guessed repair.")); }
        const FName Then = UEdGraphSchema_K2::PN_Then, Exec = UEdGraphSchema_K2::PN_Execute;
        if (!OnlyLinked(Pin(Resolve, UEdGraphSchema_K2::PN_Self), Pin(OwningPlayer, UEdGraphSchema_K2::PN_ReturnValue))
            || !OnlyLinked(Pin(Construct, Then), Pin(Resolve, Exec))
            || !OnlyLinked(Pin(Resolve, Then), Pin(Assign, Exec))
            || !OnlyLinked(Pin(Assign, TEXT("Owner")), Pin(Resolve, UEdGraphSchema_K2::PN_ReturnValue))
            || !OnlyLinked(Pin(Validate, Then), Pin(Mainhand, Exec))
            || !OnlyLinked(Pin(Mainhand, Then), Pin(Offhand, Exec))
            || !OnlyLinked(Pin(Offhand, Then), Pin(Weapons, Exec))
            || !OnlyLinked(Pin(Weapons, Then), Pin(Dual, Exec)))
        { return Fail(TEXT("Original resolve/update dataflow changed; refusing a partial repair.")); }
        UEdGraphPin* Invalid = Pin(Validate, UEdGraphSchema_K2::PN_Else);
        if (!Invalid || !Pin(Tick, Then) || !Pin(Assign, Then) || !Pin(Validate, Exec))
        { return Fail(TEXT("Validated Owner or event execution pins are missing.")); }
        bool bAlreadyRebound = OnlyLinked(Pin(Tick, Then), Pin(Resolve, Exec))
            && OnlyLinked(Pin(Assign, Then), Pin(Validate, Exec));
        UK2Node_VariableSet* Clear[4] = {};
        auto* Empty = Named<UK2Node_MakeArray>(Graph, TEXT("SovEmptyWeapons"));
        for (int32 Index = 0; Index < 4; ++Index) { Clear[Index] = Named<UK2Node_VariableSet>(Graph, ClearNames[Index]); }
        if (bAlreadyRebound)
        {
            bool bClearValid = Empty && Empty->NumInputs == 0;
            for (int32 Index = 0; Index < 4; ++Index)
            {
                bClearValid &= Clear[Index] && Clear[Index]->GetVarName() == FName(Fields[Index])
                    && Pin(Clear[Index], FName(Fields[Index]));
                bClearValid &= OnlyLinked(Index ? Pin(Clear[Index - 1], Then) : Invalid, Pin(Clear[Index], Exec));
            }
            bClearValid &= Empty && OnlyLinked(Pin(Clear[2], TEXT("OurWeapons")), Empty->GetOutputPin());
            bClearValid &= Pin(Clear[0], TEXT("MainhandWeapon")) && Pin(Clear[0], TEXT("MainhandWeapon"))->LinkedTo.IsEmpty()
                && Pin(Clear[0], TEXT("MainhandWeapon"))->DefaultObject == nullptr;
            bClearValid &= Pin(Clear[1], TEXT("OffhandWeapon")) && Pin(Clear[1], TEXT("OffhandWeapon"))->LinkedTo.IsEmpty()
                && Pin(Clear[1], TEXT("OffhandWeapon"))->DefaultObject == nullptr;
            bClearValid &= Pin(Clear[3], TEXT("DualWielding")) && Pin(Clear[3], TEXT("DualWielding"))->LinkedTo.IsEmpty()
                && Pin(Clear[3], TEXT("DualWielding"))->DefaultValue == TEXT("false");
            if (!bClearValid) { return Fail(TEXT("Existing repair has modified clearing behavior; refusing to overwrite it.")); }
        }
        else
        {
            if (!OnlyLinked(Pin(Tick, Then), Pin(Validate, Exec)) || !Pin(Assign, Then)->LinkedTo.IsEmpty()
                || !Invalid->LinkedTo.IsEmpty() || Empty || Clear[0] || Clear[1] || Clear[2] || Clear[3])
            { return Fail(TEXT("Widget is neither the known vendor graph nor the complete owned repair.")); }
            const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
            Blueprint->Modify(); Graph->Modify();
            Tick->Modify(); Assign->Modify(); Validate->Modify(); Resolve->Modify();
            Pin(Tick, Then)->BreakLinkTo(Pin(Validate, Exec));
            if (!Schema->TryCreateConnection(Pin(Tick, Then), Pin(Resolve, Exec))
                || !Schema->TryCreateConnection(Pin(Assign, Then), Pin(Validate, Exec)))
            { return Fail(TEXT("Could not connect the current-owner refresh. Nothing was saved.")); }
            for (int32 Index = 0; Index < 4; ++Index)
            {
                Clear[Index] = NewObject<UK2Node_VariableSet>(Graph, FName(ClearNames[Index]), RF_Transactional);
                Graph->AddNode(Clear[Index], false, false);
                Clear[Index]->CreateNewGuid();
                Clear[Index]->VariableReference.SetSelfMember(FName(Fields[Index]));
                Clear[Index]->NodePosX = Validate->NodePosX + 300 + Index * 250;
                Clear[Index]->NodePosY = Validate->NodePosY + 300;
                Clear[Index]->AllocateDefaultPins();
                if (!Pin(Clear[Index], FName(Fields[Index]))
                    || !Schema->TryCreateConnection(Index ? Pin(Clear[Index - 1], Then) : Invalid, Pin(Clear[Index], Exec)))
                { return Fail(TEXT("Could not construct the empty-owner clearing branch. Nothing was saved.")); }
            }
            Pin(Clear[3], TEXT("DualWielding"))->DefaultValue = TEXT("false");
            Empty = NewObject<UK2Node_MakeArray>(Graph, TEXT("SovEmptyWeapons"), RF_Transactional);
            Graph->AddNode(Empty, false, false); Empty->CreateNewGuid(); Empty->NumInputs = 0;
            Empty->NodePosX = Clear[2]->NodePosX; Empty->NodePosY = Clear[2]->NodePosY + 180;
            Empty->AllocateDefaultPins();
            if (!Schema->TryCreateConnection(Empty->GetOutputPin(), Pin(Clear[2], TEXT("OurWeapons"))))
            { return Fail(TEXT("Could not supply the actual empty weapon array. Nothing was saved.")); }
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Log);
        Result.bSucceeded = Log.NumErrors == 0 && Blueprint->Status != BS_Error;
        Result.Report = FString::Printf(TEXT("%s; compile errors=%d warnings=%d. No assets saved."),
            bAlreadyRebound ? TEXT("Verified existing current-owner refresh and empty-owner clearing") : TEXT("Rebound each Construct/Tick and added explicit empty-owner clearing"),
            Log.NumErrors, Log.NumWarnings);
        return Result;
    }
}

FSovBlueprintAuthoringResult USovWeaponHUDAuthoringLibrary::RepairOwnedWeaponHUD(UObject* Asset)
{
    auto* Blueprint = Cast<UWidgetBlueprint>(Asset);
    if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || !IsValid(Blueprint)
        || Blueprint->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
        || Blueprint->GetOutermost()->GetName() != OwnedPackage)
    {
        FSovBlueprintAuthoringResult Result;
        Result.Report = TEXT("Only /Game/Aurelion/UI/WBP_WeaponInfo may be repaired.");
        return Result;
    }
    return RepairGraph(Blueprint);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EquipmentComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Framework/SovPlayerController.h"
#include "UObject/Script.h"
#include "UObject/StructOnScope.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    constexpr const TCHAR* VendorWidget = TEXT("/NarrativePro/Pro/Core/UI/Widgets/Weapons/WBP_WeaponInfo.WBP_WeaponInfo");
    UObject* CachedObject(UUserWidget* Widget, const TCHAR* Name)
    {
        const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Widget->GetClass(), Name);
        return Property ? Property->GetObjectPropertyValue_InContainer(Widget) : nullptr;
    }
    TArray<UWeaponItem*> CachedWeapons(UUserWidget* Widget)
    {
        const FArrayProperty* Property = FindFProperty<FArrayProperty>(Widget->GetClass(), TEXT("OurWeapons"));
        return Property ? *Property->ContainerPtrToValuePtr<TArray<UWeaponItem*>>(Widget) : TArray<UWeaponItem*>();
    }
    bool CachedDual(UUserWidget* Widget)
    {
        const FBoolProperty* Property = FindFProperty<FBoolProperty>(Widget->GetClass(), TEXT("DualWielding"));
        return Property && Property->GetPropertyValue_InContainer(Widget);
    }
    bool CallWidgetEvent(UUserWidget* Widget, FName Name)
    {
        UFunction* Function = Widget ? Widget->FindFunction(Name) : nullptr;
        if (!Function) { return false; }
        FStructOnScope Parameters(Function);
        Widget->ProcessEvent(Function, Parameters.GetStructMemory());
        return true;
    }
    bool FixtureWield(ANarrativeCharacter* Character, UWeaponItem* MainhandItem, UWeaponItem* OffhandItem)
    {
        UEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
        const FMapProperty* Property = Equipment ? FindFProperty<FMapProperty>(Equipment->GetClass(), TEXT("WieldedWeapons")) : nullptr;
        if (!Property) { return false; }
        // Fixture-only inventory arrangement; the compiled widget reads the real native equipment getters.
        auto* Map = Property->ContainerPtrToValuePtr<TMap<FGameplayTag, UWeaponItem*>>(Equipment);
        Map->Reset();
        if (MainhandItem) { Map->Add(FNarrativeGameplayTags::Get().Weapon_WieldSlot_Mainhand, MainhandItem); }
        if (OffhandItem) { Map->Add(FNarrativeGameplayTags::Get().Weapon_WieldSlot_Offhand, OffhandItem); }
        return true;
    }
    struct FWeaponHUDWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        ASovPlayerController* PC = nullptr;
        TStrongObjectPtr<ULocalPlayer> Local;
        FWeaponHUDWorld()
        {
            const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false)
                .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (!World || !GEngine) { return; }
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            PC = World->SpawnActor<ASovPlayerController>();
            if (PC)
            {
                World->AddController(PC);
                Local.Reset(NewObject<ULocalPlayer>(GEngine));
                PC->SetPlayer(Local.Get());
            }
        }
        ~FWeaponHUDWorld()
        {
            if (PC) { PC->Player = nullptr; }
            Local.Reset();
            if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeaponHUDSourceGuardTest,
    "ProjectVelkorran.Campaign.Frontend.WeaponHUDSourceScopeGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWeaponHUDSourceGuardTest::RunTest(const FString& Parameters)
{
    UWidgetBlueprint* Source = LoadObject<UWidgetBlueprint>(nullptr, VendorWidget);
    if (!TestNotNull(TEXT("Actual installed weapon widget source"), Source)) { return false; }
    const FString Before = USovBlueprintAuthoringLibrary::FingerprintBlueprint(Source);
    TestFalse(TEXT("Public helper refuses the vendor package"), USovWeaponHUDAuthoringLibrary::RepairOwnedWeaponHUD(Source).bSucceeded);
    TestEqual(TEXT("Refusal leaves every source persistent property unchanged"), USovBlueprintAuthoringLibrary::FingerprintBlueprint(Source), Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeaponHUDRebindingTest,
    "ProjectVelkorran.Campaign.Frontend.WeaponHUDRebindsCompiledChildAcrossPossession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWeaponHUDRebindingTest::RunTest(const FString& Parameters)
{
    UWidgetBlueprint* Source = LoadObject<UWidgetBlueprint>(nullptr, VendorWidget);
    if (!TestNotNull(TEXT("Actual installed weapon widget source"), Source)) { return false; }
    const FString SourceBefore = USovBlueprintAuthoringLibrary::FingerprintBlueprint(Source);
    TStrongObjectPtr<UPackage> Package(CreatePackage(*FString::Printf(TEXT("/Temp/SovWeaponHUD/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
    TStrongObjectPtr<UWidgetBlueprint> Copy(DuplicateObject<UWidgetBlueprint>(Source, Package.Get(), TEXT("WBP_WeaponInfo_Test")));
    if (!TestNotNull(TEXT("Unpersisted source duplicate"), Copy.Get())) { return false; }
    Copy->SetFlags(RF_Transient);
    const auto FirstRepair = RepairGraph(Copy.Get());
    if (!TestTrue(*FirstRepair.Report, FirstRepair.bSucceeded)) { return false; }
    const int32 NodeCount = Copy->UbergraphPages[0]->Nodes.Num();
    const auto Again = RepairGraph(Copy.Get());
    if (!TestTrue(TEXT("Complete repair is accepted idempotently"), Again.bSucceeded)) { return false; }
    TestEqual(TEXT("Repeated repair creates no duplicate graph nodes"), Copy->UbergraphPages[0]->Nodes.Num(), NodeCount);
    FWeaponHUDWorld F;
    if (!TestNotNull(TEXT("Local native Narrative controller"), F.PC)) { return false; }
    ASovPlayerCharacterBase* First = F.World->SpawnActor<ASovPlayerCharacterBase>();
    ASovPlayerCharacterBase* Second = F.World->SpawnActor<ASovPlayerCharacterBase>();
    if (!TestNotNull(TEXT("First actual pawn"), First) || !TestNotNull(TEXT("Replacement actual pawn"), Second)) { return false; }
    // The real save subsystem observes all spawns: use the project's ordinary stable identities.
    const FGuid ControllerGuid = F.PC->GetActorGUID_Implementation();
    const FGuid FirstGuid = First->GetActorGUID_Implementation();
    const FGuid SecondGuid = Second->GetActorGUID_Implementation();
    TestTrue(TEXT("Actual controller and both pawns publish stable actor identities"),
        ControllerGuid.IsValid() && FirstGuid.IsValid() && SecondGuid.IsValid());
    TestTrue(TEXT("Stable identity is per actual instance"),
        ControllerGuid != FirstGuid && ControllerGuid != SecondGuid && FirstGuid != SecondGuid);
    TestEqual(TEXT("Ordinary project getter preserves the first pawn identity"),
        First->GetActorGUID_Implementation(), FirstGuid);
    UWeaponItem* FirstMain = NewObject<UWeaponItem>(First);
    UWeaponItem* FirstOff = NewObject<UWeaponItem>(First);
    UWeaponItem* SecondMain = NewObject<UWeaponItem>(Second);
    if (!TestTrue(TEXT("Actual equipment getter fixture"), FixtureWield(First, FirstMain, FirstOff) && FixtureWield(Second, SecondMain, nullptr))) { return false; }
    F.PC->Possess(First);
    TStrongObjectPtr<UUserWidget> Widget(CreateWidget<UUserWidget>(F.PC, TSubclassOf<UUserWidget>(Copy->GeneratedClass.Get())));
    if (!TestNotNull(TEXT("Compiled source-layout widget"), Widget.Get())) { return false; }
    TestTrue(TEXT("Construct runs the real compiled graph"), CallWidgetEvent(Widget.Get(), TEXT("Construct")));
    TestTrue(TEXT("First owner cached"), CachedObject(Widget.Get(), TEXT("Owner")) == First);
    TestTrue(TEXT("First mainhand and offhand read from native equipment"), CachedObject(Widget.Get(), TEXT("MainhandWeapon")) == FirstMain && CachedObject(Widget.Get(), TEXT("OffhandWeapon")) == FirstOff);
    TestEqual(TEXT("Both wielded items projected"), CachedWeapons(Widget.Get()).Num(), 2);
    TestTrue(TEXT("Dual-wield presentation enabled"), CachedDual(Widget.Get()));
    F.PC->Possess(Second);
    TestTrue(TEXT("Ordinary possession updated the native character-owner interface"), F.PC->GetNarrativeCharacter() == Second);
    TestTrue(TEXT("One existing widget Tick executes"), CallWidgetEvent(Widget.Get(), TEXT("Tick")));
    TestTrue(TEXT("Same child now references the replacement pawn"), CachedObject(Widget.Get(), TEXT("Owner")) == Second);
    TestTrue(TEXT("Same child now displays replacement weapon"), CachedObject(Widget.Get(), TEXT("MainhandWeapon")) == SecondMain);
    TestNull(TEXT("Retired offhand cleared"), CachedObject(Widget.Get(), TEXT("OffhandWeapon")));
    TestTrue(TEXT("Retired weapon array replaced"), CachedWeapons(Widget.Get()) == TArray<UWeaponItem*>{SecondMain});
    TestFalse(TEXT("Retired dual-wield flag cleared"), CachedDual(Widget.Get()));
    // Losing the owning local player exercises the invalid branch that was unconnected in the vendor graph.
    Widget->SetPlayerContext(FLocalPlayerContext());
    TestTrue(TEXT("Invalid-owner Tick executes"), CallWidgetEvent(Widget.Get(), TEXT("Tick")));
    TestNull(TEXT("Missing owner removes the cached character"), CachedObject(Widget.Get(), TEXT("Owner")));
    TestNull(TEXT("Missing owner removes mainhand"), CachedObject(Widget.Get(), TEXT("MainhandWeapon")));
    TestNull(TEXT("Missing owner removes offhand"), CachedObject(Widget.Get(), TEXT("OffhandWeapon")));
    TestTrue(TEXT("Missing owner removes all item references"), CachedWeapons(Widget.Get()).IsEmpty());
    TestFalse(TEXT("Missing owner removes dual-wield flag"), CachedDual(Widget.Get()));
    Widget->SetOwningPlayer(F.PC);
    TestTrue(TEXT("Same child can reacquire a returning local owner"), CallWidgetEvent(Widget.Get(), TEXT("Tick")));
    TestTrue(TEXT("Replacement weapon returns without Construct/HUD recreation"), CachedObject(Widget.Get(), TEXT("MainhandWeapon")) == SecondMain);
    TestEqual(TEXT("Original Blueprint remains untouched after compiled duplicate execution"), USovBlueprintAuthoringLibrary::FingerprintBlueprint(Source), SourceBefore);
    Widget.Reset();
    return true;
}
#endif
