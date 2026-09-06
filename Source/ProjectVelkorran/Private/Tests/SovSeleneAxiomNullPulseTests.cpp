// Copyright Fallen Signal Studios. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "Abilities/SovGameplayAbility_SeleneEcho.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovSeleneAxiomNullPulseRuntimeContractTest,
	"ProjectVelkorran.Campaign.Selene.AxiomNullPulseRuntimeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovSeleneAxiomNullPulseRuntimeContractTest::RunTest(
	const FString& Parameters)
{
	const USovGameplayAbility_SeleneAxiomNullPulse* Pulse =
		GetDefault<USovGameplayAbility_SeleneAxiomNullPulse>();
	TestNotNull(TEXT("Axiom Null Pulse CDO exists"), Pulse);
	if (!Pulse)
	{
		return false;
	}

	TestTrue(
		TEXT("Axiom uses one mutable release ledger per avatar"),
		Pulse->GetInstancingPolicy()
			== EGameplayAbilityInstancingPolicy::InstancedPerActor);
	TestTrue(
		TEXT("Axiom prediction cannot terminate the authority instance"),
		Pulse->GetNetSecurityPolicy()
			== EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination);
	const UFunction* ReleaseFunction = Pulse->FindFunction(
		GET_FUNCTION_NAME_CHECKED(
			USovGameplayAbility_SeleneAxiomNullPulse,
			ReleaseAxiomNullPulseCommandLinks));
	TestNotNull(TEXT("Axiom exposes an explicit release seam"), ReleaseFunction);
	TestTrue(
		TEXT("The explicit Axiom release seam is authority-only"),
		ReleaseFunction
			&& ReleaseFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));

	TestTrue(
		TEXT("Axiom has a positive authority charge duration"),
		Pulse->GetAxiomFullChargeDuration() > 0.0f);
	TestTrue(
		TEXT("Axiom charge expands pulse range"),
		Pulse->GetAxiomPulseRange(1.0f)
			> Pulse->GetAxiomPulseRange(0.0f));
	TestTrue(
		TEXT("Axiom charge expands pulse angle"),
		Pulse->GetAxiomPulseHalfAngleDegrees(1.0f)
			> Pulse->GetAxiomPulseHalfAngleDegrees(0.0f));
	TestTrue(
		TEXT("Axiom range clamps charge below zero"),
		FMath::IsNearlyEqual(
			Pulse->GetAxiomPulseRange(-1.0f),
			Pulse->GetAxiomPulseRange(0.0f)));
	TestTrue(
		TEXT("Axiom angle clamps charge above one"),
		FMath::IsNearlyEqual(
			Pulse->GetAxiomPulseHalfAngleDegrees(2.0f),
			Pulse->GetAxiomPulseHalfAngleDegrees(1.0f)));

	const FVector Origin = FVector::ZeroVector;
	const FVector Forward = FVector::ForwardVector;
	TestTrue(
		TEXT("A command node centered in the pulse is selected"),
		USovGameplayAbility_SeleneAxiomNullPulse::IsLocationInsideAxiomPulse(
			Origin, Forward, FVector(1000.0, 0.0, 0.0), 1500.0f, 20.0f));
	TestTrue(
		TEXT("A command node on the inclusive angle boundary is selected"),
		USovGameplayAbility_SeleneAxiomNullPulse::IsLocationInsideAxiomPulse(
			Origin,
			Forward,
			FVector(1000.0, 0.0, 0.0).RotateAngleAxis(20.0, FVector::UpVector),
			1500.0f,
			20.0f));
	TestFalse(
		TEXT("A command node outside the pulse angle is rejected"),
		USovGameplayAbility_SeleneAxiomNullPulse::IsLocationInsideAxiomPulse(
			Origin,
			Forward,
			FVector(1000.0, 0.0, 0.0).RotateAngleAxis(20.5, FVector::UpVector),
			1500.0f,
			20.0f));
	TestFalse(
		TEXT("A command node outside the pulse range is rejected"),
		USovGameplayAbility_SeleneAxiomNullPulse::IsLocationInsideAxiomPulse(
			Origin, Forward, FVector(1500.1, 0.0, 0.0), 1500.0f, 20.0f));
	TestFalse(
		TEXT("A command node behind Selene is rejected"),
		USovGameplayAbility_SeleneAxiomNullPulse::IsLocationInsideAxiomPulse(
			Origin, Forward, FVector(-100.0, 0.0, 0.0), 1500.0f, 20.0f));
	TestFalse(
		TEXT("A malformed direction fails closed"),
		USovGameplayAbility_SeleneAxiomNullPulse::IsLocationInsideAxiomPulse(
			Origin, FVector::ZeroVector, FVector(100.0, 0.0, 0.0), 1500.0f, 20.0f));

	return true;
}

#endif
