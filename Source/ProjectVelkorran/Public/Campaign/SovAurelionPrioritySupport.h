// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovAurelionPrioritySupport.generated.h"

class UBoxComponent;
class USovCampaignStateComponent;

UENUM(BlueprintType)
enum class ESovAurelionRescuePriority : uint8 { Unset, WestStretchers, EastWalkers };

/** Physical support access derived from the campaign journal, never a second reward ledger.
 * Place the existing one-time field cache behind WestCacheBarrier. No pickups are spawned or reset. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionPrioritySupport : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionPrioritySupport();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USceneComponent> SceneRoot;
    /** Blocks access until west stretchers receive priority. Author a visible matching gate. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<UBoxComponent> WestCacheBarrier;
    /** East priority opens this before E4; the normal route opens for either outcome at phase B. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<UBoxComponent> EastFlankBarrier;
    UFUNCTION(BlueprintPure, Category="Aurelion") ESovAurelionRescuePriority GetPriority() const;
    UFUNCTION(BlueprintPure, Category="Aurelion") bool IsWestCacheAccessible() const;
    UFUNCTION(BlueprintPure, Category="Aurelion") bool IsEastFlankOpen() const;
    /** Dialogue uses this same durable decision for M13's acknowledgment. No new consequence is awarded. */
    UFUNCTION(BlueprintPure, Category="Aurelion") FName GetAftermathConsequenceId() const;
    UFUNCTION(BlueprintCallable, Category="Aurelion") void RefreshFromCampaign();
    static ESovAurelionRescuePriority ReadPriority(const USovCampaignStateComponent* State);
    static FName OutcomeBeat(ESovAurelionRescuePriority Priority);
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    const USovCampaignStateComponent* Campaign() const;
    bool bRefreshing = false;
};
