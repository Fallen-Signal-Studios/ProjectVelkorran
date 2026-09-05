// Copyright Narrative Tools 2025.


#include "Widgets/NarrativeMenu.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetNavigation.h"

namespace
{
	constexpr EUINavigation MenuDirections[] = { EUINavigation::Left, EUINavigation::Right,
		EUINavigation::Up, EUINavigation::Down, EUINavigation::Next, EUINavigation::Previous };

	EUINavigationRule ReadNarrativeMenuNavigationRule(const UWidgetNavigation& Navigation, EUINavigation Direction)
	{
		// UWidgetNavigation's convenience getter is editor-only. These runtime
		// fields hold the same authored rules in game and packaged builds.
		switch (Direction)
		{
		case EUINavigation::Up: return Navigation.Up.Rule;
		case EUINavigation::Down: return Navigation.Down.Rule;
		case EUINavigation::Left: return Navigation.Left.Rule;
		case EUINavigation::Right: return Navigation.Right.Rule;
		case EUINavigation::Next: return Navigation.Next.Rule;
		case EUINavigation::Previous: return Navigation.Previous.Rule;
		default: return EUINavigationRule::Escape;
		}
	}
}

UNarrativeMenu::UNarrativeMenu()
{

}

void UNarrativeMenu::SetMenuNavigationWrap(bool bEnabled)
{
	bMenuNavigationWrap = bEnabled;
	RefreshMenuNavigation();
}

void UNarrativeMenu::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshMenuNavigation();
}

void UNarrativeMenu::NativeDestruct()
{
	ReleaseMenuNavigation();
	Super::NativeDestruct();
}

void UNarrativeMenu::NativeOnActivated()
{
	RefreshMenuNavigation();
	Super::NativeOnActivated();
	if (IsActivated())
	{
		//Blueprint activation can replace the root or populate a different menu.
		RefreshMenuNavigation();
	}
}

void UNarrativeMenu::ReleaseMenuNavigation()
{
	UWidget* Root = NavigationRoot.Get();
	if (Root && Root->Navigation == OwnedNavigation.Get())
	{
		for (uint8 Index = 0; Index < UE_ARRAY_COUNT(MenuDirections); ++Index)
		{
			if ((OwnedWrapDirections & (1 << Index)) != 0 &&
				ReadNarrativeMenuNavigationRule(*Root->Navigation, MenuDirections[Index]) == EUINavigationRule::Wrap)
			{
				Root->SetNavigationRuleBase(MenuDirections[Index], EUINavigationRule::Escape);
			}
		}
	}
	NavigationRoot.Reset();
	OwnedNavigation.Reset();
	OwnedWrapDirections = 0;
}

void UNarrativeMenu::RefreshMenuNavigation()
{
	UWidget* Root = WidgetTree ? WidgetTree->RootWidget.Get() : nullptr;
	if (!bMenuNavigationWrap || Root != NavigationRoot.Get() ||
		(Root && Root->Navigation != OwnedNavigation.Get()))
	{
		ReleaseMenuNavigation();
	}
	if (!bMenuNavigationWrap || !Root)
	{
		return;
	}
	NavigationRoot = Root;
	for (uint8 Index = 0; Index < UE_ARRAY_COUNT(MenuDirections); ++Index)
	{
		const EUINavigationRule Rule = Root->Navigation ?
			ReadNarrativeMenuNavigationRule(*Root->Navigation, MenuDirections[Index]) : EUINavigationRule::Escape;
		if (Rule == EUINavigationRule::Escape)
		{
			Root->SetNavigationRuleBase(MenuDirections[Index], EUINavigationRule::Wrap);
			OwnedWrapDirections |= (1 << Index);
		}
		else if (Rule != EUINavigationRule::Wrap)
		{
			//An authored or runtime override has taken ownership of this direction.
			OwnedWrapDirections &= ~(1 << Index);
		}
	}
	OwnedNavigation = Root->Navigation;
}
