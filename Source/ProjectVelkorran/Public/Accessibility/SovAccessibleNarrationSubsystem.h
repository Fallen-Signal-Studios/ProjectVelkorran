// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "SovAccessibleNarrationSubsystem.generated.h"

/** False means interrupted/cancelled, never successful reading completion. */
DECLARE_DELEGATE_TwoParams(FSovNarrationCompletion, FGuid, bool);

/** Small native backend seam; production uses UE's platform TextToSpeech factory. */
class PROJECTVELKORRAN_API ISovAccessibleSpeech
{
public:
	virtual ~ISovAccessibleSpeech() = default;
	virtual bool Speak(const FString& Text, FSimpleDelegate Finished) = 0;
	virtual void Stop() = 0;
};

/** In-game narrator, not an invented completion signal for external NVDA/VoiceOver. */
UCLASS()
class PROJECTVELKORRAN_API USovAccessibleNarrationSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	bool Announce(UObject* Owner, const FText& Text, FGuid& OutRequest,
		FSovNarrationCompletion Completion = FSovNarrationCompletion());
	void Cancel(UObject* Owner);
	UFUNCTION(BlueprintPure, Category="Sovereign|Accessibility") bool IsSupported() const;
	UFUNCTION(BlueprintPure, Category="Sovereign|Accessibility") FText GetUnavailableReason() const;
	virtual void Deinitialize() override;
private:
	friend struct FSovNarrationTestAccess;
	TSharedPtr<ISovAccessibleSpeech> CreateBackend() const;
	void Finish(FGuid Request, bool bCompleted);
	void Retire(bool bNotify);
	TSharedPtr<ISovAccessibleSpeech> Backend;
	TWeakObjectPtr<UObject> RequestOwner;
	FGuid ActiveRequest;
	FSovNarrationCompletion ActiveCompletion;
	uint64 Generation = 0;
	bool bShuttingDown = false;
#if WITH_DEV_AUTOMATION_TESTS
	TFunction<TSharedPtr<ISovAccessibleSpeech>()> TestBackendFactory;
#endif
};
