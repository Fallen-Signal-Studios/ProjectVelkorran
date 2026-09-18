// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Camera/SovCameraControlComponent.h"

#include "Characters/SovPlayerCharacterBase.h"
#include "Misc/ScopeExit.h"

namespace SovCameraPolicy
{
	/**
	 * Settles a set of claims into one state.
	 *
	 * Claims are applied from the weakest upward, each overwriting only the fields it actually names, so
	 * a claim that cares about distance and not about shoulder leaves the shoulder where the profile put
	 * it. The strongest claim - highest priority, most recent on a tie - names the state's owner, which
	 * is what DescribeClaims and the diagnostics report.
	 *
	 * Free of any world or component so the arbitration itself is testable in isolation.
	 */
	template <typename ClaimType>
	FSovCameraState Resolve(TArrayView<const ClaimType> Claims, const FSovCameraState& Base)
	{
		FSovCameraState Out = Base;
		Out.Priority = ESovCameraPriority::Profile;
		Out.Reason = NAME_None;

		TArray<const ClaimType*, TInlineAllocator<8>> Ordered;
		Ordered.Reserve(Claims.Num());
		for (const ClaimType& Claim : Claims) { Ordered.Add(&Claim); }
		Ordered.Sort([](const ClaimType& A, const ClaimType& B)
		{
			if (A.Request.Priority != B.Request.Priority) { return A.Request.Priority < B.Request.Priority; }
			return A.Serial < B.Serial;
		});

		for (const ClaimType* Claim : Ordered)
		{
			const FSovCameraRequest& Request = Claim->Request;
			if (Request.Style != ESovCameraStyle::Unchanged) { Out.Style = Request.Style; }
			if (Request.Mode != ESovCameraMode::Unchanged) { Out.Mode = Request.Mode; }
			if (Request.Shoulder != ESovCameraShoulder::Unchanged) { Out.Shoulder = Request.Shoulder; }
			Out.Priority = Request.Priority;
			Out.Reason = Request.Reason;
		}
		return Out;
	}
}

USovCameraControlComponent::USovCameraControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Arbitration is authoritative on whoever is looking through the camera: this is presentation, and
	// a client's own camera is the one that matters to them. It deliberately does not replicate.
	SetIsReplicatedByDefault(false);
}

void USovCameraControlComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshProfile();
}

void USovCameraControlComponent::EnsureProfile()
{
	// A claim may arrive before BeginPlay - a component built by a test, or a system that runs early -
	// and the fallback has to already be underneath it when it does.
	if (ProfileHandle.IsValid() || bEstablishingProfile) { return; }
	RefreshProfile();
}

FGuid USovCameraControlComponent::RequestCamera(const FSovCameraRequest& Request)
{
	EnsureProfile();
	FClaim& Claim = Claims.AddDefaulted_GetRef();
	Claim.Handle = FGuid::NewGuid();
	Claim.Request = Request;
	Claim.Serial = ++SerialCounter;
	Resolve();
	return Claim.Handle;
}

bool USovCameraControlComponent::ReleaseCamera(const FGuid Handle)
{
	if (!Handle.IsValid()) { return false; }
	const int32 Removed = Claims.RemoveAll([&Handle](const FClaim& Claim) { return Claim.Handle == Handle; });
	if (Removed <= 0) { return false; }
	Resolve();
	return true;
}

bool USovCameraControlComponent::UpdateCamera(const FGuid Handle, const FSovCameraRequest& Request)
{
	FClaim* const Existing = Claims.FindByPredicate([&Handle](const FClaim& Claim) { return Claim.Handle == Handle; });
	if (!Existing) { return false; }
	// The serial is kept: an update is the same claim changing its mind, not a new one jumping the queue.
	Existing->Request = Request;
	Resolve();
	return true;
}

void USovCameraControlComponent::RefreshProfile()
{
	// Held for the whole call so the RequestCamera below does not re-enter through EnsureProfile and
	// leave an orphaned claim behind the one this function ends up owning.
	bEstablishingProfile = true;
	ON_SCOPE_EXIT { bEstablishingProfile = false; };

	FSovCameraRequest Request;
	Request.Priority = ESovCameraPriority::Profile;
	Request.Reason = TEXT("Profile");

	FGameplayTag Identity;
	if (const ASovPlayerCharacterBase* const Player = Cast<ASovPlayerCharacterBase>(GetOwner()))
	{ Identity = Player->GetProtagonistIdentityTag(); }

	// An entry with no protagonist tag is the fallback, so a pawn without an identity - or one that has
	// not been given a profile - still ends up with a named baseline rather than a silent default.
	const FSovCameraProfile* Match = Profiles.FindByPredicate(
		[&Identity](const FSovCameraProfile& Profile) { return Identity.IsValid() && Profile.Protagonist == Identity; });
	if (!Match)
	{
		Match = Profiles.FindByPredicate([](const FSovCameraProfile& Profile) { return !Profile.Protagonist.IsValid(); });
	}
	if (Match)
	{
		Request.Style = Match->Style;
		Request.Mode = Match->Mode;
		Request.Shoulder = Match->Shoulder;
		if (Match->Protagonist.IsValid()) { Request.Reason = Match->Protagonist.GetTagName(); }
	}

	if (ProfileHandle.IsValid() && UpdateCamera(ProfileHandle, Request)) { return; }
	ProfileHandle = RequestCamera(Request);
}

TArray<FString> USovCameraControlComponent::DescribeClaims() const
{
	TArray<const FClaim*, TInlineAllocator<8>> Ordered;
	for (const FClaim& Claim : Claims) { Ordered.Add(&Claim); }
	Ordered.Sort([](const FClaim& A, const FClaim& B)
	{
		if (A.Request.Priority != B.Request.Priority) { return A.Request.Priority > B.Request.Priority; }
		return A.Serial > B.Serial;
	});

	TArray<FString> Out;
	Out.Reserve(Ordered.Num());
	for (const FClaim* Claim : Ordered)
	{
		Out.Add(FString::Printf(TEXT("%s reason=%s handle=%s serial=%llu"),
			*StaticEnum<ESovCameraPriority>()->GetNameStringByValue(static_cast<int64>(Claim->Request.Priority)),
			*Claim->Request.Reason.ToString(), *Claim->Handle.ToString(EGuidFormats::DigitsWithHyphens), Claim->Serial));
	}
	return Out;
}

void USovCameraControlComponent::Resolve()
{
	const FSovCameraState Next = SovCameraPolicy::Resolve<FClaim>(Claims, FSovCameraState());
	if (Next == Resolved) { return; }
	Resolved = Next;
	++PublishedRevision;
	// Only a real change reaches the rig: re-applying an identical state would restart the authored
	// blends every time any claimant came or went behind the winner.
	ApplyCameraState(Resolved);
	OnCameraStateChanged.Broadcast(Resolved);
}
