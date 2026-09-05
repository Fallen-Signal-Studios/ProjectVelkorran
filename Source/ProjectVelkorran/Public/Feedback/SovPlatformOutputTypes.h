// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "SovPlatformOutputTypes.generated.h"

UENUM(BlueprintType)
enum class ESovHapticChannel : uint8 { Combat, Interaction, Cinematic, Ambience, UI };

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovHapticSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float Master = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float Combat = .6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float Interaction = .35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float Cinematic = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float Ambience = .3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float UI = .25f;
	float Scale(ESovHapticChannel Channel) const;
	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovHDROutputStatus
{
	GENERATED_BODY()
	/** Engine platform/RHI support query, not a saved user preference. */
	UPROPERTY(BlueprintReadOnly) bool bSupported = false;
	UPROPERTY(BlueprintReadOnly) bool bEnabled = false;
	/** Engine-selected output level, which may differ from the requested level. Zero means SDR. */
	UPROPERTY(BlueprintReadOnly) int32 PeakNits = 0;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovHapticSettingsChanged, const FSovHapticSettings&, Settings);
