// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeSavableComponent.h"
#include "Resonance/SovResonanceTypes.h"
#include "SovResonanceTargetComponent.generated.h"
class USovWeakPointComponent;
class USovCommandLinkComponent;
class USovResonanceComponent;

/** Authored physical context. Exposure and terminal pressure are established by native observations. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovResonanceTargetComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
public:
	USovResonanceTargetComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance") TArray<ESovResonanceType> AllowedTypes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance") FName RequiredBeat;
	/** Required interaction targets are withheld from autonomous companion damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance") bool bRequiresPlayerFinish = true;
	/** The lane's forward axis and position define a real, bounded authored advance corridor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance") TObjectPtr<AActor> CorridorAnchor;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="100",ClampMax="2500")) float CorridorLength = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="50",ClampMax="500")) float CorridorHalfWidth = 200.f;
	/** Explicit support coordinator, normally on the handler rather than its supported heavy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance") TObjectPtr<AActor> SupportLinkOwner;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance") FName SupportWeakPointId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0.1",ClampMax="3")) float CounterExtension = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="50",ClampMax="1500")) float BreachMaximumDistance = 1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="100",ClampMax="3000")) float BreachSpeed = 1800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="50",ClampMax="300")) float InteractionRange = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0.5",ClampMax="15")) float TerminalHoldSeconds = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="1",ClampMax="500")) float MaximumPressure = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0",ClampMax="2")) float ReleaseDamagePerPressure = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="50",ClampMax="1500")) float ReleaseRadius = 650.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0.1",ClampMax="10")) float CorridorSeconds = 3.f;
	/** Hold-to-interact begins from Selene's real actor; release/cancel calls EndTerminalOperation. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Resonance") bool BeginTerminalOperation(AActor* Selene, FString& Reason);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Resonance") void EndTerminalOperation();
	UFUNCTION(BlueprintPure, Category="Resonance") bool IsTerminalCompleted() const { return bTerminalCompleted; }
	bool IsExposedBy(AActor* Selene) const;
	bool CanResolve(ESovResonanceType Type, const USovResonanceComponent* Coordinator) const;
	USovCommandLinkComponent* GetSupportLink() const;
	bool IsWithinCorridor(const AActor* Actor) const;
	virtual void Load_Implementation() override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
private:
	friend class USovResonanceComponent;
	void RecordProtection(AActor* Selene, float Pressure);
	UFUNCTION() void ObserveReveal(bool bRevealed, float Remaining, AActor* Instigator);
	UPROPERTY(Transient) TObjectPtr<USovWeakPointComponent> WeakPoints;
	TWeakObjectPtr<AActor> ExposedBy;
	TWeakObjectPtr<AActor> Operator;
	float ExposureUntil = 0.f;
	float OperationStarted = 0.f;
	float ProtectedPressure = 0.f;
	UPROPERTY(SaveGame) bool bTerminalCompleted = false;
};
