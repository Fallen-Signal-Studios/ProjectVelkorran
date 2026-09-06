// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovProtectionRuntimeTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

USovProtectionRuntimeGunfire::USovProtectionRuntimeGunfire()
{
	bAutoReleasePayload = false;
	BurstShotCount = 1;
	SpreadDegrees = 0.f;
	TraceRadius = 0.f;
	MaximumRange = 2000.f;
	FallbackMuzzleOffset = FVector(100.f, 0.f, 0.f);
	MuzzleSocketNames.Reset();
	GunshotPresentationClass = nullptr;
	CooldownDuration = 0.f;
	PostFireRecovery = 0.05f;
	DamagePerShot = 10.f;
	PoiseDamagePerShot = 0.f;
}
void USovProtectionRuntimeGunfire::Finish()
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
void USovProtectionRuntimeObserver::TargetResolved(const FSovDamageResult&)
{
	if (!bNestOnNextTargetResult || !Source.IsValid() || !Target.IsValid()) return;
	bNestOnNextTargetResult = false;
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Source.Get(), Source.Get());
	FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.f);
	Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Channel_Kinetic);
	Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 1.f);
	ASC->ApplyGameplayEffectSpecToTarget(Spec, Target->GetNarrativeAbilitySystemComponent());
}
void USovProtectionRuntimeObserver::SourceResolved(const FSovDamageResult& Result)
{
	SourceResults.Add(Result);
}
void USovProtectionRuntimeObserver::EchoAwarded(float Amount, float, ESovTarrikEchoAwardType Type, AActor*)
{
	if (Type != ESovTarrikEchoAwardType::ProtectionIntercept) return;
	++ProtectionAwardCount;
	ProtectionAwardTotal += Amount;
	if (CancelOnAward.IsValid()) { CancelOnAward->Finish(); CancelOnAward.Reset(); }
}
