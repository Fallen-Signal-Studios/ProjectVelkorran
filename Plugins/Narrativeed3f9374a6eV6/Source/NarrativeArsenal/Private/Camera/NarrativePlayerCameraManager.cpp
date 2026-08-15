// Copyright Narrative Tools 2025.

#include "Camera/NarrativePlayerCameraManager.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "ArsenalStatics.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "Items/RangedWeaponItem.h"

//#include "IXRTrackingSystem.h"
//#include "Camera/CameraModifier.h"

DECLARE_CYCLE_STAT(TEXT("NarrativeCameraManager ProcessViewRotation"), STAT_NarrativeCameraManager_ProcessViewRotation, STATGROUP_Game);


ANarrativePlayerCameraManager::ANarrativePlayerCameraManager(const FObjectInitializer& ObjectInit) : Super(ObjectInit)
{
	FirstPersonRenderScale = 1.f; 
}

void ANarrativePlayerCameraManager::ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
	Super::ProcessViewRotation(DeltaTime, OutViewRotation, OutDeltaRot);
}

void ANarrativePlayerCameraManager::DoUpdateCamera(float DeltaTime)
{
	Super::DoUpdateCamera(DeltaTime);

	FMinimalViewInfo NewPOV = GetCameraCacheView();

	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOwningPlayerController()))
	{
		if (UNarrativeGameUserSettings* GUS = UArsenalStatics::GetNarrativeGameUserSettings())
		{
			if (ANarrativePlayerCharacter* PChar = PC->GetControlledCharacter())
			{
				if (!GUS->WantsEnableBloom())
				{
					NewPOV.PostProcessSettings.bOverride_BloomIntensity = true; 
					NewPOV.PostProcessSettings.BloomIntensity = 0.f;
				}
				else
				{
					NewPOV.PostProcessSettings.bOverride_BloomIntensity = false;
				}

				if (!GUS->WantsEnableMotionBlur())
				{
					NewPOV.PostProcessSettings.bOverride_MotionBlurAmount = true; 
					NewPOV.PostProcessSettings.MotionBlurAmount = 0.f;
					NewPOV.PostProcessSettings.bOverride_MotionBlurMax = true;
					NewPOV.PostProcessSettings.MotionBlurMax = 0.f;
				}
				else
				{
					NewPOV.PostProcessSettings.bOverride_MotionBlurAmount = false;
					NewPOV.PostProcessSettings.bOverride_MotionBlurMax = false;
				}

				//Ranged weapons can aim down sights - enforce a fixed field of view so that scopes/iron sights are always framed correctly, regardless of weapon FOV. 
				if (URangedWeaponItem* EquippedWeapon = Cast<URangedWeaponItem>(PChar->GetWeapon()))
				{
					//Figure out what percentage aiming down sights we are  
					const float AimPct = FMath::GetMappedRangeValueClamped(FVector2D(EquippedWeapon->GetAimFOV() / GUS->GetFieldOfView(), 1.f), FVector2D(0.f, 1.f), EquippedWeapon->GetAimFOV() / NewPOV.FOV); 

					//When aiming down sights, we want a fixed FOV, not one that the user can change in the settings. This is because scopes etc need framed correctly by designers, and we don't want players breaking that framing. 
					const float ADSFOV = EquippedWeapon->GetAimWeaponRenderFOV();

					//Smoothly Lerp between fixed ADS weapon FOV and standard weapon FOV 
					NewPOV.FirstPersonFOV = FMath::Lerp(GUS->GetWeaponFieldOfView(), ADSFOV, AimPct);

					//Apply depth of field to the butt of the gun - removed for now as i think this may get out of hand, for example this will probably override designer intended DOF settings 
					//NewPOV.PostProcessSettings.bOverride_DepthOfFieldFstop = true;
					//NewPOV.PostProcessSettings.DepthOfFieldFstop = FMath::Lerp(15.f, EquippedWeapon->GetAimFStop(), AimPct);
					//NewPOV.PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
					//NewPOV.PostProcessSettings.DepthOfFieldFocalDistance = 1000.f;
				}
				else
				{
					NewPOV.FirstPersonFOV = GUS->GetWeaponFieldOfView();
				}
			}
		}
	}


	NewPOV.FirstPersonScale = FirstPersonRenderScale;
	NewPOV.bUseFirstPersonParameters = WantsFirstPersonRender();
	NewPOV.PerspectiveNearClipPlane = 0.01f;

	// Cache results
	FillCameraCache(NewPOV);
}

void ANarrativePlayerCameraManager::InitializeFor(class APlayerController* PC)
{
	APlayerCameraManager::InitializeFor(PC);

	OwningNarrativeController = Cast<ANarrativePlayerController>(PCOwner);

	if (OwningNarrativeController)
	{
		OwningNarrativeCharacter = OwningNarrativeController->GetNarrativeCharacter();
	}
}

bool ANarrativePlayerCameraManager::WantsFirstPersonRender() const
{
	if (OwningNarrativeController)
	{
		if (OwningNarrativeController->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().Camera_FirstPerson_DisableFirstPersonRendering))
		{
			return false; 
		}

		//Typically we only want to do first person rendering if we have a weapon out
		if (OwningNarrativeCharacter)
		{
			return IsValid(OwningNarrativeCharacter->GetWeapon());
		}
	}

	return true; 
}
