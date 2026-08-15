// Copyright Narrative Tools 2025.

#pragma once

#include "IDetailCustomization.h"

class FArsenalSettingsDetailCustomization : public IDetailCustomization
{
public:
	
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	
};
