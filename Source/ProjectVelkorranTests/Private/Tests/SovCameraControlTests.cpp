// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Camera/SovCameraControlComponent.h"

#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/**
	 * Arbitration needs no world: the component is asked who has the camera and answers from its own
	 * claims. Constructing it bare keeps these tests about the rules rather than about a fixture.
	 */
	USovCameraControlComponent* MakeControl(TArray<FSovCameraProfile> Profiles = {})
	{
		auto* Control = NewObject<USovCameraControlComponent>(GetTransientPackage());
		Control->Profiles = MoveTemp(Profiles);
		Control->RefreshProfile();
		return Control;
	}

	FSovCameraRequest Request(const ESovCameraPriority Priority, const FName Reason,
		const ESovCameraStyle Style = ESovCameraStyle::Unchanged,
		const ESovCameraMode Mode = ESovCameraMode::Unchanged,
		const ESovCameraShoulder Shoulder = ESovCameraShoulder::Unchanged)
	{
		FSovCameraRequest Out;
		Out.Priority = Priority; Out.Reason = Reason; Out.Style = Style; Out.Mode = Mode; Out.Shoulder = Shoulder;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCameraArbitrationTest, "ProjectVelkorran.Camera.Arbitration.PriorityAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCameraArbitrationTest::RunTest(const FString& Parameters)
{
	FSovCameraProfile Identity;
	Identity.Protagonist = FSovGameplayTags::Get().Character_Player_Tarrik;
	Identity.Style = ESovCameraStyle::Close;
	// There is no owning pawn here, so the profile resolves through the unnamed fallback entry rather
	// than the identity one - which is the behaviour a pawn without a profile has to get.
	FSovCameraProfile Fallback;
	Fallback.Style = ESovCameraStyle::Far;
	const TStrongObjectPtr<USovCameraControlComponent> Control(MakeControl({ Identity, Fallback }));

	TestEqual(TEXT("The profile is itself a claim, so there is always something to fall back to"), Control->GetClaimCount(), 1);
	TestEqual(TEXT("The profile settles the baseline"), Control->GetCameraState().Style, ESovCameraStyle::Far);

	const FGuid Focus = Control->RequestCamera(
		Request(ESovCameraPriority::ThreatFocus, TEXT("Focus"), ESovCameraStyle::Unchanged, ESovCameraMode::Strafe));
	TestEqual(TEXT("A claim takes the camera from the profile"), Control->GetCameraState().Priority, ESovCameraPriority::ThreatFocus);
	TestEqual(TEXT("Threat focus asks for strafing"), Control->GetCameraState().Mode, ESovCameraMode::Strafe);
	TestEqual(TEXT("A field the claim did not name keeps the protagonist's own framing"), Control->GetCameraState().Style, ESovCameraStyle::Far);

	const FGuid Aim = Control->RequestCamera(Request(ESovCameraPriority::Aim, TEXT("Aim"), ESovCameraStyle::Close));
	TestEqual(TEXT("Aim outranks threat focus"), Control->GetCameraState().Priority, ESovCameraPriority::Aim);
	TestEqual(TEXT("Aim pulls the camera in"), Control->GetCameraState().Style, ESovCameraStyle::Close);
	TestEqual(TEXT("The claim underneath still contributes what the winner left unnamed"), Control->GetCameraState().Mode, ESovCameraMode::Strafe);

	const FGuid Scene = Control->RequestCamera(
		Request(ESovCameraPriority::Cinematic, TEXT("Scene"), ESovCameraStyle::FirstPerson, ESovCameraMode::FreeCam));
	TestEqual(TEXT("A scene takes the camera outright"), Control->GetCameraState().Style, ESovCameraStyle::FirstPerson);
	TestEqual(TEXT("...and nothing beneath it overrides a field the scene named"), Control->GetCameraState().Mode, ESovCameraMode::FreeCam);

	TestTrue(TEXT("Releasing a claim reports that it was held"), Control->ReleaseCamera(Scene));
	TestEqual(TEXT("Releasing hands the camera to the next claim down"), Control->GetCameraState().Priority, ESovCameraPriority::Aim);
	TestFalse(TEXT("Releasing twice is not a second release"), Control->ReleaseCamera(Scene));
	TestFalse(TEXT("An invalid handle releases nothing"), Control->ReleaseCamera(FGuid()));

	Control->ReleaseCamera(Aim);
	Control->ReleaseCamera(Focus);
	TestEqual(TEXT("With every claimant gone the profile still holds the camera"), Control->GetCameraState().Priority, ESovCameraPriority::Profile);
	TestEqual(TEXT("...at the framing it asked for"), Control->GetCameraState().Style, ESovCameraStyle::Far);
	TestEqual(TEXT("...and it is the only claim left"), Control->GetClaimCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCameraTieBreakTest, "ProjectVelkorran.Camera.Arbitration.TiesAndUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCameraTieBreakTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<USovCameraControlComponent> Control(MakeControl());

	const FGuid First = Control->RequestCamera(Request(ESovCameraPriority::Aim, TEXT("First"), ESovCameraStyle::Close));
	Control->RequestCamera(Request(ESovCameraPriority::Aim, TEXT("Second"), ESovCameraStyle::Balanced));
	const FGuid Second = [&Control]
	{
		// Recover the second handle from the description rather than holding it, so the ordering the
		// diagnostics report is the same ordering the resolver used.
		const TArray<FString> Lines = Control->DescribeClaims();
		FString Handle;
		for (const FString& Line : Lines)
		{
			if (!Line.Contains(TEXT("reason=Second"))) { continue; }
			FString Tail; Line.Split(TEXT("handle="), nullptr, &Tail); Tail.Split(TEXT(" "), &Handle, nullptr);
		}
		FGuid Parsed; FGuid::Parse(Handle, Parsed); return Parsed;
	}();
	TestTrue(TEXT("The description carries a handle that can actually be used"), Second.IsValid());
	TestEqual(TEXT("Between equals the most recent claim wins"), Control->GetCameraState().Reason, FName(TEXT("Second")));

	// An update is the same claimant changing its mind, so it must not jump ahead of a claim that
	// arrived after it - otherwise a system refreshing every frame would permanently hold the camera.
	TestTrue(TEXT("A held claim can be updated"),
		Control->UpdateCamera(First, Request(ESovCameraPriority::Aim, TEXT("First"), ESovCameraStyle::Far)));
	TestEqual(TEXT("Updating does not jump the queue"), Control->GetCameraState().Reason, FName(TEXT("Second")));
	TestFalse(TEXT("An unheld handle cannot be updated"), Control->UpdateCamera(FGuid::NewGuid(), FSovCameraRequest()));

	TestTrue(TEXT("The recovered handle releases the claim it names"), Control->ReleaseCamera(Second));
	TestEqual(TEXT("The earlier claim is still holding what it changed to"), Control->GetCameraState().Style, ESovCameraStyle::Far);

	// Raising priority through an update is how a system escalates without giving up its lease.
	Control->UpdateCamera(First, Request(ESovCameraPriority::Finisher, TEXT("Finisher"), ESovCameraStyle::Close));
	TestEqual(TEXT("An update may escalate"), Control->GetCameraState().Priority, ESovCameraPriority::Finisher);

	const TArray<FString> Described = Control->DescribeClaims();
	TestEqual(TEXT("Every live claim is described"), Described.Num(), 2);
	TestTrue(TEXT("The strongest claim is named first, so a stuck camera says what is holding it"),
		Described[0].Contains(TEXT("Finisher")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCameraPublicationTest, "ProjectVelkorran.Camera.Arbitration.PublishesOnlyRealChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCameraPublicationTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<USovCameraControlComponent> Control(MakeControl());
	const int32 Baseline = Control->GetPublishedRevision();

	const FGuid Aim = Control->RequestCamera(Request(ESovCameraPriority::Aim, TEXT("Aim"), ESovCameraStyle::Close));
	TestEqual(TEXT("Taking the camera is a change"), Control->GetPublishedRevision(), Baseline + 1);

	// A claim that loses changes nothing anyone can see, and re-applying the winner would restart the
	// authored blend. The rig must only hear about states that actually differ.
	Control->RequestCamera(Request(ESovCameraPriority::Traversal, TEXT("Traversal"), ESovCameraStyle::Far));
	TestEqual(TEXT("A claim that loses does not disturb the camera"), Control->GetPublishedRevision(), Baseline + 1);

	Control->UpdateCamera(Aim, Request(ESovCameraPriority::Aim, TEXT("Aim"), ESovCameraStyle::Close));
	TestEqual(TEXT("Restating the same request is not a change"), Control->GetPublishedRevision(), Baseline + 1);

	Control->ReleaseCamera(Aim);
	TestEqual(TEXT("Losing the winner is a change"), Control->GetPublishedRevision(), Baseline + 2);
	TestEqual(TEXT("...and the claim underneath is what the rig is now asked to apply"),
		Control->GetCameraState().Style, ESovCameraStyle::Far);
	return true;
}

#endif
