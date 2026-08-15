// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Items/NarrativeItem.h"
#include "IDetailCustomization.h"

/**
 * Custom details panel for NarrativeItem to allow for thumbnail creation 
 */
class NARRATIVEARSENALEDITOR_API FItemDetailsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	
	TWeakObjectPtr<UNarrativeItem> Target;
	
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	//Bit of fun - in the editor we can quick-generate item thumbnails by rendering their mesh.
	//TODO wants the angle to capture from to be passed in as a parameter, and provide multiple buttons to try capture from top-side-bottom, etc angles. 
	void GenerateThumbnail();
};
