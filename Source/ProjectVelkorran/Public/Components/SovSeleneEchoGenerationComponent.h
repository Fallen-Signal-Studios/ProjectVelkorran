// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "GameplayTagContainer.h"
#include "SovSeleneEchoGenerationComponent.generated.h"

class UNarrativeAbilitySystemComponent;
class USovDeflectionComponent;
class USovEchoComponent;
struct FSovCommandLinkSeverResult;

UENUM(BlueprintType)
enum class ESovSeleneEchoAwardType : uint8
{
	PerfectDeflection UMETA(DisplayName = "Perfect Deflection"),
	WeakPointBreak UMETA(DisplayName = "Weak Point Break"),
	CommandLinkSever UMETA(DisplayName = "Command Link Sever"),
	ExposureKill UMETA(DisplayName = "Exposure Kill")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FSovSeleneEchoAwardedSignature,
	float, AwardedEcho,
	float, NewEcho,
	ESovSeleneEchoAwardType, AwardType,
	FName, WeakPointId,
	AActor*, OtherActor);

/**
 * Selene-only precision Echo rules.
 *
 * The component consumes typed authoritative results. It never infers a body
 * shot as precision, never grants for Tarrik, and can consume each target-owned
 * weak-point break transaction only once.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovSeleneEchoGenerationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovSeleneEchoGenerationComponent();

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo|Selene")
	bool InitializeWithAbilitySystem(
		UNarrativeAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo|Selene")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo|Selene")
	float GetPerfectDeflectionEchoReward() const
	{
		return FMath::Max(PerfectDeflectionEchoReward, 0.0f);
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo|Selene")
	float GetWeakPointBreakEchoReward() const
	{
		return FMath::Max(WeakPointBreakEchoReward, 0.0f);
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo|Selene")
	float GetCommandLinkSeverEchoReward() const
	{
		return FMath::Max(CommandLinkSeverEchoReward, 0.0f);
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo|Selene")
	float GetExposureKillEchoReward() const
	{
		return FMath::Max(ExposureKillEchoReward, 0.0f);
	}

	/**
	 * Consumes one authoritative command-link Sever transaction. The Axiom
	 * ability routes a successful target-owned transaction here; it never
	 * writes Echo directly.
	 */
	bool ConsumeCommandLinkSever(
		const FSovCommandLinkSeverResult& SeverResult);

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo|Selene|Presentation")
	FSovSeleneEchoAwardedSignature OnSeleneEchoAwarded;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Selene|Tuning", meta = (ClampMin = "0.0"))
	float PerfectDeflectionEchoReward = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Selene|Tuning", meta = (ClampMin = "0.0"))
	float WeakPointBreakEchoReward = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Selene|Tuning", meta = (ClampMin = "0.0"))
	float CommandLinkSeverEchoReward = 12.0f;

	/** Defeating a hostile target during Selene's active Exposed window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Selene|Tuning", meta = (ClampMin = "0.0"))
	float ExposureKillEchoReward = 6.0f;

	/** Small server-only FIFO fence; damage transaction GUIDs never need campaign-lifetime retention. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Selene|Replay", meta = (ClampMin = "16", ClampMax = "2048"))
	int32 DamageReplayLedgerCapacity = 256;

private:
	void TryInitializeFromOwner();
	void UninitializeFromAbilitySystem();
	bool CanGenerateSeleneEcho(bool bAllowDuringEchoAbility = false) const;
	bool IsEchoAbilityDamage(const FSovDamageResult& DamageResult) const;
	AActor* ResolveLogicalDamageSource(const FSovDamageResult& DamageResult) const;
	bool IsHostileTarget(const AActor* TargetActor) const;
	bool ConsumeDamageRewardTransaction(
		const FGuid& TransactionId,
		TSet<FGuid>& ConsumedTransactions,
		TArray<FGuid>& TransactionOrder);
	void AwardEcho(
		float RequestedEcho,
		const FGameplayTag& SourceTag,
		ESovSeleneEchoAwardType AwardType,
		FName WeakPointId,
		AActor* OtherActor,
		bool bAllowDuringEchoAbility = false);

	UFUNCTION()
	void HandleOwnerASCInitialized();

	UFUNCTION()
	void HandlePerfectDeflection(const FSovDamageResult& DamageResult);

	UFUNCTION()
	void HandleDamageResolvedAsSource(const FSovDamageResult& DamageResult);

	UFUNCTION(Client, Unreliable)
	void ClientNotifySeleneEchoAwarded(
		float AwardedEcho,
		float NewEcho,
		ESovSeleneEchoAwardType AwardType,
		FName WeakPointId,
		AActor* OtherActor);

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<USovEchoComponent> EchoComponent;

	UPROPERTY(Transient)
	TObjectPtr<USovDeflectionComponent> DeflectionComponent;

	/** Server-only replay fence for command-link transactions. */
	TSet<FGuid> ConsumedCommandLinkSeverTransactions;

	/** Server-only replay fences for damage-result-driven rewards. */
	TSet<FGuid> ConsumedPerfectDeflectionTransactions;
	TArray<FGuid> PerfectDeflectionTransactionOrder;
	TSet<FGuid> ConsumedExposureKillTransactions;
	TArray<FGuid> ExposureKillTransactionOrder;
};
