// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovCombatReadinessWidget.h"
#include "UI/SovCombatVitalsWidget.h"
#include "UI/SovHUDStyle.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "Abilities/SovGameplayAbility_TarrikEcho.h"
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Companions/SovCompanionComponent.h"
#include "Components/SovEchoComponent.h"
#include "Components/CanvasPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilityInputMapping.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Settings/SovGameUserSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "SovCombatReadiness"
namespace
{
    const USovGameplayAbility_EchoBase* CurrentAbility(const FGameplayAbilitySpec& Spec)
    { return Cast<USovGameplayAbility_EchoBase>(Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get()); }
}

ESovAbilityHUDIcon SovCombatReadiness::IconForAbilityClass(const UClass* AbilityClass)
{
    if (!AbilityClass) { return ESovAbilityHUDIcon::Unknown; }
    // These are equipment pictograms for the concrete kit, never a guess from a name or input slot.
    if (AbilityClass->IsChildOf(USovGameplayAbility_TarrikCinderStickyGrenade::StaticClass())) { return ESovAbilityHUDIcon::CinderGrenade; }
    if (AbilityClass->IsChildOf(USovGameplayAbility_TarrikVelkorransHunger::StaticClass())) { return ESovAbilityHUDIcon::Hunger; }
    if (AbilityClass->IsChildOf(USovGameplayAbility_TarrikCinderJudgement::StaticClass())) { return ESovAbilityHUDIcon::Judgement; }
    if (AbilityClass->IsChildOf(USovGameplayAbility_TarrikCinderSlam::StaticClass())) { return ESovAbilityHUDIcon::Slam; }
    if (AbilityClass->IsChildOf(USovGameplayAbility_TarrikCinderlineRequiem::StaticClass())) { return ESovAbilityHUDIcon::Requiem; }
    if (AbilityClass->IsChildOf(USovGameplayAbility_SeleneStillpointGrenade::StaticClass())) { return ESovAbilityHUDIcon::Stillpoint; }
    if (AbilityClass->IsChildOf(USovGameplayAbility_SeleneVeritysWake::StaticClass())) { return ESovAbilityHUDIcon::Wake; }
    if (AbilityClass->IsChildOf(USovGameplayAbility_SeleneStaccatoZero::StaticClass())) { return ESovAbilityHUDIcon::Staccato; }
    if (AbilityClass->IsChildOf(USovGameplayAbility_SeleneAxiomNullPulse::StaticClass())) { return ESovAbilityHUDIcon::NullPulse; }
    if (AbilityClass->IsChildOf(USovGameplayAbility_SeleneDispatch::StaticClass())) { return ESovAbilityHUDIcon::Dispatch; }
    return ESovAbilityHUDIcon::Unknown;
}

FText SovCombatReadiness::BindingForInput(const UNarrativeAbilityInputMapping* Schema, FGameplayTag Input,
    const UEnhancedInputLocalPlayerSubsystem* Subsystem, bool bGamepad)
    {
        if (!IsValid(Schema) || !Subsystem) { return FText::GetEmpty(); }
        const auto Keys = [&](const UInputAction* Action)
        {
            TArray<FString> Labels;
            for (const FKey& Key : Subsystem->QueryKeysMappedToAction(Action))
            {
                if (Key.IsValid() && Key.IsGamepadKey() == bGamepad) { Labels.AddUnique(Key.GetDisplayName(true).ToString()); }
            }
            Labels.Sort();
            return FString::Join(Labels, TEXT(" / "));
        };
        TArray<FString> Routes;
        for (const auto& Row : Schema->InputAbilities)
        {
            if (!IsValid(Row.InputAction)) { continue; }
            const bool Direct = Row.InputTag == Input;
            const bool Modified = Row.ModifiedInputTag == Input && Row.RequiredModifierTag.IsValid();
            if (!Direct && !Modified) { continue; }
            const FString Key = Keys(Row.InputAction);
            if (Key.IsEmpty()) { continue; }
            if (Direct) { Routes.AddUnique(Key); }
            else
            {
                for (const auto& Modifier : Schema->InputAbilities)
                {
                    if (Modifier.InputTag != Row.RequiredModifierTag || !IsValid(Modifier.InputAction)) { continue; }
                    const FString ModifierKey = Keys(Modifier.InputAction);
                    if (!ModifierKey.IsEmpty()) { Routes.AddUnique(ModifierKey + TEXT(" + ") + Key); }
                }
            }
        }
        Routes.Sort();
        return FText::FromString(FString::Join(Routes, TEXT(" / ")));
    }
namespace
{
    FText Companion(const ASovPlayerController* PC, const ASovPlayerCharacterBase* Pawn)
    {
        const auto* State = PC->GetConvergenceCompanionState();
        if (!IsValid(State) || State->GetOwner() != PC || State->HasStagedProxy() || State->IsEncounterRestorePending()) { return {}; }
        const auto* Proxy = State->GetActiveCompanion();
        const auto* Component = IsValid(Proxy) ? Proxy->GetCompanionComponent() : nullptr;
        const auto* ASC = IsValid(Proxy) ? Proxy->GetNarrativeAbilitySystemComponent() : nullptr;
        if (!IsValid(Proxy) || Proxy->IsActorBeingDestroyed() || Proxy->GetWorld() != Pawn->GetWorld() || Proxy->GetOwner() != PC
            || Proxy->IsCharacterPendingLoad() || !IsValid(Component) || Component->GetOwner() != Proxy
            || Component->GetCurrentLeader() != Pawn || !IsValid(ASC) || ASC->GetAvatarActor() != Proxy
            || ASC->GetSet<UNarrativeAttributeSetBase>() != Proxy->GetAttributeSetBase()) { return {}; }
        const auto& Tags = FSovGameplayTags::Get();
        const auto Identity = Proxy->GetCompanionIdentity();
        if (Identity != Tags.Character_Player_Tarrik && Identity != Tags.Character_Player_Selene) { return {}; }
        if (Identity == Pawn->GetProtagonistIdentityTag()) { return {}; }
        const FText Name = Identity == Tags.Character_Player_Tarrik ? LOCTEXT("Tarrik", "Tarrik") : LOCTEXT("Selene", "Selene");
        const float Health = Proxy->GetHealth(), MaxHealth = Proxy->GetMaxHealth();
        if (!FMath::IsFinite(Health) || !FMath::IsFinite(MaxHealth) || Health < 0.f || MaxHealth <= 0.f) { return {}; }
        FText StateText = LOCTEXT("CoActionIdle", "Co-action: idle");
        if (Component->IsDisabled() || Health <= 0.f) { StateText = LOCTEXT("CompanionDisabled", "Disabled"); }
        else if (Component->GetCommandState() == ESovCompanionCommandState::MovingToAnchor) { StateText = LOCTEXT("CoActionMoving", "Co-action: moving"); }
        else if (Component->GetCommandState() == ESovCompanionCommandState::Succeeded) { StateText = LOCTEXT("CoActionDone", "Co-action: complete"); }
        else if (Component->GetCommandState() == ESovCompanionCommandState::Failed) { StateText = LOCTEXT("CoActionFailed", "Co-action: not completed"); }
        FNumberFormattingOptions Number; Number.SetMaximumFractionalDigits(0);
        return FText::Format(LOCTEXT("CompanionSummary", "{0}  {1}/{2} HP\n{3}"), Name, FText::AsNumber(Health, &Number), FText::AsNumber(MaxHealth, &Number), StateText);
    }
}

bool SovCombatReadiness::Read(const ASovPlayerController* PC, FSovCombatReadinessSnapshot& Out)
{
    Out = {};
    FSovCombatVitalsSnapshot Vitals;
    if (!USovCombatVitalsWidget::ReadCurrentVitals(PC, Vitals) || Vitals.Values[0].Current <= 0.f) { return false; }
    auto* Pawn = Cast<ASovPlayerCharacterBase>(Vitals.Pawn.Get());
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(PC->GetAbilitySystemComponent());
    if (!IsValid(Pawn) || !ASC || !ASC->AbilityActorInfo.IsValid() || ASC->AbilityActorInfo->AvatarActor.Get() != Pawn
        || ASC->AbilityActorInfo->AbilitySystemComponent.Get() != ASC) { return false; }
    const uint64 Epoch = ASC->GetCombatActorInfoEpoch();
    const FGameplayAbilityActorInfo Info = *ASC->AbilityActorInfo;
    const auto* Echo = Pawn->GetEchoComponent();
    const auto& Tags = FNarrativeGameplayTags::Get();
    const FGameplayTag Inputs[] = {Tags.Narrative_Input_Ability1, Tags.Narrative_Input_Ability2, Tags.Narrative_Input_Ability3};
    FSovCombatReadinessSnapshot Current; Current.Pawn = Pawn; Current.Protagonist = Vitals.Protagonist;
    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        TArray<const FGameplayAbilitySpec*> Granted, Equipped, Active;
        for (const auto& Spec : ASC->GetActivatableAbilities())
        {
            const auto* Ability = CurrentAbility(Spec);
            if (!IsValid(Ability) || !Spec.Handle.IsValid() || Spec.PendingRemove || Ability->GetEchoAbilityDisplayName().IsEmpty()
                || !Spec.GetDynamicSpecSourceTags().HasTagExact(Inputs[Slot])) { continue; }
            Granted.Add(&Spec);
            if (Spec.IsActive()) { Active.Add(&Spec); }
            if (Ability->CanUseEchoWeaponContext(Spec.Handle, &Info)) { Equipped.Add(&Spec); }
        }
        // Never pick an arbitrary shared-input variant or invent a not-yet-granted slot.
        const auto& Selection = !Active.IsEmpty() ? Active : !Equipped.IsEmpty() ? Equipped : Granted;
        if (Selection.Num() != 1) { continue; }
        const auto& Spec = *Selection[0];
        const auto* Ability = CurrentAbility(Spec);
        FSovAbilityHUDEntry Entry; Entry.Handle = Spec.Handle; Entry.InputTag = Inputs[Slot]; Entry.SemanticSlot = Slot;
        Entry.Name = Ability->GetEchoAbilityDisplayName(); Entry.EchoCost = Ability->GetEchoCost();
        Entry.Icon = IconForAbilityClass(Ability->GetClass());
        Entry.EchoRequired = FMath::Max(Entry.EchoCost, Ability->GetMinimumEchoRequired()); Entry.EchoCurrent = Echo->GetEcho();
        if (!FMath::IsFinite(Entry.EchoCost) || !FMath::IsFinite(Entry.EchoRequired) || !FMath::IsFinite(Entry.EchoCurrent)) { continue; }
        const auto* LP = PC->GetLocalPlayer();
        Entry.Binding = BindingForInput(PC->GetAbilityHUDInputMappings(), Entry.InputTag,
            LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr, PC->IsUsingGamepad());
        Ability->GetCooldownTimeRemainingAndDuration(Spec.Handle, &Info, Entry.CooldownRemaining, Entry.CooldownDuration);
        if (!FMath::IsFinite(Entry.CooldownRemaining) || !FMath::IsFinite(Entry.CooldownDuration)) { continue; }
        FString CostReason;
        Entry.bCostSatisfied = Ability->CheckEchoPresentationCost(Spec.Handle, &Info, CostReason);
        Entry.State = ESovAbilityHUDState::EchoReady; Entry.Status = LOCTEXT("EchoReady", "Echo ready");
        FNumberFormattingOptions Number; Number.SetMaximumFractionalDigits(0);
        if (Spec.IsActive()) { Entry.State = ESovAbilityHUDState::Active; Entry.Status = LOCTEXT("Active", "Active"); }
        else if (!Equipped.Contains(&Spec)) { Entry.State = ESovAbilityHUDState::WeaponRequired; Entry.Status = LOCTEXT("NeedsWeapon", "Equip weapon"); }
        else if (Entry.CooldownRemaining > 0.f)
        {
            Number.SetMaximumFractionalDigits(1);
            Entry.State = ESovAbilityHUDState::Cooldown;
            Entry.Status = FText::Format(LOCTEXT("Cooldown", "{0}s cooldown"), FText::AsNumber(Entry.CooldownRemaining, &Number));
        }
        else if (!Ability->CheckCooldown(Spec.Handle, &Info)) { Entry.State = ESovAbilityHUDState::Cooldown; Entry.Status = LOCTEXT("CooldownNoTime", "Cooldown"); }
        else if (!Echo->CanAffordEcho(Entry.EchoRequired))
        {
            Entry.State = ESovAbilityHUDState::NeedsEcho;
            Entry.Status = FText::Format(LOCTEXT("NeedsEcho", "Needs {0} Echo"), FText::AsNumber(Entry.EchoRequired, &Number));
        }
        else if (!Entry.bCostSatisfied) { Entry.State = ESovAbilityHUDState::Unavailable; Entry.Status = LOCTEXT("Unavailable", "Unavailable"); }
        else if (!Ability->DoesAbilitySatisfyTagRequirements(*ASC) || ASC->IsAbilityInputBlocked(Spec.InputID)
            || PC->IsGameplayAbilityInputSuppressed())
        { Entry.State = ESovAbilityHUDState::InputLocked; Entry.Status = LOCTEXT("InputLocked", "Input locked"); }
        else if (Entry.Binding.IsEmpty()) { Entry.State = ESovAbilityHUDState::Unbound; Entry.Status = LOCTEXT("Unbound", "Unbound"); }
        Current.Abilities.Add(MoveTemp(Entry));
    }
    Current.CompanionText = Companion(PC, Pawn);
    FSovCombatVitalsSnapshot After;
    if (!USovCombatVitalsWidget::ReadCurrentVitals(PC, After) || After.Pawn != Current.Pawn || After.Values[0].Current <= 0.f
        || ASC->GetCombatActorInfoEpoch() != Epoch || ASC->GetAvatarActor() != Pawn) { return false; }
    Out = MoveTemp(Current); return true;
}

USovCombatReadinessWidget::USovCombatReadinessWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{ SetIsFocusable(false); SetVisibility(ESlateVisibility::HitTestInvisible); }

TSharedRef<SWidget> USovCombatReadinessWidget::RebuildWidget()
{
    Displayed = {};
    if (WidgetTree && !WidgetTree->RootWidget) { WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(); }
    return Super::RebuildWidget();
}

void USovCombatReadinessWidget::NativeDestruct()
{ Present({}); Super::NativeDestruct(); }

void USovCombatReadinessWidget::Present(const FSovCombatReadinessSnapshot& Snapshot)
{
    Displayed = Snapshot;
    SetVisibility(Displayed.Pawn.IsValid() && !Displayed.Abilities.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    const FText Description = GetAccessibleReadinessText();
    SetToolTipText(Description);
#if WITH_ACCESSIBILITY
    if (const auto Accessible = GetAccessibleWidget())
    { Accessible->SetAccessibleBehavior(EAccessibleBehavior::Custom, TAttribute<FText>::CreateWeakLambda(this, [this]() { return GetAccessibleReadinessText(); }), EAccessibleType::Main); }
#endif
}

FText USovCombatReadinessWidget::GetAccessibleReadinessText() const
{
    if (!Displayed.Pawn.IsValid()) { return FText::GetEmpty(); }
    TArray<FString> Lines;
    for (const auto& Entry : Displayed.Abilities)
    {
        Lines.Add(FText::Format(LOCTEXT("AccessibleAbility", "{0}. {1}. {2}."), Entry.Name,
            Entry.Binding.IsEmpty() ? LOCTEXT("AccessibleUnbound", "Unbound") : Entry.Binding, Entry.Status).ToString());
    }
    return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

namespace
{
struct FReadinessIconLayout
{
    TArray<FString> Binding, Status, UnknownName;
    float IconY=0.f, StatusY=0.f;
};
struct FReadinessStripLayout
{
    TArray<FReadinessIconLayout> Icons;
    FSlateFontInfo Font=FCoreStyle::GetDefaultFontStyle("Regular",12);
    FSlateFontInfo Bold=FCoreStyle::GetDefaultFontStyle("Bold",13);
    float Line=17.f, Height=112.f, IconSize=70.f, Cell=100.f;
};

FReadinessStripLayout LayoutReadiness(const FSovCombatReadinessSnapshot& Snapshot, float Width)
{
    FReadinessStripLayout Layout;
    if (Snapshot.Abilities.IsEmpty() || !FSlateApplication::IsInitialized()) { return Layout; }
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    Layout.Line=FMath::CeilToFloat(FMath::Max(Measure->Measure(TEXT("Mg"),Layout.Font).Y,
        Measure->Measure(TEXT("Mg"),Layout.Bold).Y))+2.f;
    Layout.Cell=Width/3.f;
    Layout.IconSize=FMath::Min(70.f,FMath::Max(28.f,Layout.Cell-18.f));
    const auto Wrap=[&](const FString& Text,const FSlateFontInfo& Face)
    {
        TArray<FString> Lines;
        FString Remaining=Text;
        const float Available=FMath::Max(20.f,Layout.Cell-8.f);
        while (!Remaining.IsEmpty())
        {
            if (Measure->Measure(Remaining,Face).X<=Available) { Lines.Add(Remaining); break; }
            int32 Count=Remaining.Len();
            while (Count>1 && Measure->Measure(Remaining.Left(Count),Face).X>Available) { --Count; }
            const int32 WordBreak=Remaining.Left(Count).Find(TEXT(" "),ESearchCase::CaseSensitive,ESearchDir::FromEnd);
            if (WordBreak>0) { Count=WordBreak; }
            Lines.Add(Remaining.Left(Count)); Remaining=Remaining.Mid(Count).TrimStart();
        }
        return Lines;
    };
    Layout.Height=0.f;
    for (const auto& Entry : Snapshot.Abilities)
    {
        FReadinessIconLayout Icon;
        Icon.Binding=Wrap(Entry.Binding.IsEmpty()?TEXT("--"):Entry.Binding.ToString(),Layout.Bold);
        Icon.Status=Wrap(Entry.State==ESovAbilityHUDState::Cooldown && Entry.CooldownRemaining>0.f
            ? FString::Printf(TEXT("%.1fs"),Entry.CooldownRemaining) : Entry.Status.ToString(),Layout.Font);
        // Unknown grants keep their real name rather than acquiring another ability's image.
        if (Entry.Icon==ESovAbilityHUDIcon::Unknown) { Icon.UnknownName=Wrap(Entry.Name.ToString(),Layout.Font); }
        Icon.IconY=Icon.Binding.Num()*Layout.Line+2.f;
        Icon.StatusY=Icon.IconY+Layout.IconSize+8.f;
        Layout.Height=FMath::Max(Layout.Height,Icon.StatusY+(Icon.Status.Num()+Icon.UnknownName.Num())*Layout.Line+2.f);
        Layout.Icons.Add(MoveTemp(Icon));
    }
    // Keep image baselines aligned when a real remapped chord wraps to more lines.
    float SharedIconY=0.f;
    for (const auto& Icon:Layout.Icons) { SharedIconY=FMath::Max(SharedIconY,Icon.IconY); }
    Layout.Height=0.f;
    for (auto& Icon:Layout.Icons)
    {
        Icon.IconY=SharedIconY; Icon.StatusY=SharedIconY+Layout.IconSize+8.f;
        Layout.Height=FMath::Max(Layout.Height,Icon.StatusY+(Icon.Status.Num()+Icon.UnknownName.Num())*Layout.Line+2.f);
    }
    return Layout;
}

// Resolution-independent, HUD-owned equipment silhouettes. Each uses the concrete
// granted ability's identity. They are not faction-colored stock/RPG texture tiles.
TArray<TArray<FVector2D>> EquipmentImage(ESovAbilityHUDIcon Icon)
{
    TArray<TArray<FVector2D>> P;
    const auto Stroke=[&](std::initializer_list<FVector2D> Points) { P.Add(TArray<FVector2D>(Points)); };
    const auto Arc=[&](FVector2D Center,double Radius,double Start,double End)
    {
        TArray<FVector2D> Points;
        for (int32 I=0;I<=20;++I)
        { const double A=FMath::DegreesToRadians(Start+(End-Start)*I/20.); Points.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius); }
        P.Add(MoveTemp(Points));
    };
    switch (Icon)
    {
    case ESovAbilityHUDIcon::CinderGrenade:
        Stroke({{23,16},{41,16},{47,28},{47,47},{40,55},{24,55},{17,47},{17,28},{23,16}});
        Stroke({{27,16},{27,9},{40,9},{46,18},{46,28}});
        Stroke({{29,45},{25,37},{32,24},{34,34},{39,29},{40,41},{34,47},{29,45}}); break;
    case ESovAbilityHUDIcon::Hunger:
        Stroke({{17,51},{24,44},{18,38},{24,35},{39,10},{48,5},{47,18},{31,40},{34,47},{27,46},{20,55}});
        Stroke({{29,36},{40,17}}); Stroke({{10,42},{8,31},{15,19},{15,30},{20,24}});
        Stroke({{40,47},{50,38},{53,25},{56,37},{51,48},{40,54}}); break;
    case ESovAbilityHUDIcon::Judgement:
        Stroke({{9,22},{32,22},{45,32},{32,42},{9,42},{9,22}});
        Stroke({{3,28},{27,28},{35,32},{27,36},{3,36}});
        Stroke({{49,15},{56,15},{56,49},{49,49}}); Stroke({{44,6},{53,6}}); Stroke({{44,58},{53,58}}); break;
    case ESovAbilityHUDIcon::Slam:
        Stroke({{25,7},{39,7},{38,30},{32,40},{26,30},{25,7}});
        Stroke({{21,18},{43,18}}); Stroke({{32,7},{32,2}});
        Stroke({{5,46},{20,46},{25,40},{32,47},{39,40},{44,46},{59,46}});
        Stroke({{20,54},{13,60}}); Stroke({{32,53},{32,62}}); Stroke({{44,54},{51,60}}); break;
    case ESovAbilityHUDIcon::Requiem:
        Stroke({{8,27},{22,27},{31,32},{22,37},{8,37}});
        Stroke({{5,10},{33,10},{47,18},{33,26}});
        Stroke({{20,32},{45,32},{59,32},{49,24}}); Stroke({{59,32},{49,40}});
        Stroke({{5,54},{33,54},{47,46},{33,38}}); break;
    case ESovAbilityHUDIcon::Stillpoint:
        Stroke({{32,16},{47,31},{47,44},{32,57},{17,44},{17,31},{32,16}});
        Stroke({{27,16},{27,8},{39,8},{44,14}});
        Stroke({{32,24},{32,48}}); Stroke({{21,30},{43,42}}); Stroke({{21,42},{43,30}});
        Stroke({{27,25},{32,30},{37,25}}); Stroke({{27,47},{32,42},{37,47}}); break;
    case ESovAbilityHUDIcon::Wake:
        Stroke({{12,53},{22,39},{27,41},{42,14},{49,7},{47,22},{33,46},{29,45},{22,58}});
        Stroke({{21,37},{35,46}}); Stroke({{28,39},{40,20}});
        Arc({29,30},19,165,270); Arc({29,30},27,165,270);
        Stroke({{40,51},{48,47},{55,38}}); break;
    case ESovAbilityHUDIcon::Staccato:
        Stroke({{5,16},{14,23},{5,30}}); Stroke({{18,16},{27,23},{18,30}}); Stroke({{31,16},{40,23},{31,30}});
        Arc({45,43},13,0,360); Stroke({{36,52},{54,34}}); Stroke({{8,42},{24,42}}); Stroke({{8,49},{19,49}}); break;
    case ESovAbilityHUDIcon::NullPulse:
        Arc({32,32},21,0,360); Stroke({{3,32},{20,32},{26,20},{36,44},{43,32},{61,32}});
        Stroke({{32,3},{32,12}}); Stroke({{32,52},{32,61}}); break;
    case ESovAbilityHUDIcon::Dispatch:
        Stroke({{9,23},{9,9},{23,9}}); Stroke({{41,9},{55,9},{55,23}});
        Stroke({{9,41},{9,55},{23,55}}); Stroke({{41,55},{55,55},{55,41}});
        Stroke({{32,15},{39,32},{32,45},{25,32},{32,15}}); Stroke({{16,32},{48,32}});
        Stroke({{32,45},{32,61}}); break;
    default:
        Stroke({{32,12},{52,32},{32,52},{12,32},{32,12}}); break;
    }
    return P;
}
}

float USovCombatReadinessWidget::GetPresentationHeight(float Width) const
{ return LayoutReadiness(Displayed,FMath::Max(90.f,Width)).Height; }

int32 USovCombatReadinessWidget::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,
    FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    Layer=Super::NativePaint(Args,Geometry,CullingRect,Elements,Layer,Style,bParentEnabled);
    if (!Displayed.Pawn.IsValid() || Displayed.Abilities.IsEmpty() || !FSlateApplication::IsInitialized()
        || !bParentEnabled || Style.GetColorAndOpacityTint().A<=.01f) { return Layer; }
    const FVector2D Size=Geometry.GetLocalSize();
    if (Size.X<90.f || Size.Y<1.f) { return Layer; }
    const auto* Settings=USovGameUserSettings::Get();
    const bool HighContrast=Settings && Settings->GetSettingsSnapshot().bHighContrastHUD;
    const auto Theme=SovHUDStyle::ForProtagonist(Displayed.Protagonist,HighContrast);
    const auto Layout=LayoutReadiness(Displayed,Size.X);
    const float Opacity=Style.GetColorAndOpacityTint().A;
    const auto Lines=[&](const TArray<FVector2D>& Points,FLinearColor Color,float Thickness)
    {
        TArray<FVector2f> Values;for (const auto& P:Points) { Values.Add(FVector2f(P)); }
        FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),Values,ESlateDrawEffect::None,
            Color.CopyWithNewOpacity(Color.A*Opacity),true,Thickness);
    };
    const auto Text=[&](double X,double Y,const TArray<FString>& Rows,const FSlateFontInfo& Font,FLinearColor Color)
    {
        const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        for (const auto& Row:Rows)
        {
            const double Left=X+(Layout.Cell-Measure->Measure(Row,Font).X)*.5;
            for (int32 Shadow=1;Shadow>=0;--Shadow)
            {
                const auto C=Shadow ? FLinearColor(0,0,0,Opacity) : Color.CopyWithNewOpacity(Color.A*Opacity);
                FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2f(Size),
                    FSlateLayoutTransform(FVector2f(Left+Shadow,Y+Shadow))),FText::FromString(Row),Font,ESlateDrawEffect::None,C);
            }
            Y+=Layout.Line;
        }
    };
    Elements.PushClip(FSlateClippingZone(Geometry));
    for (int32 Index=0;Index<Displayed.Abilities.Num();++Index)
    {
        const auto& Entry=Displayed.Abilities[Index];const auto& Icon=Layout.Icons[Index];
        const double X=FMath::Clamp(Entry.SemanticSlot,0,2)*Layout.Cell;
        const FVector2D Origin(X+(Layout.Cell-Layout.IconSize)*.5,Icon.IconY);
        const float ImageScale=Layout.IconSize/64.f;
        const bool Ready=Entry.State==ESovAbilityHUDState::EchoReady || Entry.State==ESovAbilityHUDState::Active;
        const FLinearColor Color=HighContrast ? FLinearColor::White : Ready ? Theme.Accent : FLinearColor(.67f,.75f,.8f);
        for (const auto& Path:EquipmentImage(Entry.Icon))
        {
            TArray<FVector2D> Points;for (const auto& P:Path) { Points.Add(Origin+P*ImageScale); }
            Lines(Points,FLinearColor(0,0,0,.72f),4.5f*ImageScale);
            Lines(Points,Color,2.3f*ImageScale);
        }
        Text(X,0,Icon.Binding,Layout.Bold,FLinearColor::White);
        Text(X,Icon.StatusY,Icon.Status,Layout.Font,Color);
        Text(X,Icon.StatusY+Icon.Status.Num()*Layout.Line,Icon.UnknownName,Layout.Font,FLinearColor::White);
        // A slim charge/cooldown rail belongs to the image, with no card or enclosing panel.
        const float Fraction=Entry.State==ESovAbilityHUDState::Cooldown && Entry.CooldownDuration>0.f
            ? 1.f-Entry.CooldownRemaining/Entry.CooldownDuration : Entry.EchoRequired>0.f ? Entry.EchoCurrent/Entry.EchoRequired : 1.f;
        const FVector2D Rail=Origin+FVector2D(3,Layout.IconSize+4);
        Lines({Rail,Rail+FVector2D(Layout.IconSize-6,0)},FLinearColor(0,0,0,.75f),4.f);
        Lines({Rail,Rail+FVector2D((Layout.IconSize-6)*FMath::Clamp(Fraction,0.f,1.f),0)},Color,2.f);
    }
    Elements.PopClip();
    return Layer;
}

#undef LOCTEXT_NAMESPACE
