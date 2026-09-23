// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Combat/SovSelenePayload.h"
#include "Weapons/NarrativeProjectile.h"
#include "SovSeleneCombatProjectile.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

UENUM(BlueprintType)
enum class ESovSeleneProjectileMode : uint8 { Stillpoint, Wake, Dispatch };
UENUM(BlueprintType)
enum class ESovSeleneProjectilePhase : uint8 { Outbound, Field, Recalling, Returned, Expired };

/** Native tuning copied at release, independent of the ability's lifetime. */
struct PROJECTVELKORRAN_API FSovSeleneProjectileParameters
{
	FSovSelenePayloadContext Context;
	ESovSeleneProjectileMode Mode = ESovSeleneProjectileMode::Stillpoint;
	FVector Direction = FVector::ForwardVector;
	float Damage = 0.0f;
	float Poise = 0.0f;
	float DamagePerSecond = 12.0f;
	float ControlDuration = 3.5f;
	float RefreezeLockout = 3.0f;
	float Fuse = 0.8f;
	float Radius = 450.0f;
	float Range = 2200.0f;
	float CenterlineWidth = 120.0f;
	float Speed = 1800.0f;
	float GravityScale = 1.0f; // Stillpoint only; multiplied by the actual world's gravity.
	float ReturnSpeed = 2600.0f;
	float OutboundDuration = 2.5f;
	float SteeringDegrees = 180.0f;
	float ShatterBonusPoise = 30.0f;
	float MaximumLifetime = 7.0f;
};

class ASovSeleneCombatProjectile;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovSeleneProjectileFinished, ASovSeleneCombatProjectile*, Projectile, bool, bReturned);

/** Server swept payload; Blueprint children only supply mesh, audio and phase presentation. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovSeleneCombatProjectile : public ANarrativeProjectile
{
	GENERATED_BODY()
public:
	ASovSeleneCombatProjectile();
	static ASovSeleneCombatProjectile* SpawnNativePayload(TSubclassOf<ANarrativeProjectile> AuthoredClass,
		const FVector& Origin, const FSovSeleneProjectileParameters& Parameters);
	bool InitializePayload(const FSovSeleneProjectileParameters& Parameters);
	bool Recall();
	virtual FVector GetVelocity() const override { return Velocity; }
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Selene")
	FSovSeleneProjectileFinished OnPayloadFinished;
	UFUNCTION(BlueprintPure, Category = "Sovereign|Selene")
	ESovSeleneProjectilePhase GetPayloadPhase() const { return Phase; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Selene|Presentation")
	void ReceivePayloadPhaseChanged(ESovSeleneProjectileMode NewMode, ESovSeleneProjectilePhase NewPhase);
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Selene|Presentation")
	void ReceivePayloadHit(AActor* Target, bool bReturnLeg, bool bFrozen);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Selene|Presentation")
	TObjectPtr<class UStaticMeshComponent> PresentationMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Selene|Presentation|Niagara")
	TObjectPtr<UNiagaraSystem> FlightNiagaraSystem;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Selene|Presentation|Niagara")
	TObjectPtr<UNiagaraSystem> FieldNiagaraSystem;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Selene|Presentation|Niagara")
	TObjectPtr<UNiagaraSystem> RecallNiagaraSystem;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Selene|Presentation|Niagara")
	TObjectPtr<UNiagaraSystem> HitNiagaraSystem;
private:
	void PresentPhase();
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPayloadHit(FVector_NetQuantize Point);
	friend struct FSovSelenePayloadTestAccess;
	UFUNCTION()
	void OnRep_Phase();
	void SetPhase(ESovSeleneProjectilePhase NewPhase);
	void Finish(bool bReturned);
	void Advance(float DeltaSeconds);
	void ApplyField();
	void HitTarget(AActor* Target, const FHitResult& Hit, const FVector& SegmentStart);
	UPROPERTY(Replicated)
	ESovSeleneProjectileMode Mode = ESovSeleneProjectileMode::Stillpoint;
	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	ESovSeleneProjectilePhase Phase = ESovSeleneProjectilePhase::Outbound;
	FSovSeleneProjectileParameters Tuning;
	TSet<TWeakObjectPtr<AActor>> OutboundTargets;
	TSet<TWeakObjectPtr<AActor>> ReturnTargets;
	TSet<TWeakObjectPtr<AActor>> SceneryTargets;
	FVector Velocity = FVector::ZeroVector;
	FVector ReleaseOrigin = FVector::ZeroVector;
	float Age = 0.0f;
	float Travelled = 0.0f;
	float FieldAge = 0.0f;
	float FieldQueryTime = 0.0f;
	bool bInitialized = false;
	bool bFinished = false;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActivePhaseNiagara;
};
