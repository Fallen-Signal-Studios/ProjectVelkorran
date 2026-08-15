// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NavigationDeveloperSettings.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, meta = (DisplayName="Narrative - Navigator Settings"))
class NARRATIVEARSENAL_API UNavigationDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:

	UNavigationDeveloperSettings();

	//If we're the local player the nav marker will appear this color
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Setup")
	FLinearColor PlayerColor;

	//If we're friendly the nav marker will appear this color
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Setup")
	FLinearColor FriendlyColor;

	//If we're neutral the nav marker will appear this color
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Setup")
	FLinearColor NeutralColor;

	//If we're hostile the nav marker will appear this color
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Setup")
	FLinearColor HostileColor;

};
