// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SovCameraControlComponent.generated.h"

/** Mirrors Narrative's authored E_CameraStyle so C++ can name a distance without owning the rig. */
UENUM(BlueprintType)
enum class ESovCameraStyle : uint8 { Unchanged, Far, Balanced, Close, FirstPerson };

/** Mirrors Narrative's authored E_CameraMode. */
UENUM(BlueprintType)
enum class ESovCameraMode : uint8 { Unchanged, FreeCam, Strafe };

/** Which shoulder the camera sits over. The rig applies it; this decides it. */
UENUM(BlueprintType)
enum class ESovCameraShoulder : uint8 { Unchanged, Right, Left };

/**
 * Who is asking, in the order they win. A higher value takes the camera from a lower one.
 *
 * These are the claims §4.3 says one owner has to arbitrate. They are deliberately coarse: the point
 * is that two systems can never both believe they are driving, not that every caller gets a rung.
 */
UENUM(BlueprintType)
enum class ESovCameraPriority : uint8
{
	/** The protagonist's profile. Always present, always loses to anything else. */
	Profile = 0,
	Traversal = 1,
	ThreatFocus = 2,
	Aim = 3,
	Finisher = 4,
	/** A scene owns the camera outright and nothing may take it. */
	Cinematic = 5
};

/** One claim on the camera. Fields left Unchanged defer to whatever the profile already established. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCameraRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sovereign|Camera")
	ESovCameraPriority Priority = ESovCameraPriority::Profile;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sovereign|Camera")
	ESovCameraStyle Style = ESovCameraStyle::Unchanged;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sovereign|Camera")
	ESovCameraMode Mode = ESovCameraMode::Unchanged;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sovereign|Camera")
	ESovCameraShoulder Shoulder = ESovCameraShoulder::Unchanged;
	/** Names the claimant in diagnostics. A camera that is stuck should say what is holding it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sovereign|Camera")
	FName Reason;

	bool operator==(const FSovCameraRequest& Other) const
	{
		return Priority == Other.Priority && Style == Other.Style && Mode == Other.Mode
			&& Shoulder == Other.Shoulder && Reason == Other.Reason;
	}
	bool operator!=(const FSovCameraRequest& Other) const { return !(*this == Other); }
};

/** The resolved camera, with every field settled. This is what the authored rig is asked to apply. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCameraState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Camera") ESovCameraStyle Style = ESovCameraStyle::Balanced;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Camera") ESovCameraMode Mode = ESovCameraMode::FreeCam;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Camera") ESovCameraShoulder Shoulder = ESovCameraShoulder::Right;
	/** The priority that settled this state, and the reason it gave. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Camera") ESovCameraPriority Priority = ESovCameraPriority::Profile;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Camera") FName Reason;

	bool operator==(const FSovCameraState& Other) const
	{
		return Style == Other.Style && Mode == Other.Mode && Shoulder == Other.Shoulder
			&& Priority == Other.Priority && Reason == Other.Reason;
	}
	bool operator!=(const FSovCameraState& Other) const { return !(*this == Other); }
};

/** A protagonist's baseline framing: §4.4's distinction between mass and precision, as data. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCameraProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Camera", meta = (Categories = "Sov.Character.Player"))
	FGameplayTag Protagonist;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Camera")
	ESovCameraStyle Style = ESovCameraStyle::Balanced;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Camera")
	ESovCameraMode Mode = ESovCameraMode::FreeCam;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Camera")
	ESovCameraShoulder Shoulder = ESovCameraShoulder::Right;
};

namespace SovCameraPolicy
{
	/**
	 * The framing a protagonist gets before any content says otherwise.
	 *
	 * Free of a component so §4.4's distinction can be asserted directly rather than through a pawn.
	 */
	PROJECTVELKORRAN_API FSovCameraProfile BuiltInProfile(const FGameplayTag& Identity);
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovCameraStateChanged, const FSovCameraState&, State);

/**
 * The §4.3 camera owner: it arbitrates who is driving, and nothing else.
 *
 * The protagonists run on UE GameplayCameras through Narrative's authored rig - aim FOV, collision
 * offset, cover, crouch, and the hurt/stamina/ADS modifiers are all content that already works. What
 * was missing is a single answer to "who has the camera right now", so aim, threat focus, a finisher
 * and a scene could each believe they were driving.
 *
 * Claims are leases: a system asks, holds a handle, and releases it. A claimant that is destroyed or
 * forgets stops mattering as soon as its lease is released, and the resolved state falls back to the
 * next claim down - ultimately the protagonist's own profile, which is always present.
 *
 * Applying the result belongs to the rig, not here. ApplyCameraState hands the settled state to
 * Blueprint, which drives BPI_GameplayCamera. This component never moves a camera, never touches the
 * spring arm's collision test, and never writes gameplay state.
 */
UCLASS(ClassGroup = (Sovereign), meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCameraControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovCameraControlComponent();

	/**
	 * Baselines per protagonist, overriding the built-in ones. The first entry matching the current
	 * identity wins; an entry with no protagonist tag is the fallback for anyone unmatched.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Camera")
	TArray<FSovCameraProfile> Profiles;

	/** Claims the camera. Keep the handle; the claim lasts until it is released. Invalid on refusal. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Camera")
	FGuid RequestCamera(const FSovCameraRequest& Request);

	/** Releases a claim. True when this handle was actually holding one. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Camera")
	bool ReleaseCamera(FGuid Handle);

	/** Updates a claim already held, without losing its place. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Camera")
	bool UpdateCamera(FGuid Handle, const FSovCameraRequest& Request);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Camera")
	const FSovCameraState& GetCameraState() const { return Resolved; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Camera")
	int32 GetClaimCount() const { return Claims.Num(); }
	/** Counts how many times the camera has actually changed hands, not how often it was asked. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Camera")
	int32 GetPublishedRevision() const { return PublishedRevision; }

	/** One line per live claim: priority, reason and handle, for a camera that will not let go. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Camera")
	TArray<FString> DescribeClaims() const;

	/** Re-reads the protagonist profile, for a handoff that changes who is being played. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Camera")
	void RefreshProfile();

	/** The settled state, whenever it changes. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Camera")
	FSovCameraStateChanged OnCameraStateChanged;

	/** Implemented by the authored rig. Called only when the settled state actually changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Camera")
	void ApplyCameraState(const FSovCameraState& State);

protected:
	virtual void BeginPlay() override;

private:
	friend struct FSovCameraControlTestAccess;

	struct FClaim
	{
		FGuid Handle;
		FSovCameraRequest Request;
		/** Orders claims of equal priority. The most recent one wins, so a later aim beats an earlier one. */
		uint64 Serial = 0;
	};

	void Resolve();
	/** Guarantees the profile floor exists before anything else is allowed to claim the camera. */
	void EnsureProfile();

	TArray<FClaim> Claims;
	FSovCameraState Resolved;
	uint64 SerialCounter = 0;
	int32 PublishedRevision = 0;
	/** The profile claim, held for the component's whole life so there is always something to fall back to. */
	FGuid ProfileHandle;
	bool bEstablishingProfile = false;
};
