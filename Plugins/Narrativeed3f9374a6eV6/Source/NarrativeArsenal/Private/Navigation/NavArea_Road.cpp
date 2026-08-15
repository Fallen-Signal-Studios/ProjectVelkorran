// Copyright Narrative Tools 2025.


#include "NavArea_Road.h"


UNavArea_Road::UNavArea_Road(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DrawColor = FColor(75, 50, 150);	// brownish
	DefaultCost = 0.5f;
}
