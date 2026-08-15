// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include <Curves/CurveFloat.h>

#include "AI/Cover/CoverTypes.h"
#include "NarrativeCharacterMovement.generated.h"

UENUM(BlueprintType)
enum ENarrativeCustomMovementMode
{
	CMOVE_None			UMETA(Hidden),
	CMOVE_Climb			UMETA(DisplayName = "Climb"),
	CMOVE_Ragdoll       UMETA(DisplayName = "Ragdoll"),
	CMOVE_MAX			UMETA(Hidden),
};

USTRUCT(BlueprintType)
struct FCoverState
{
	GENERATED_BODY()

	FCoverState()
	{
		CoverTransform = FTransform();
		CoverPlayerRotation = FRotator();
		bIsShortCover = false;
		bIsLeftOpen = false;
		bIsRightOpen = false; 
	};


	// transform of the location of the cover. use GetCoverLocationAndRotation() to get the player location for the cover.
	UPROPERTY(BlueprintReadOnly, Category="Cover")
	FTransform CoverTransform;
	
	//The raw hit information from the last cover trace
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	FHitResult CoverHit;

	//Whether we're in a short cover or not. Server will crouch/stand us depending on this value. 
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	bool bIsShortCover;

	//Whether the space to the left of the cover is open, and we can lean left out of the cover. 
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	bool bIsLeftOpen;
	
	//Whether the space to the right of the cover is open, and we can lean right out of the cover. 
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	bool bIsRightOpen;

	//Cover rotation is always the same, but we need another rotator for the players cover rotation as moving along cover and aiming in/out will change this. 
	UPROPERTY(BlueprintReadWrite, Category = "Cover")
	FRotator CoverPlayerRotation;
};

/**
 * Any actors implementing this interface will be able to override how player input is handled when climbing
 */
UINTERFACE(BlueprintType)
class NARRATIVEARSENAL_API UClimbableActor : public UInterface
{
	GENERATED_BODY()
	
};

/**
 * Any actors implementing this interface will be able to override how player input is handled when climbing
 */
class NARRATIVEARSENAL_API IClimbableActor
{
	GENERATED_BODY()

public:

	//Allows the climbable actor to override which override layer we apply when in the climb. 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Climbable Actor")
	FGameplayTag GetClimbOverrideTag() const;
	virtual FGameplayTag GetClimbOverrideTag_Implementation() const;

	//Called when jump initially onto a climbable, allows climable to adjust props where the initial "Ledge" is. IE - if we hit a ladder, this can adjust ledge location to the closest rung on the ladder etc. 
	//You can also return false to notify the climb function that it should abort.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Climbable Actor")
	bool AdjustInitialWarpProps(UPARAM(ref)FAttachWarpProps& AttachWarpProps, const FHitResult& InitialClimableHit, class ANarrativeCharacter* Character, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const;
	virtual bool AdjustInitialWarpProps_Implementation(UPARAM(ref)FAttachWarpProps& AttachWarpProps, const FHitResult& InitialClimableHit, class ANarrativeCharacter* Character, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const;

	//Allows handling an input from the CMC trying to move whilst in the climb. 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Climbable Actor")
	bool TryFindClimbTransform(UPARAM(ref)FAttachWarpProps& AttachWarpProps, const FAttachWarpProps& OldProps, class ANarrativeCharacter* Character, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const;
	virtual bool TryFindClimbTransform_Implementation(UPARAM(ref)FAttachWarpProps& AttachWarpProps, const FAttachWarpProps& OldProps, class ANarrativeCharacter* Character, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const;

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCoverEvent);

/**
 * Custom movement component for Narrative Pro. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UNarrativeCharacterMovement();

protected:

	friend class ANarrativeCharacter;
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void InitializeComponent() override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override; 
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual float VisualizeMovement() const override;
	virtual bool CanAttemptJump() const override;
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	virtual void RequestPathMove(const FVector& MoveInput) override;
	virtual FString GetMovementName() const override;
	virtual bool CanWalkOffLedges() const override;

	virtual void StartNewPhysics(float deltaTime, int32 Iterations) override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual float GetMaxSpeed() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	
	virtual void PhysSwimming(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override; 
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;

	//Override these as we need to check if falling is sliding us downwards, in which case we're slipping and want ragdoll 
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

	virtual void MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult = 0) override;
	
public:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode = 0) override;

	// ======== SPRINT ========
public:

	//Units per second we should travel at when sprinting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint")
	float SprintSpeed;

	//Units per second we should travel at when walking 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walking")
	float SlowWalkSpeed;

	//Interp speed for orienting towards movement 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walking")
	float OrientToMovementInterpSpeed;

	UFUNCTION(BlueprintPure, Category = "Walking")
	bool GetUseAccelerationForPaths() const;

	/**BP exposed way to turn off use acceleration for paths*/
	UFUNCTION(BlueprintCallable, Category = "Walking")
	void SetUseAccelerationForPaths(const bool bNewAccelerationForPaths);

	uint8 bWantsSprint : 1;
	
	uint8 bWantsSlowWalk : 1;

	/**Request to start sprinting*/
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StartSprinting();

	/**Request to stop sprinting*/
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StopSprinting();

	/**Return true if we are moving forward - used by sprint ability 
	@param ForwardAngleTolerance the angle degrees we can be off by and still be considered to be moving forwards */
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	bool IsMovingForward(const float ForwardAngleTolerance=10.f) const;

	/**Return true if we are sprinting  */
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	bool IsSprinting() const;
	
	/**Return true if we are slow walking  */
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	bool IsSlowWalking() const;

	/**Set our mirror component*/
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void SetMirrorComponent(class UNarrativeCharacterMovement* MirrorCMC);

	virtual bool ShouldCheckForLedgeWhilstFalling() const;

protected:


	//We'll copy move speeds etc from this component if set. 
	UPROPERTY()
	TObjectPtr<class UNarrativeCharacterMovement> MirrorMovement;


	UPROPERTY()
	bool bIsWarping;
	
	FTimerHandle TH_IsWarping;
	
	// Movement
	// Root climb physics function, delegates to either strafe or transition physics
	virtual void PhysClimb(float deltaTime, int32 Iterations);
	virtual void PhysRagdoll(float deltaTime, int32 Iterations);


	virtual void OnEnterSwimming();
	virtual void OnExitSwimming();

	virtual void OnEnterClimbing();
	virtual void OnExitClimbing();

	virtual void OnEnterRagdoll();
	virtual void OnExitRagdoll();
public:

	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsClimbing() const { return IsCustomMovementMode(CMOVE_Climb); }

	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsRagdoll() const { return IsCustomMovementMode(CMOVE_Ragdoll); }

	UFUNCTION(BlueprintPure, Category = "NarrativeCharacterMovement")
	bool IsCustomMovementMode(ENarrativeCustomMovementMode InCustomMovementMode) const;

	UFUNCTION(BlueprintPure, Category = "NarrativeCharacterMovement")
	bool IsMovementMode(EMovementMode InMovementMode) const;

	UFUNCTION(BlueprintPure, Category = "NarrativeCharacterMovement")
	FVector2D GetLocalInputVector() const;

	UFUNCTION(BlueprintPure, Category = "NarrativeCharacterMovement")
	FORCEINLINE ANarrativeCharacter* GetNarrativeCharacterOwner() const {return NarrativeCharacterOwner;};

	class UNarrativeAnimInstance* GetCharacterAnimInstance() const;

	UFUNCTION(BlueprintPure, Category = "NarrativeCharacterMovement")
	FORCEINLINE UNarrativeCharacterMovement* GetMirrorMovement() const {return MirrorMovement;};


	// Internal Getters / Helpers
protected: 

	bool IsServer() const;
	float CapR() const;
	float CapHH() const;
	
	//Get the bottom of the capsule
	FVector CapB(const float ZOffset=0.f) const;

	//Location of the capsule 
	FVector CapLoc() const;

	//Get the ground location, that the capsule slightly floats above 
	FVector CapG(const float ZOffset) const;

	FCollisionQueryParams GetIgnoreCharacterParams() const;

	UPROPERTY(Transient, DuplicateTransient) 
	ANarrativeCharacter* NarrativeCharacterOwner;


public:

	FGameplayTag GetClimbOverrideLayerTag() const;

	//Trys to find an attach transform for an "attach warp" root motion montage
	//If successful the AttachWarpProps will be filled with all the data necessary for
	//CHT_AttachWarp to select the best montages
	UFUNCTION(BlueprintCallable, Category = "Traversal")
	bool TryFindAttachTransform(FAttachWarpProps &OutAttachWarpProps, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const;

	//Traces to be used inside TryFindAttachTransform for all of our climb cases
	bool TryFindClimbTransform(FAttachWarpProps &OutAttachWarpProps, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const;

	//Traces to be used inside TryFindAttachTransform for all of our Traversal cases
	bool TryFindTraversalTransform(FAttachWarpProps &OutAttachWarpProps, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const;
	
	bool IsClimbLocationIsValidForCharacter(const FTransform& LedgeTransform) const;

	bool IsTraversalLocationIsValidForCharacter(const FTransform& LedgeTransform) const;

	FVector FindLedgeEdge(const FHitResult& ForwardHit, const FHitResult& DownwardHit, const FTransform& ReferenceTransform, bool bFrontLedge) const;

	void PlayTraversalAnim(FAttachWarpProps InTraversalProps);

	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	
	void OnTraversalMontageEndedTraversal(UAnimMontage* Montage, bool bInterrupted, FAttachWarpProps InTraversalProps);

	bool ShouldEnterDive();

	UPROPERTY(EditDefaultsOnly, Category= "Swim")
	UAnimMontage* EnterDive;

	UPROPERTY(EditDefaultsOnly, Category= "Swim")
	UAnimMontage* EnterWater;

	UPROPERTY(EditDefaultsOnly, Category= "Ragdoll")
	UAnimMontage* RagdollGetUpFromFrontMontage;

	UPROPERTY(EditDefaultsOnly, Category= "Ragdoll")
	UAnimMontage* RagdollGetUpFromBackMontage;

	//Flips to true after a shot delay when movement mode becomes MOVE_Falling
	//this prevents us tracing the ledge immediately after jumping off    
	float BeginFallingTime;
	
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	bool  bDisableClimbAndMantles;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	bool  bCheckLedgeWhilstFalling;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float StartFallingLedgeCheckCooldown;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float TraversalTraceForwardDistance = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float TraversalTraceForwardDistanceSwimming = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float ClimbTraceJumpVerticalDistance = 140.f;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float ClimbTraceJumpHorizontalDistance = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float TraversalTraceForwardDistanceFalling = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float TraversalTraceForwardDistanceClimbCheck;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float TraversalTraceHeightMax = 325.f;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float TraversalTraceHeightMin = 50.f;
	//Scales the trace capsule half height as a factor of the character Capsule half height 
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float TraversalTraceCapsuleHalfHeightScale;
	UPROPERTY(EditDefaultsOnly, Category="Traversal")
	float TraversalTraceCapsuleHalfHeightScaleFalling;

protected:

	//Start ragdolling if falling at faster than this many units of Z
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll")
	float EnterRagdollFallZThreshold;

	//Start ragdolling if falling at faster than this many units of Z and we smash into an unwalkable slope
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll")
	float EnterRagdollFallZImpactSlopeThreshold;

	//Start ragdolling if we impact walkable ground at fast than this Z. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll")
	float EnterRagdollFallZImpactGroundThreshold;

	// Fall damage we'll take when landing given a certain Z velocity. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fall Damage")
	FRuntimeFloatCurve FallDamageCurve; 

	/* cover */
public:

	static FTransform InvalidCoverTransform;
	
protected:

	//The state of our cover - where is it, and what type of cover it is. 
	UPROPERTY(BlueprintReadWrite, Category="Cover")
	FCoverState CoverState;



	//If true, we'll try walk around corners in cover - otherwise, we'll stop the player at the corner 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Cover")
	bool bCanWalkAroundCornersInCover;

	//If true, we'll adjust crouching automatically depending on the height of the cover 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Cover")
	bool bOrientCrouchToCoverHeight;

	// distance to lean us left/right out of the cover. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Cover", meta=(ClampMin=1))
	float LeanFromCoverDist;

	// distance from cover for the player.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Cover", meta=(ClampMin=1))
	float PlayerOffsetFromCover;
	
	/* TODO: rename this */
	// distance left or right of the player to check for cover
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Cover", meta=(ClampMin=1))
	float NextCoverTraceSpacing;
	
	// How far forward we need to sweep a capsule to find some cover. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Cover", meta=(ClampMin=1))
	float FindCoverForwardSearchDist;

	// distance left or right of the player to check for cover
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Cover", meta=(ClampMin=1))
	float NextCoverTraceDepth;
	
	// distance left or right of the player to check for cover
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Cover", meta=(ClampMin=1))
	float CoverInterpSpeed;

public:
	
	//Try enter cover by tracing in the direction we are looking for cover. 
	UFUNCTION(BlueprintCallable, Category="Cover")

	bool TryEnterCover();

	UFUNCTION(BlueprintCallable, Category="Cover")
	bool SetCoverFromHit(const FHitResult& Hit);

	UFUNCTION(BlueprintCallable, Category="Cover")
	bool IsCoverValid(const FHitResult& Hit);

	UFUNCTION(BlueprintCallable, Category="Cover")
	void InvalidateCover();
	
	UFUNCTION(BlueprintPure, Category="Cover")
	void CoverToPlayer(FVector& Location, FRotator& Rotation, bool bIncludeLeaning=true) const;

	UFUNCTION(BlueprintPure, Category="Cover")
	bool HasCover() const;

	bool ShouldInvalidateCover() const;

	bool CoverMove(const FVector2D& LocalInput);

	//Return whether we're strafing to the left or to the right
	UFUNCTION(BlueprintPure, Category="Cover")
	bool IsCoverStrafingLeft() const;

	UPROPERTY(BlueprintAssignable, Category = "Cover")
	FCoverEvent OnEnterCover;

	UPROPERTY(BlueprintAssignable, Category = "Cover")
	FCoverEvent OnExitCover;

protected:

	FVector CoverRight() const;
	FVector CoverFwd() const;
	FVector CoverLoc() const;
	FRotator CoverRot() const;



	bool IsAiming() const;

};

class FSavedMove_NarrativeCharacter : public FSavedMove_Character
{
public:

	typedef FSavedMove_Character Super;

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual uint8 GetCompressedFlags() const override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;
	virtual void PrepMoveFor(class ACharacter* Character) override;
	virtual void Clear() override;

	uint8 bSavedWantsSprint : 1;
	uint8 bSavedWantsSlowWalk : 1;
};

class FNetworkPredictionData_Client_NarrativeCharacter : public FNetworkPredictionData_Client_Character
{
public:
	FNetworkPredictionData_Client_NarrativeCharacter(const UCharacterMovementComponent& ClientMovement);

	typedef FNetworkPredictionData_Client_Character Super;

	virtual FSavedMovePtr AllocateNewMove() override;
};
