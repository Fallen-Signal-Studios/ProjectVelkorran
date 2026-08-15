// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Custom details panel for NPCSpawner which currently allows for a custom button to be displayed on top of the details panel.
 */
class NARRATIVEARSENALEDITOR_API FSpawnerDetailsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
