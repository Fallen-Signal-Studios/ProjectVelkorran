// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovCoActionAnchor.generated.h"

class ASovPlayerCharacterBase;
class USovCampaignStateComponent;
class USovCompanionComponent;
class USceneComponent;

/** Authored, permission-gated companion mark. Native arrival is the only source of its one-use receipt. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovCoActionAnchor : public AActor
{
	GENERATED_BODY()
public:
	ASovCoActionAnchor();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Co Action") FName AnchorId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Co Action") FName MissionId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Co Action") FName CompletionBeat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Co Action") FName RequiredCompanionId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Co Action") TObjectPtr<USceneComponent> CompanionMark;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Co Action", meta = (ClampMin = "1")) float RequestRange = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Co Action", meta = (ClampMin = "1")) float ReachRadius = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Co Action", meta = (ClampMin = "0")) float HoldAtMarkSeconds = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Co Action", meta = (ClampMin = "1")) float TimeoutSeconds = 12.f;
	/** Optional one-time correction, usable only when source and destination are occluded to every player camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Co Action|Fallback") TObjectPtr<AActor> HiddenFallbackAnchor;
	bool ValidatePermission(ASovPlayerCharacterBase* Player, FName CompanionId, FString& Reason) const;
	FVector GetCompanionLocation() const;
	bool IsCompanionAtMark(const AActor* CompanionActor) const;
private:
	friend class USovCompanionComponent;
	friend struct FSovCoActionTestAccess;
	friend class USovCampaignStateComponent;
	bool CommitArrival(USovCompanionComponent* Companion, ASovPlayerCharacterBase* Player, FGuid RequestId);
	bool ConsumeReceipt(USovCampaignStateComponent* State);
	UPROPERTY(Transient) TObjectPtr<USovCompanionComponent> ReceiptCompanion;
	UPROPERTY(Transient) TObjectPtr<ASovPlayerCharacterBase> ReceiptPlayer;
	FGuid ReceiptRequestId;
	bool bReceiptAvailable = false;
};
