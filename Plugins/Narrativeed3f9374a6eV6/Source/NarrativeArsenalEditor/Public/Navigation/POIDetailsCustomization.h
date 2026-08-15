// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class NARRATIVEARSENALEDITOR_API FPOIDetailsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
