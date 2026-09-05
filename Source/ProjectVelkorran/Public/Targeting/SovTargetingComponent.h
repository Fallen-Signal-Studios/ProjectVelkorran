// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SovTargetingComponent.generated.h"
class ANarrativePlayerController;
class ANarrativeCharacter;

UENUM(BlueprintType)
enum class ESovLockLossReason : uint8 { None, Cancelled, InvalidTarget, Distance, Occluded, Navigation, Cinematic, OwnerUnavailable };
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
	UPROPERTY(BlueprintAssignable, Category="Sovereign|Targeting") FSovLockTargetChanged OnLockTargetChanged;
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Targeting", meta=(ClampMin="100")) float MaximumLockDistance = 2500.f;
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Targeting", meta=(ClampMin="0",ClampMax="2")) float OcclusionTimeoutSeconds = .4f;
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Targeting", meta=(ClampMin="1",ClampMax="180")) float MaximumCameraDegreesPerSecond = 90.f;
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Targeting") bool bRequireNavigationRelationship = true;
	static FName HardLockPermissionTag() { return TEXT("Sov.Target.HardLock"); }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* TickFunction) override;
private:
	friend class FSovTargetingWorldTest;
	bool ResolveController();
	bool CanControlCamera() const;
	bool IsValidTarget(ANarrativeCharacter* Target, bool bCheckLOS, bool bCheckNavigation, ESovLockLossReason& Reason) const;
	bool HasLineOfSight(ANarrativeCharacter* Target) const;
	bool HasNavigationRelationship(ANarrativeCharacter* Target) const;
	TArray<ANarrativeCharacter*> CollectTargets() const;
	void SetTarget(ANarrativeCharacter* Target, ESovLockLossReason Reason);
	void TryAimSnap();
	void RotateCameraToward(const FVector& Point, float Delta, float Strength);
	UFUNCTION() void HandleSemanticInput(FGameplayTag Tag, bool bPressed);
	UPROPERTY(Transient) TWeakObjectPtr<ANarrativePlayerController> Controller;
	UPROPERTY(Transient) TWeakObjectPtr<ANarrativeCharacter> LockedTarget;
	bool bHasPublishedLock = false;
	bool bEndingPlay = false;
	bool bWasAiming = false;
	float OccludedFor = 0.f;
	float NavigationElapsed = 0.f;
};
