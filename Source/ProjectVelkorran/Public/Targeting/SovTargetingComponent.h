// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Camera/SovCameraControlComponent.h"
#include "SovTargetingComponent.generated.h"
class ANarrativePlayerController;
class ANarrativeCharacter;

UENUM(BlueprintType)
enum class ESovLockLossReason : uint8 { None, Cancelled, InvalidTarget, Distance, Occluded, Navigation, Cinematic, OwnerUnavailable };
/** What a designation press produced. Marked and CommandTarget are the two protagonist reward windows. */
UENUM(BlueprintType)
enum class ESovDesignationResult : uint8 { Refused, Marked, CommandTarget, Defended };
/** Why camera framing is not being driven, which is not by itself a reason to lose the lock. */
UENUM(BlueprintType)
enum class ESovFramingSuspension : uint8 { None, Aiming, Occluded };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovLockTargetChanged, AActor*, Target, ESovLockLossReason, Reason);

/** Native framing over Narrative's camera, never actor translation or a competing camera actor.
 * Eligible duel/elite actors carry authored actor tag Sov.Target.HardLock. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovTargetingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USovTargetingComponent();
	UFUNCTION(BlueprintPure, Category="Sovereign|Targeting") AActor* GetLockedTarget() const;
	UFUNCTION(BlueprintCallable, Category="Sovereign|Targeting") bool ToggleHardLock();
	UFUNCTION(BlueprintCallable, Category="Sovereign|Targeting") bool CycleTarget(bool bRight);
	UFUNCTION(BlueprintCallable, Category="Sovereign|Targeting") void ClearHardLock();
	/** Designates the focused threat and orders the companion onto it. With no threat focused, the
	 * companion is instead ordered to defend the ally under the reticle, or the protagonist. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Sovereign|Targeting")
	ESovDesignationResult DesignateFocus(FString& Reason);
	UFUNCTION(BlueprintPure, Category="Sovereign|Targeting") AActor* GetDesignatedTarget() const { return DesignatedTarget.Get(); }
	/** Aiming keeps the threat focus and hands framing back to the weapon; it is never a lock loss. */
	UFUNCTION(BlueprintPure, Category="Sovereign|Targeting") ESovFramingSuspension GetFramingSuspension() const { return FramingSuspension; }
	UPROPERTY(BlueprintAssignable, Category="Sovereign|Targeting") FSovLockTargetChanged OnLockTargetChanged;
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Targeting", meta=(ClampMin="100")) float MaximumLockDistance = 2500.f;
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Targeting", meta=(ClampMin="0",ClampMax="2")) float OcclusionTimeoutSeconds = .4f;
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Targeting", meta=(ClampMin="1",ClampMax="180")) float MaximumCameraDegreesPerSecond = 90.f;
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Targeting") bool bRequireNavigationRelationship = true;
	/** How closely the protagonist must be looking at an ally for a defend order to select them. */
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Targeting", meta=(ClampMin="0.5",ClampMax="1")) float DefendReticleTightness = .93f;
	static FName HardLockPermissionTag() { return TEXT("Sov.Target.HardLock"); }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* TickFunction) override;
private:
	friend class FSovTargetingWorldTest;
	bool ResolveController();
	/** Owner may hold and change a threat focus: possessed, viewing itself, alive, unpaused, accepting input. */
	bool CanHoldFocus() const;
	/** Owner additionally permits native framing. Aiming withdraws framing without withdrawing the focus. */
	bool CanControlCamera() const;
	bool IsAiming() const;
	bool IsValidTarget(ANarrativeCharacter* Target, bool bCheckLOS, bool bCheckNavigation, ESovLockLossReason& Reason) const;
	bool HasLineOfSight(ANarrativeCharacter* Target) const;
	bool HasNavigationRelationship(ANarrativeCharacter* Target) const;
	TArray<ANarrativeCharacter*> CollectTargets() const;
	void SetTarget(ANarrativeCharacter* Target, ESovLockLossReason Reason);
	void TryAimSnap();
	class ANarrativeCharacter* FindAllyUnderReticle() const;
	class USovCompanionComponent* ResolveCompanionCommands() const;
	void RotateCameraToward(const FVector& Point, float Delta, float Strength);
	/** Keeps this component's single camera claim matching what it is actually doing. */
	void PublishCameraClaim();
	class USovCameraControlComponent* ResolveCameraControl() const;
	UFUNCTION() void HandleSemanticInput(FGameplayTag Tag, bool bPressed);
	UPROPERTY(Transient) TWeakObjectPtr<ANarrativePlayerController> Controller;
	UPROPERTY(Transient) TWeakObjectPtr<ANarrativeCharacter> LockedTarget;
	UPROPERTY(Transient) TWeakObjectPtr<AActor> DesignatedTarget;
	bool bHasPublishedLock = false;
	bool bEndingPlay = false;
	bool bWasAiming = false;
	ESovFramingSuspension FramingSuspension = ESovFramingSuspension::None;
	float OccludedFor = 0.f;
	float NavigationElapsed = 0.f;
	/** The lease this component holds on the camera, and the request it currently stands for. */
	FGuid CameraClaim;
	FSovCameraRequest HeldRequest;
};
