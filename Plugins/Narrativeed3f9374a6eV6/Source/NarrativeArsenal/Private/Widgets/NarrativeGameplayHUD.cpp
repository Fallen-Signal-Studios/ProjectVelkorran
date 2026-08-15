// Copyright Narrative Tools 2025.


#include "Widgets/NarrativeGameplayHUD.h"
#include "UnrealFramework/NarrativePlayerController.h"

UNarrativeGameplayHUD::UNarrativeGameplayHUD()
{

}

void UNarrativeGameplayHUD::NativeConstruct()
{
	Super::NativeConstruct();

	OwnerPC = Cast<ANarrativePlayerController>(GetOwningPlayer());
}

UNarrativeMenu* UNarrativeGameplayHUD::OpenMenu(TSubclassOf<UNarrativeMenu> MenuClass, FGameplayTag LayerTag)
{
	if (UCommonActivatableWidgetContainerBase* ContainerToUse = GetLayerContainer(LayerTag))
	{
		if (IsValid(MenuClass) && IsValid(ContainerToUse) && IsValid(OwnerPC))
		{
			if (const UNarrativeMenu* MenuCDO = GetDefault<UNarrativeMenu>(MenuClass))
			{
				if (OwnerPC->HasAnyMatchingGameplayTags(MenuCDO->BlockTags))
				{
					return nullptr;
				}
			}

			//None of the menus in narrative ever add themselves to the screen multiple times, but we may want config for this in future 
			if (UCommonActivatableWidget* CurrentWidget = ContainerToUse->GetActiveWidget())
			{
				if (CurrentWidget->GetClass() == MenuClass)
				{
					return nullptr;
				}
			}

			return ContainerToUse->AddWidget<UNarrativeMenu>(MenuClass);
		}
	}

	return nullptr; 
}

UCommonActivatableWidgetContainerBase* UNarrativeGameplayHUD::GetLayerContainer(FGameplayTag LayerTag)
{
	return Layers.FindRef(LayerTag);
}

void UNarrativeGameplayHUD::RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerWidget)
{
	if (!IsDesignTime())
	{
		Layers.Add(LayerTag, LayerWidget);
	}
}
