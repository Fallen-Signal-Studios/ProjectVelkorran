// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNarrativeNavigator, Log, All);

#define ObjectChannel_NarrativeProjectile ObjectTypeQuery1

#define TraceChannel_NarrativeInteraction ECC_GameTraceChannel1
#define TraceChannel_NarrativeWeapon ECC_GameTraceChannel2
#define TraceChannel_NarrativeClimbable ECC_GameTraceChannel3
#define TraceChannel_NarrativeTraversable ECC_GameTraceChannel4
#define TraceChannel_NarrativeCover ECC_GameTraceChannel5
#define TraceChannel_NarrativeProjectile ECC_GameTraceChannel6

//Can be used to do version specific fixups if needed. 
#define NARRATIVE_MAJOR_VERSION	2
#define NARRATIVE_MINOR_VERSION	4
#define NARRATIVE_PATCH_VERSION	0


class FNarrativeArsenalModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
