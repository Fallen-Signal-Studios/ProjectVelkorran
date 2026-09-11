// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Characters/SovDroneNPCBase.h"
#include "Components/ActorComponent.h"
#include "Components/SovCommandLinkComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "Components/SovAurelionThermalFractureComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "NarrativeSavableComponent.h"
#include "SovAurelionEnemyRoles.generated.h"

class ANarrativeNPCController;
class ASovEncounterDirector;
class UCharacterMovementComponent;
class UNarrativeAbilitySystemComponent;
class USovWeakPointComponent;
class USovAurelionThermalFractureComponent;
class USovPoiseComponent;

/** Capsule-centre route along a real wall and onto a landing, authored in local space.
 * No smart-link teleport or damage payload; the BT task owns continuous swept movement. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionWallRoute : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionWallRoute();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Traversal") FName RouteId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Traversal") TArray<FVector> LocalPoints;
    /** Local vector from first climb segment toward the physical wall. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Traversal") FVector WallProbeDirection = FVector(0., 1., 0.);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Traversal") float WallProbeDistance = 160.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Traversal") float Speed = 300.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Traversal") float EntryTolerance = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Traversal") float MaximumDuration = 8.f;
    /** Geometry-only validation, also used before saving authored routes. */
    UFUNCTION(BlueprintPure, Category="Aurelion|Traversal") bool ValidateRoute(FString& Error) const;
    TArray<FVector> GetWorldPoints() const;
};

UENUM(BlueprintType)
enum class ESovAurelionTraversalResult : uint8 { Unavailable, Running, Completed, Cancelled, Blocked };

/** Passive component: no tick or independent decision loop. A selected Narrative activity's BT owns a lease. */
UCLASS(ClassGroup=(Sovereign), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovAurelionWallTraversalComponent : public UActorComponent, public INarrativeSavableComponent
{
    GENERATED_BODY()
public:
    USovAurelionWallTraversalComponent();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Traversal") TObjectPtr<ASovAurelionWallRoute> Route;
    UFUNCTION(BlueprintPure, Category="Aurelion|Traversal") bool CanBeginTraversal() const;
    UFUNCTION(BlueprintPure, Category="Aurelion|Traversal") bool IsTraversing() const { return bTraversing; }
    UFUNCTION(BlueprintPure, Category="Aurelion|Traversal") bool HasCompletedRoute() const { return bRouteCompleted; }
    UFUNCTION(BlueprintPure, Category="Aurelion|Traversal") ESovAurelionTraversalResult GetLastResult() const { return LastResult; }
    /** Non-reflected lease APIs intentionally keep task identity out of Blueprint gameplay. */
    uint64 BeginTraversal(UObject* RequestOwner);
    ESovAurelionTraversalResult AdvanceTraversal(UObject* RequestOwner, uint64 Lease, float DeltaSeconds);
    bool CancelTraversal(UObject* RequestOwner, uint64 Lease);
    virtual void PrepareForSave_Implementation() override;
    virtual void Load_Implementation() override;
    virtual bool WasSaveRecordLoadAccepted() const override { return bAcceptedRestore; }
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    bool HasReadyOwner(bool bContinuing, bool bMovementAcquired = true) const;
    bool Owns(UObject* RequestOwner, uint64 Lease) const;
    bool ValidatePhysicalRoute(TArray<FVector>& Points) const;
    bool HasWalkableLanding(const FVector& Point) const;
    void FinishTraversal(uint64 Lease, ESovAurelionTraversalResult Result);
    void OnTraversalBusyChanged(FGameplayTag Tag, int32 NewCount,
        TWeakObjectPtr<UNarrativeAbilitySystemComponent> ExpectedASC, uint64 ExpectedLease);
    UPROPERTY(Replicated, Transient) bool bTraversing = false;
    UPROPERTY(Transient) ESovAurelionTraversalResult LastResult = ESovAurelionTraversalResult::Unavailable;
    TWeakObjectPtr<UObject> LeaseOwner;
    TWeakObjectPtr<ASovNPCCharacterBase> ActiveCharacter;
    TWeakObjectPtr<ANarrativeNPCController> ActiveController;
    TWeakObjectPtr<UNarrativeAbilitySystemComponent> ActiveASC;
    TWeakObjectPtr<UCharacterMovementComponent> ActiveMovement;
    TWeakObjectPtr<ASovAurelionWallRoute> ActiveRoute;
    TWeakObjectPtr<ASovEncounterDirector> ActiveEncounter;
    FGuid ActiveEncounterAttempt;
    FDelegateHandle BusyChangedHandle;
    UPROPERTY(SaveGame) FName SavedRouteId;
    UPROPERTY(SaveGame) bool bRouteCompleted = false;
    bool bAcceptedRestore = true;
    TArray<FVector> ActivePoints;
    uint64 Generation = 0;
    uint64 ActorInfoEpoch = 0;
    int32 PointIndex = 0;
    uint8 PriorMovementMode = 0;
    uint8 PriorCustomMode = 0;
    bool bOwnsBusy = false;
    bool bOwnsMovementMode = false;
    bool bMutating = false;
    float Elapsed = 0.f;
    float CapturedSpeed = 0.f;
    float CapturedMaximumDuration = 0.f;
};

/** Reversible resistance, never a health/shield refill or an invulnerability gate. */
UCLASS()
class PROJECTVELKORRAN_API USovAurelionWeaverWard : public UGameplayEffect
{
    GENERATED_BODY()
public:
    USovAurelionWeaverWard();
};

/** Applied once by the copied elite ability configuration after its normal base attributes. */
UCLASS()
class PROJECTVELKORRAN_API USovAurelionElitePoiseAttributes : public UGameplayEffect
{
    GENERATED_BODY()
public:
    USovAurelionElitePoiseAttributes();
};

/** Readiness-only bootstrap. Stops after a live/restored/severed instance or 30 seconds.
 * It never runs combat decisions or resets an existing link. */
UCLASS(ClassGroup=(Sovereign), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovAurelionFreshCommandLink : public USovCommandLinkComponent, public INarrativeSavableComponent
{
    GENERATED_BODY()
public:
    USovAurelionFreshCommandLink();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category="Aurelion|Link") bool bAutoInitializeFreshLink = false;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Aurelion|Link") bool InitializeFreshLink();
    virtual void PrepareForSave_Implementation() override;
    virtual void Load_Implementation() override;
    virtual bool WasSaveRecordLoadAccepted() const override { return bAcceptedRestore; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void PollReadiness();
    FTimerHandle ReadinessTimer;
    double BootstrapDeadline = 0.;
    bool bInitializing = false;
    UPROPERTY(SaveGame) FName SavedConfigurationId;
    bool bRestoredConfiguration = false;
    bool bAcceptedRestore = true;
};

/** Two independent severable anchors can each contribute one +15 Armor modifier. */
UCLASS(ClassGroup=(Sovereign), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovAurelionWeaverLink : public USovAurelionFreshCommandLink
{
    GENERATED_BODY()
public:
    USovAurelionWeaverLink();
};

/** Core identity is stable; missing actual mesh hit bones deliberately fails configuration. */
UCLASS()
class PROJECTVELKORRAN_API USovAurelionCoreWeakPoints : public USovWeakPointComponent
{
    GENERATED_BODY()
public:
    USovAurelionCoreWeakPoints();
};

/** Rebinds the same uniquely tagged physical frost anchor after native actor reconstruction. */
UCLASS()
class PROJECTVELKORRAN_API USovAurelionEliteThermalFracture : public USovAurelionThermalFractureComponent
{
    GENERATED_BODY()
public:
    virtual void PrepareForSave_Implementation() override;
    virtual void Load_Implementation() override;
    virtual bool WasSaveRecordLoadAccepted() const override { return bAcceptedAnchorRestore; }
private:
    UPROPERTY(SaveGame) FName SavedAnchorId;
    bool bAcceptedAnchorRestore = true;
};

UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionSecurityDrone : public ASovDroneNPCBase
{
    GENERATED_BODY()
public:
    ASovAurelionSecurityDrone(const FObjectInitializer& Initializer);
    UFUNCTION(BlueprintPure, Category="Aurelion|Formation") USovAurelionFreshCommandLink* GetFormationLink() const { return FormationLink; }
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USovAurelionFreshCommandLink> FormationLink;
};

/** Actual attack selection, weapon grants and damage remain in Narrative's authored melee activity/GAS. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionLinkbound : public ASovNPCCharacterBase
{
    GENERATED_BODY()
public:
    ASovAurelionLinkbound(const FObjectInitializer& Initializer);
};

UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionWallRunner : public ASovAurelionLinkbound
{
    GENERATED_BODY()
public:
    ASovAurelionWallRunner(const FObjectInitializer& Initializer);
    UFUNCTION(BlueprintPure, Category="Aurelion|Traversal") USovAurelionWallTraversalComponent* GetWallTraversal() const { return WallTraversal; }
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USovAurelionWallTraversalComponent> WallTraversal;
};

/** Backline support: severable armor tethers and bounded Narrative ally-alert sharing.
 * It does not force ally focus, manufacture direct sight or revive severed link instances. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionWeaver : public ASovNPCCharacterBase
{
    GENERATED_BODY()
public:
    ASovAurelionWeaver(const FObjectInitializer& Initializer);
    UFUNCTION(BlueprintPure, Category="Aurelion|Weaver") USovAurelionWeaverLink* GetAnchorA() const { return AnchorA; }
    UFUNCTION(BlueprintPure, Category="Aurelion|Weaver") USovAurelionWeaverLink* GetAnchorB() const { return AnchorB; }
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Aurelion|Weaver") bool InitializeFreshLinks();
    UFUNCTION(BlueprintPure, Category="Aurelion|Weaver") bool HasActiveSupportLink() const;
    /** Invoked by the support activity's task; the controller validates faction, radius and provenance again. */
    bool ShareObservedThreatWithLinkedAllies();
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USovAurelionWeaverLink> AnchorA;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USovAurelionWeaverLink> AnchorB;
private:
    bool HasReadySupportOwner() const;
    bool bUpdatingLinks = false;
};

UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionElite : public ASovAurelionLinkbound
{
    GENERATED_BODY()
public:
    ASovAurelionElite(const FObjectInitializer& Initializer);
    UFUNCTION(BlueprintPure, Category="Aurelion|Elite") USovWeakPointComponent* GetCoreWeakPoints() const { return CoreWeakPoints; }
    UFUNCTION(BlueprintPure, Category="Aurelion|Elite") USovAurelionThermalFractureComponent* GetThermalFracture() const { return ThermalFracture; }
    UFUNCTION(BlueprintPure, Category="Aurelion|Elite") USovPoiseComponent* GetElitePoise() const { return ElitePoise; }
protected:
    /** Author the verified mesh bone/material matcher. An empty definition deliberately fails validation. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USovWeakPointComponent> CoreWeakPoints;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USovAurelionThermalFractureComponent> ThermalFracture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USovPoiseComponent> ElitePoise;
};
