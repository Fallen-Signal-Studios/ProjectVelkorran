// Copyright Epic Games, Inc. All Rights Reserved.

#include "NarrativeArsenal.h"
#include "AI/NarrativeAIStartupDiagnostics.h"
#include "NarrativeGameplayTags.h"
#include "Navigation/NavigatorGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "GameplayTagsManager.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Vehicles/Mass/MassVehicle.h"

#if WITH_GAMEPLAY_DEBUGGER
#include <GameplayDebugger.h>
#include "UnrealFramework/GameplayDebuggerCategory_NChar.h"
#endif 


DEFINE_LOG_CATEGORY(LogNarrativeNavigator);
DEFINE_LOG_CATEGORY(LogMassVehicle);

#define LOCTEXT_NAMESPACE "FNarrativeArsenalModule"

void FNarrativeArsenalModule::StartupModule()
{
	FNarrativeAIStartupDiagnostics::Startup();
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("ActivityComponent", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_ActivityComponent::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate, 1);
	GameplayDebuggerModule.RegisterCategory("NarrativeCharacter", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_NarrativeCharacter::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebuggerModule.RegisterCategory("InteractionSlots", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_Interactable::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif

	FNarrativeGameplayTags::InitializeNativeTags();
	FNavigatorGameplayTags::InitializeNativeTags();
	FSovGameplayTags::InitializeNativeTags();
	UGameplayTagsManager::Get().DoneAddingNativeTags(); 
	
}

void FNarrativeArsenalModule::ShutdownModule()
{
	FNarrativeAIStartupDiagnostics::Shutdown();
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("ActivityComponent");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNarrativeArsenalModule, NarrativeArsenal)
