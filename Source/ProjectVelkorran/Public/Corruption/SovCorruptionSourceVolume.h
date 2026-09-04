// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SovCorruptionComponent.h"
#include "SovCorruptionSourceVolume.generated.h"

/** Contact producer only; enemy hit/Corruption damage-channel producers remain separate integrations. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovCorruptionSourceVolume : public AActor
{
	GENERATED_BODY()
public:
	ASovCorruptionSourceVolume();
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Corruption") bool SetCorruptionProfile(USovCorruptionProfile* NewProfile);
	UFUNCTION(BlueprintPure, Category="Corruption") USovCorruptionProfile* GetCorruptionProfile() const { return Profile; }
	bool ValidateContact(AActor* Target, float& OutFalloff) const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Corruption") TObjectPtr<class USphereComponent> ExposureSphere;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") TObjectPtr<USovCorruptionProfile> Profile;
private:
	friend struct FSovCorruptionTestAccess;
	void RefreshContacts();
	void ReleaseContacts();
	TMap<TWeakObjectPtr<USovCorruptionComponent>, FSovCorruptionSourceHandle> Contacts;
};
