// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovCombatVitalsWidget.h"
#include "Tests/SovReadinessRuntimeTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace
{
    struct FVitalsWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        ASovHandoffRuntimeTestController* PC = nullptr;
        ASovPlayerState* PS = nullptr;
        FVitalsWorld()
        {
            const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
                .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
            // ULocalPlayer resolves its controller through this world's registered controller list.
            // These transient worlds do not run normal actor PostInitializeComponents.
            if (PC) { World->AddController(PC); }
            PS = World->SpawnActor<ASovPlayerState>();
            if (PC && PS) { PC->SetTestPlayerState(PS); }
        }
        ~FVitalsWorld()
        { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
        ASovReadinessRuntimeTestPawn* StagePawn()
        {
            if (!PC || !PS) { return nullptr; }
            auto* Pawn = World->SpawnActor<ASovReadinessRuntimeTestPawn>();
            auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            if (!Pawn || !Pawn->PrepareCampaignInitialization(Definition)) { return nullptr; }
            PC->Possess(Pawn);
            if (!Pawn->StageTestReadiness(PS, true)) { return nullptr; }
            Pawn->BindProductionReadiness();
            return Pawn;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatVitalsHandoffTest,
    "ProjectVelkorran.Campaign.Frontend.CombatVitalsFollowOnlyReadyCurrentPawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCombatVitalsHandoffTest::RunTest(const FString& Parameters)
{
    FVitalsWorld F;
    auto* First = F.StagePawn();
    if (!TestNotNull(TEXT("Staged first pawn"), First)) { return false; }
    FSovCombatVitalsSnapshot View;
    TestFalse(TEXT("Incomplete initial data cannot appear in combat readout"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestTrue(TEXT("First pawn crosses production readiness"), First->CompleteCampaignDataInitialization(false));
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(F.PS->GetAbilitySystemComponent());
    if (!TestNotNull(TEXT("PlayerState ASC"), ASC)) { return false; }
    const FGameplayAttribute Attributes[] = {UNarrativeAttributeSetBase::GetHealthAttribute(),
        UNarrativeAttributeSetBase::GetShieldAttribute(), UNarrativeAttributeSetBase::GetStaminaAttribute(),
        UNarrativeAttributeSetBase::GetPoiseAttribute(), UNarrativeAttributeSetBase::GetEchoAttribute()};
    const float Values[] = {41.f, 23.f, 60.f, 17.f, 78.f};
    for (int32 Index = 0; Index < 5; ++Index) { ASC->SetNumericAttributeBase(Attributes[Index], Values[Index]); }
    TestTrue(TEXT("Ready current pawn projects all five resources"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestTrue(TEXT("Snapshot names the current pawn"), View.Pawn.Get() == First);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        TestEqual(FString::Printf(TEXT("Resource %d reads its canonical value"), Index), View.Values[Index].Current, Values[Index]);
        TestEqual(FString::Printf(TEXT("Projection cannot write resource %d"), Index), ASC->GetNumericAttribute(Attributes[Index]), Values[Index]);
    }
    auto* Second = F.StagePawn();
    if (!TestNotNull(TEXT("Replacement pawn"), Second)) { return false; }
    TestFalse(TEXT("Handoff to an unready pawn hides the old display"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestFalse(TEXT("Hidden view does not retain the old pawn"), View.Pawn.IsValid());
    TestEqual(TEXT("Hidden view clears old resource values"), View.Values[4].Current, 0.f);
    TestTrue(TEXT("Replacement completes its own production readiness"), Second->CompleteCampaignDataInitialization(false));
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 12.f);
    TestTrue(TEXT("Readout follows the replacement after readiness"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestTrue(TEXT("Readout never pins the outgoing pawn"), View.Pawn.Get() == Second);
    TestEqual(TEXT("Replacement resource is projected without old-value carryover"), View.Values[4].Current, 12.f);
    ASC->InitAbilityActorInfo(F.PS, First);
    TestFalse(TEXT("An ASC/avatar mismatch hides even a previously ready pawn"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestFalse(TEXT("Retired binding clears the projected owner"), View.Pawn.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatVitalsHUDHideTest,
    "ProjectVelkorran.Campaign.Frontend.CombatVitalsHonorsCountedHUDHideRequests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCombatVitalsHUDHideTest::RunTest(const FString& Parameters)
{
    FVitalsWorld F;
    auto* Pawn = F.StagePawn();
    if (!TestNotNull(TEXT("Staged current pawn"), Pawn)
        || !TestTrue(TEXT("Pawn reaches production readiness"), Pawn->CompleteCampaignDataInitialization(false))) { return false; }
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(F.PS->GetAbilitySystemComponent());
    if (!TestNotNull(TEXT("Current player ASC"), ASC)) { return false; }
    const FGameplayAttribute Attributes[] = {UNarrativeAttributeSetBase::GetHealthAttribute(),
        UNarrativeAttributeSetBase::GetShieldAttribute(), UNarrativeAttributeSetBase::GetStaminaAttribute(),
        UNarrativeAttributeSetBase::GetPoiseAttribute(), UNarrativeAttributeSetBase::GetEchoAttribute()};
    const float Values[] = {41.f, 23.f, 60.f, 17.f, 78.f};
    for (int32 Index = 0; Index < 5; ++Index) { ASC->SetNumericAttributeBase(Attributes[Index], Values[Index]); }
    FSovCombatVitalsSnapshot View;
    TestTrue(TEXT("Ready pawn projects before a hide request"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    const auto CheckResources = [&]()
    {
        for (int32 Index = 0; Index < 5; ++Index)
        { TestEqual(FString::Printf(TEXT("HUD visibility never changes resource %d"), Index), ASC->GetNumericAttribute(Attributes[Index]), Values[Index]); }
    };
    const auto CheckHidden = [&]()
    {
        TestFalse(TEXT("Owned HUD-hide request suppresses the resource projection"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
        TestFalse(TEXT("Hidden projection clears its pawn"), View.Pawn.IsValid());
        for (int32 Index = 0; Index < 5; ++Index)
        {
            TestEqual(TEXT("Hidden projection clears every current value"), View.Values[Index].Current, 0.f);
            TestEqual(TEXT("Hidden projection clears every maximum value"), View.Values[Index].Maximum, 0.f);
        }
        CheckResources();
    };
    const FGameplayTag Hide = FNarrativeGameplayTags::Get().State_Player_WantsHideHUD;
    const FGameplayTag HideAll = FNarrativeGameplayTags::Get().State_Player_WantsHideHUD_All;
    ASC->AddLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    CheckHidden();
    ASC->AddLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    TestEqual(TEXT("Two independent owners retain two hide counts"), ASC->GetTagCount(Hide), 2);
    ASC->RemoveLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    TestEqual(TEXT("Releasing one owner preserves the other request"), ASC->GetTagCount(Hide), 1);
    CheckHidden();
    ASC->RemoveLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    TestTrue(TEXT("Projection resumes when the last ordinary hide owner releases"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    ASC->AddLooseGameplayTag(HideAll, 1, EGameplayTagReplicationState::CountToOwner);
    CheckHidden();
    ASC->RemoveLooseGameplayTag(HideAll, 1, EGameplayTagReplicationState::CountToOwner);
    TestTrue(TEXT("Projection resumes after the all-HUD request ends"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestTrue(TEXT("Resumed view belongs to the same current pawn"), View.Pawn.Get() == Pawn);
    for (int32 Index = 0; Index < 5; ++Index)
    { TestEqual(TEXT("Resumed view displays the preserved resource"), View.Values[Index].Current, Values[Index]); }
    CheckResources();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatVitalsContextLayoutTest,
    "ProjectVelkorran.Campaign.Frontend.CombatVitalsContextualStaminaAndEchoLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCombatVitalsContextLayoutTest::RunTest(const FString& Parameters)
{
    FVitalsWorld F;
    auto* Pawn = F.StagePawn();
    if (!TestNotNull(TEXT("Staged current pawn"), Pawn)
        || !TestTrue(TEXT("Current pawn is ready"), Pawn->CompleteCampaignDataInitialization(false))) { return false; }
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(F.PS->GetAbilitySystemComponent());
    if (!TestNotNull(TEXT("Current ASC"), ASC)) { return false; }
    const TStrongObjectPtr<ULocalPlayer> LocalPlayer(NewObject<ULocalPlayer>(GEngine));
    F.PC->Player = LocalPlayer.Get(); LocalPlayer->PlayerController = F.PC; F.PC->SetAsLocalPlayerController();
    const TStrongObjectPtr<USovCombatVitalsWidget> Widget(NewObject<USovCombatVitalsWidget>(F.PC));
    Widget->SetOwningPlayer(F.PC); Widget->Initialize();
    if (!TestTrue(TEXT("LocalPlayer resolves its registered world controller"), LocalPlayer->GetPlayerController(F.World) == F.PC)
        || !TestTrue(TEXT("Mounted widget resolves that same controller"), Widget->GetOwningPlayer() == F.PC)) { return false; }
    // Retain the real Slate tree as a viewport does, so these assertions exercise mounted rows.
    const TSharedRef<SWidget> SlateRoot = Widget->TakeWidget();
    SlateRoot->SlatePrepass();
    auto* Survival = Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("SurvivalVitalsPanel")));
    auto* Echo = Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("EchoVitalsPanel")));
    auto* StaminaRow = Cast<UVerticalBox>(Widget->WidgetTree->FindWidget(TEXT("StaminaVitalRow")));
    auto* HealthRow = Cast<UVerticalBox>(Widget->WidgetTree->FindWidget(TEXT("HealthVitalRow")));
    auto* EchoRow = Cast<UVerticalBox>(Widget->WidgetTree->FindWidget(TEXT("EchoVitalRow")));
    if (!TestNotNull(TEXT("Survival panel"), Survival) || !TestNotNull(TEXT("Echo panel"), Echo)
        || !TestNotNull(TEXT("Stamina row"), StaminaRow) || !TestNotNull(TEXT("Health row"), HealthRow)
        || !TestNotNull(TEXT("Echo row"), EchoRow)) { return false; }
    auto* SurvivalSlot = Cast<UCanvasPanelSlot>(Survival->Slot);
    auto* EchoSlot = Cast<UCanvasPanelSlot>(Echo->Slot);
    if (!TestNotNull(TEXT("Survival canvas slot"), SurvivalSlot)
        || !TestNotNull(TEXT("Echo canvas slot"), EchoSlot)) { return false; }
    TestEqual(TEXT("Survival anchors at lower left"), SurvivalSlot->GetAnchors().Minimum, FVector2D(0.f, 1.f));
    TestEqual(TEXT("Echo anchors at lower right"), EchoSlot->GetAnchors().Minimum, FVector2D(1.f, 1.f));
    TestEqual(TEXT("Echo growth follows the bottom-right pivot"), Echo->GetRenderTransformPivot(), FVector2D(1.f, 1.f));
    TestTrue(TEXT("Echo has a separate parent from survival resources"), EchoRow->GetParent() != HealthRow->GetParent());
    TestFalse(TEXT("The overlay never takes keyboard focus"), Widget->IsFocusable());

    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(), 100.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 100.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 25.f);
    Widget->RefreshVitals();
    TestTrue(TEXT("Real Slate root remains retained"), Widget->GetCachedWidget().IsValid());
    TestEqual(TEXT("Full stationary stamina is hidden on first admission"), StaminaRow->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("Damaged health remains in the visible survival panel"), Survival->GetVisibility(), ESlateVisibility::HitTestInvisible);
    TestTrue(TEXT("Health row is never collapsed by stamina context"), HealthRow->GetVisibility() != ESlateVisibility::Collapsed);
    TestTrue(TEXT("Echo reserves at least 144 logical pixels above the existing ammo margin"), EchoSlot->GetPosition().Y <= -168.f);
    TestEqual(TEXT("Echo preserves the right-edge margin"), EchoSlot->GetPosition().X, -24.);
    TestEqual(TEXT("Echo remains noninteractive"), Echo->GetVisibility(), ESlateVisibility::HitTestInvisible);

    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 70.f);
    Widget->RefreshVitals();
    TestEqual(TEXT("Spending stamina reveals the row"), StaminaRow->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Widget->RefreshVitals();
    TestEqual(TEXT("Stable stamina below full stays visible"), StaminaRow->GetVisibility(), ESlateVisibility::HitTestInvisible);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 90.f);
    Widget->RefreshVitals();
    TestEqual(TEXT("Recovery remains visible"), StaminaRow->GetVisibility(), ESlateVisibility::HitTestInvisible);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 100.f);
    Widget->RefreshVitals();
    TestEqual(TEXT("The recovery update reaching full remains visible"), StaminaRow->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Widget->RefreshVitals();
    TestEqual(TEXT("Full settled stamina hides again"), StaminaRow->GetVisibility(), ESlateVisibility::Collapsed);

    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(), 120.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 120.f);
    Widget->RefreshVitals();
    TestEqual(TEXT("A changing maximum is visible even when current also reaches full"), StaminaRow->GetVisibility(), ESlateVisibility::HitTestInvisible);
    FSovCombatVitalsSnapshot BeforeHide;
    TestTrue(TEXT("Capture canonical resources before presentation-only changes"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, BeforeHide));
    const auto Hide = FNarrativeGameplayTags::Get().State_Player_WantsHideHUD;
    ASC->AddLooseGameplayTag(Hide, 2, EGameplayTagReplicationState::CountToOwner);
    Widget->RefreshVitals();
    TestEqual(TEXT("Cinematic ownership hides survival"), Survival->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("Cinematic ownership also hides Echo"), Echo->GetVisibility(), ESlateVisibility::Collapsed);
    ASC->RemoveLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    Widget->RefreshVitals();
    TestEqual(TEXT("One remaining hide owner still collapses survival"), Survival->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("One remaining hide owner still collapses Echo"), Echo->GetVisibility(), ESlateVisibility::Collapsed);
    ASC->RemoveLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    Widget->RefreshVitals();
    FSovCombatVitalsSnapshot AfterHide;
    TestTrue(TEXT("Projection resumes with the same resource owner"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, AfterHide));
    for (int32 Index = 0; Index < 5; ++Index)
    {
        TestEqual(TEXT("Widget layout/hide changes preserve every resource"), AfterHide.Values[Index].Current, BeforeHide.Values[Index].Current);
        TestEqual(TEXT("Widget layout/hide changes preserve every maximum"), AfterHide.Values[Index].Maximum, BeforeHide.Values[Index].Maximum);
    }
    TestEqual(TEXT("Returning from hidden context does not reuse an old change sample"), StaminaRow->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("Damage is still visible after the hide owner releases"), Survival->GetVisibility(), ESlateVisibility::HitTestInvisible);

    auto* Replacement = F.StagePawn();
    if (!TestNotNull(TEXT("Replacement pawn"), Replacement)) { return false; }
    Widget->RefreshVitals();
    TestEqual(TEXT("Unready handoff collapses survival"), Survival->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("Unready handoff collapses Echo"), Echo->GetVisibility(), ESlateVisibility::Collapsed);
    TestTrue(TEXT("Replacement reaches readiness"), Replacement->CompleteCampaignDataInitialization(false));
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(), 80.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 80.f);
    Widget->RefreshVitals();
    TestEqual(TEXT("A full replacement does not inherit outgoing stamina history"), StaminaRow->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("Projection does not change replacement stamina"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()), 80.f);
    F.PC->Player = nullptr; LocalPlayer->PlayerController = nullptr;
    return true;
}
#endif
