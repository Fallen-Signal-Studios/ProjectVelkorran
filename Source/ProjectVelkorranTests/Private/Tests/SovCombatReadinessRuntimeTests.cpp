// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatReadinessTestFixtures.h"
#include "Tests/SovReadinessRuntimeTestFixtures.h"
#include "UI/SovCombatReadinessWidget.h"
#include "Tests/SovTarrikPayloadTestFixtures.h"
#include "Tests/SovCombatInterruptionTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

int32 USovHUDReadinessTestAbility::ActivationQueries=0;
USovHUDReadinessTestAbility::USovHUDReadinessTestAbility()
{
    bRequiresAllowedWeapon=false;
    MinimumEchoRequired=35.f; EchoCost=20.f;
    RequiredCharacterTag=FSovGameplayTags::Get().Character_Player_Tarrik;
    InputTag=FNarrativeGameplayTags::Get().Narrative_Input_Ability1;
    AbilityDisplayName=FText::FromString(TEXT("Echo HUD probe"));
    CooldownTags.AddTag(FNarrativeGameplayTags::Get().Narrative_Input_Ability3);
}
bool USovHUDReadinessTestAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* Relevant) const
{ ++ActivationQueries; return Super::CanActivateAbility(Handle,Info,SourceTags,TargetTags,Relevant); }
USovHUDReadinessCooldown::USovHUDReadinessCooldown()
{ DurationPolicy=EGameplayEffectDurationType::HasDuration; DurationMagnitude=FGameplayEffectModifierMagnitude(FScalableFloat(2.f)); }

#if WITH_AUTOMATION_TESTS
namespace
{
    struct FReadinessWorld
    {
        FEditorScriptExecutionGuard Guard;
        UWorld* World=nullptr;
        ASovHUDReadinessTestController* PC=nullptr;
        ASovPlayerState* PS=nullptr;
        FReadinessWorld()
        {
            const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
            if(!World){return;}
            if(GEngine){GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);}
            PC=World->SpawnActor<ASovHUDReadinessTestController>();
            if(PC){World->AddController(PC);}
            PS=World->SpawnActor<ASovPlayerState>();
            if(PC&&PS){PC->SetTestPlayerState(PS);}
        }
        ~FReadinessWorld()
        { if(World){World->DestroyWorld(false);if(GEngine){GEngine->DestroyWorldContext(World);}} }
        ASovReadinessRuntimeTestPawn* Stage(bool bComplete=true)
        {
            if(!PC||!PS){return nullptr;}
            auto* Pawn=World->SpawnActor<ASovReadinessRuntimeTestPawn>();
            auto* Definition=NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            if(!Pawn||!Pawn->PrepareCampaignInitialization(Definition)){return nullptr;}
            PC->Possess(Pawn);
            if(!Pawn->StageTestReadiness(PS,true)){return nullptr;}
            Pawn->BindProductionReadiness();
            if(bComplete&&!Pawn->CompleteCampaignDataInitialization(false)){return nullptr;}
            return Pawn;
        }
        UNarrativeAbilitySystemComponent* ASC() const
        {return PS?Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent()):nullptr;}
        FGameplayAbilitySpecHandle Grant()
        {
            FGameplayAbilitySpec Spec(USovHUDReadinessTestAbility::StaticClass(),1);
            Spec.GetDynamicSpecSourceTags().AddTag(FNarrativeGameplayTags::Get().Narrative_Input_Ability1);
            return ASC()->GiveAbility(Spec);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAbilityHUDResources,
    "ProjectVelkorran.UI.Readiness.NativeGrantCostAndCooldownWithoutActivation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovAbilityHUDResources::RunTest(const FString& Parameters)
{
    FReadinessWorld F;
    if(!TestNotNull(TEXT("Production-ready owner"),F.Stage())||!TestNotNull(TEXT("Current native ASC"),F.ASC())){return false;}
    auto* ASC=F.ASC(); const auto Handle=F.Grant();
    if(!TestTrue(TEXT("Actual native grant"),Handle.IsValid())){return false;}
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(),10.f);
    const float Health=ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
    const int32 Queries=USovHUDReadinessTestAbility::ActivationQueries;
    const int32 Effects=ASC->GetActiveEffects(FGameplayEffectQuery()).Num();
    FSovCombatReadinessSnapshot View;
    for(int32 I=0;I<10;++I)
    {
        if(!TestTrue(TEXT("Read current grant"),SovCombatReadiness::Read(F.PC,View))||!TestEqual(TEXT("Only actual grant visible"),View.Abilities.Num(),1)){return false;}
    }
    TestTrue(TEXT("Exact current grant handle"),View.Abilities[0].Handle==Handle);
    TestEqual(TEXT("Native authored cost"),View.Abilities[0].EchoCost,20.f);
    TestEqual(TEXT("Native threshold can exceed spend"),View.Abilities[0].EchoRequired,35.f);
    TestFalse(TEXT("Insufficient cost state is truthful"),View.Abilities[0].bCostSatisfied);
    TestTrue(TEXT("Echo deficit is explicit"),View.Abilities[0].State==ESovAbilityHUDState::NeedsEcho);
    TestEqual(TEXT("Read does not invoke activation/Blueprint admission"),USovHUDReadinessTestAbility::ActivationQueries,Queries);
    TestEqual(TEXT("Read never spends Echo"),ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()),10.f);
    TestEqual(TEXT("Read never changes health"),ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()),Health);
    TestEqual(TEXT("Read never applies effects"),ASC->GetActiveEffects(FGameplayEffectQuery()).Num(),Effects);
    TestFalse(TEXT("Read never presses an ability input"),ASC->FindAbilitySpecFromHandle(Handle)->InputPressed);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(),40.f);
    SovCombatReadiness::Read(F.PC,View);
    TestTrue(TEXT("Native threshold and cost now both satisfied"),View.Abilities.Num()==1&&View.Abilities[0].bCostSatisfied);
    FString Reason; FGameplayTagContainer PresentationTags,ActivationTags;
    const auto* Ability=Cast<USovGameplayAbility_EchoBase>(ASC->FindAbilitySpecFromHandle(Handle)->Ability);
    const auto Info=*ASC->AbilityActorInfo;
    TestEqual(TEXT("Diagnostic cost helper preserves CheckCost result"),Ability->CheckEchoPresentationCost(Handle,&Info,Reason,&PresentationTags),Ability->CheckCost(Handle,&Info,&ActivationTags));
    TestTrue(TEXT("Diagnostic and activation failure tags agree"),PresentationTags==ActivationTags);
    FGameplayEffectSpec Cooldown(GetDefault<USovHUDReadinessCooldown>(),ASC->MakeEffectContext(),1.f);
    Cooldown.DynamicGrantedTags.AppendTags(*Ability->GetCooldownTags());
    const auto Effect=ASC->ApplyGameplayEffectSpecToSelf(Cooldown);
    if(!TestTrue(TEXT("Real native cooldown effect"),Effect.IsValid())){return false;}
    SovCombatReadiness::Read(F.PC,View);
    if(!TestEqual(TEXT("Cooldown keeps the actual slot"),View.Abilities.Num(),1)){return false;}
    TestTrue(TEXT("Countdown comes from actual active GE duration"),View.Abilities[0].State==ESovAbilityHUDState::Cooldown&&View.Abilities[0].CooldownRemaining>0.f&&View.Abilities[0].CooldownRemaining<=2.01f);
    TestTrue(TEXT("Cooldown never falsifies the separate resource read"),View.Abilities[0].bCostSatisfied);
    ASC->RemoveActiveGameplayEffect(Effect);
    SovCombatReadiness::Read(F.PC,View);
    TestTrue(TEXT("Native effect removal retires the countdown"),View.Abilities.Num()==1&&View.Abilities[0].CooldownRemaining==0.f&&View.Abilities[0].State!=ESovAbilityHUDState::Cooldown);
    TestEqual(TEXT("All readiness samples remain activation-free"),USovHUDReadinessTestAbility::ActivationQueries,Queries);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAbilityHUDRetirement,
    "ProjectVelkorran.UI.Readiness.DuplicateGrantHideHandoffAndRemovalRetireState",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovAbilityHUDRetirement::RunTest(const FString& Parameters)
{
    FReadinessWorld F; auto* First=F.Stage();
    if(!TestNotNull(TEXT("Ready owner"),First)||!TestNotNull(TEXT("Native ASC"),F.ASC())){return false;}
    auto* ASC=F.ASC(); const auto Handle=F.Grant();
    FSovCombatReadinessSnapshot View;
    TestTrue(TEXT("Current actual grant is present"),SovCombatReadiness::Read(F.PC,View)&&View.Abilities.Num()==1);
    const auto Duplicate=F.Grant();
    TestTrue(TEXT("Duplicate live semantic grants refuse an arbitrary choice"),SovCombatReadiness::Read(F.PC,View)&&View.Abilities.IsEmpty());
    ASC->ClearAbility(Duplicate);
    TestTrue(TEXT("Native duplicate removal restores a unique grant"),SovCombatReadiness::Read(F.PC,View)&&View.Abilities.Num()==1);
    const auto Hide=FNarrativeGameplayTags::Get().State_Player_WantsHideHUD;
    ASC->AddLooseGameplayTag(Hide,2);
    TestFalse(TEXT("Counted cinematic hide clears readiness"),SovCombatReadiness::Read(F.PC,View));
    TestTrue(TEXT("No stale ability, pawn or companion text"),View.Abilities.IsEmpty()&&!View.Pawn.IsValid()&&View.CompanionText.IsEmpty());
    ASC->RemoveLooseGameplayTag(Hide,1);
    TestFalse(TEXT("One remaining hide owner still suppresses"),SovCombatReadiness::Read(F.PC,View));
    ASC->RemoveLooseGameplayTag(Hide,1);
    TestTrue(TEXT("Final hide release restores only current grants"),SovCombatReadiness::Read(F.PC,View)&&View.Abilities.Num()==1);
    auto* Second=F.Stage(false);
    if(!TestNotNull(TEXT("Actual staged replacement"),Second)){return false;}
    TestFalse(TEXT("Unready new avatar cannot retain old readiness"),SovCombatReadiness::Read(F.PC,View));
    TestFalse(TEXT("Old pawn reference retired"),View.Pawn.IsValid());
    if(!TestTrue(TEXT("Replacement crosses production readiness"),Second->CompleteCampaignDataInitialization(false))){return false;}
    TestTrue(TEXT("Current snapshot follows actual replacement"),SovCombatReadiness::Read(F.PC,View)&&View.Pawn.Get()==Second);
    ASC->ClearAbility(Handle);
    TestTrue(TEXT("Native grant removal removes the slot"),SovCombatReadiness::Read(F.PC,View)&&View.Abilities.IsEmpty());
    ASC->InitAbilityActorInfo(F.PS,First);
    TestFalse(TEXT("Replaced ASC/avatar binding clears all view state"),SovCombatReadiness::Read(F.PC,View));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAbilityHUDBindings,
    "ProjectVelkorran.UI.Readiness.CurrentEnhancedInputRemapChordAndDeviceHints",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovAbilityHUDBindings::RunTest(const FString& Parameters)
{
    FReadinessWorld F;
    if(!TestNotNull(TEXT("Ready native owner"),F.Stage())){return false;}
    auto* LP=NewObject<ULocalPlayer>(GEngine); F.PC->Player=LP; LP->PlayerController=F.PC; F.PC->SetAsLocalPlayerController();
    auto* Input=NewObject<UEnhancedPlayerInput>(F.PC); F.PC->PlayerInput=Input;
    auto* Subsystem=NewObject<UEnhancedInputLocalPlayerSubsystem>(LP); F.PC->KeepAlive.Add(Subsystem);
    auto* Context=NewObject<UInputMappingContext>(F.PC); F.PC->KeepAlive.Add(Context);
    auto* Action=NewObject<UInputAction>(Context); auto* Modifier=NewObject<UInputAction>(Context);
    auto* Schema=NewObject<UNarrativeAbilityInputMapping>(F.PC); F.PC->KeepAlive.Add(Schema); F.PC->SetInputSchema(Schema);
    const auto& Tags=FNarrativeGameplayTags::Get();
    FAbilityInputMappingData Row; Row.InputAction=Action; Row.InputTag=Tags.Narrative_Input_Ability1;
    Row.RequiredModifierTag=FSovGameplayTags::Get().Input_AbilityModifier; Row.ModifiedInputTag=Tags.Narrative_Input_Ability2;
    Schema->InputAbilities.Add(Row);
    FAbilityInputMappingData ModifierRow; ModifierRow.InputAction=Modifier; ModifierRow.InputTag=Row.RequiredModifierTag;
    Schema->InputAbilities.Add(ModifierRow);
    Context->MapKey(Action,EKeys::Q); Context->MapKey(Action,EKeys::Gamepad_FaceButton_Top); Context->MapKey(Modifier,EKeys::LeftShift);
    FModifyContextOptions Options; Options.bForceImmediately=true;
    Subsystem->AddMappingContext(Context,0,Options);
    const auto Hint=[&](FGameplayTag Tag,bool bGamepad=false){return SovCombatReadiness::BindingForInput(Schema,Tag,Subsystem,bGamepad).ToString();};
    TestEqual(TEXT("Schema getter is actual controller schema"),F.PC->GetAbilityHUDInputMappings(),static_cast<const UNarrativeAbilityInputMapping*>(Schema));
    TestEqual(TEXT("Keyboard hint comes from active native mapping"),Hint(Row.InputTag),EKeys::Q.GetDisplayName(true).ToString());
    TestEqual(TEXT("Gamepad uses its active device mapping"),Hint(Row.InputTag,true),EKeys::Gamepad_FaceButton_Top.GetDisplayName(true).ToString());
    TestEqual(TEXT("Semantic modifier hint uses both actual actions"),Hint(Row.ModifiedInputTag),EKeys::LeftShift.GetDisplayName(true).ToString()+TEXT(" + ")+EKeys::Q.GetDisplayName(true).ToString());
    Context->UnmapKey(Action,EKeys::Q); Context->MapKey(Action,EKeys::R);
    Subsystem->RequestRebuildControlMappings(Options);
    TestEqual(TEXT("Real mapping rebuild immediately retires the old key"),Hint(Row.InputTag),EKeys::R.GetDisplayName(true).ToString());
    Subsystem->RemoveMappingContext(Context,Options);
    TestTrue(TEXT("Unmapped action never invents a binding"),Hint(Row.InputTag).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAbilityHUDImages,
    "ProjectVelkorran.UI.Readiness.ConcreteGrantImagesAndAccessibleRetirement",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovAbilityHUDImages::RunTest(const FString& Parameters)
{
    FReadinessWorld F;
    if(!TestNotNull(TEXT("Production-ready owner"),F.Stage())||!TestNotNull(TEXT("Native ASC"),F.ASC())){return false;}
    auto* ASC=F.ASC();
    auto* Widget=NewObject<USovCombatReadinessWidget>(F.PC);
    Widget->SetOwningPlayer(F.PC);
    const auto Slate=Widget->TakeWidget();
    const auto Input=FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
    const auto Grant=[&](UClass* Class)
    {
        FGameplayAbilitySpec Spec(Class,1); Spec.GetDynamicSpecSourceTags().AddTag(Input);
        return ASC->GiveAbility(Spec);
    };
    const auto Sword=Grant(USovTarrikHungerTestAbility::StaticClass());
    FSovCombatReadinessSnapshot View;
    if(!TestTrue(TEXT("Actual sword grant read"),SovCombatReadiness::Read(F.PC,View))
        ||!TestEqual(TEXT("One actual semantic grant"),View.Abilities.Num(),1)){return false;}
    TestTrue(TEXT("Concrete sword supplies its equipment image"),View.Abilities[0].Icon==ESovAbilityHUDIcon::Hunger);
    TestEqual(TEXT("Actual slot stays stable"),View.Abilities[0].SemanticSlot,1);
    Widget->Present(View);
    const FString FullName=View.Abilities[0].Name.ToString();
    TestTrue(TEXT("Prominent image retains full actual name for accessibility"),Widget->GetAccessibleReadinessText().ToString().Contains(FullName));
    TestTrue(TEXT("Accessible status remains the actual native status"),Widget->GetAccessibleReadinessText().ToString().Contains(View.Abilities[0].Status.ToString()));
#if WITH_ACCESSIBILITY
    TestTrue(TEXT("Slate exposes the full description"),Widget->GetAccessibleText().ToString().Contains(FullName));
#endif
    ASC->ClearAbility(Sword);
    const auto Rifle=Grant(USovCombatJudgementTestAbility::StaticClass());
    if(!TestTrue(TEXT("Actual replacement read"),SovCombatReadiness::Read(F.PC,View))
        ||!TestEqual(TEXT("Same input has one replacement"),View.Abilities.Num(),1)){return false;}
    TestTrue(TEXT("Same semantic input acquires the rifle image, never the stale sword"),View.Abilities[0].Icon==ESovAbilityHUDIcon::Judgement);
    Widget->Present(View);
    TestFalse(TEXT("Replaced ability name retires"),Widget->GetAccessibleReadinessText().ToString().Contains(FullName));
    ASC->ClearAbility(Rifle);
    F.Grant();
    if(!TestTrue(TEXT("Unmapped concrete class still reads honestly"),SovCombatReadiness::Read(F.PC,View))
        ||!TestEqual(TEXT("One unfamiliar grant"),View.Abilities.Num(),1)){return false;}
    TestTrue(TEXT("Foreign ability gets no borrowed equipment image"),View.Abilities[0].Icon==ESovAbilityHUDIcon::Unknown);
    Widget->Present({});
    TestTrue(TEXT("Hidden/retired owner clears accessible name and status"),Widget->GetAccessibleReadinessText().IsEmpty());
#if WITH_ACCESSIBILITY
    TestTrue(TEXT("Slate description retires too"),Widget->GetAccessibleText().IsEmpty());
#endif
    TestTrue(TEXT("Retired strip collapses"),Widget->GetVisibility()==ESlateVisibility::Collapsed);
    return true;
}
#endif
