// Copyright Narrative Tools 2024. 


#include "CharacterCreator/CharacterCreatorConfiguration.h"
#include "Character/NarrativeCharacterVisual.h"

#define LOCTEXT_NAMESPACE "CharacterCreator"

UCharacterCreatorConfiguration::UCharacterCreatorConfiguration()
{

}

UCharacterCreatorSection::UCharacterCreatorSection()
{
	SectionDisplayName = LOCTEXT("SectionDisplayName", "Section Title");
}

UCharacterCreatorPage::UCharacterCreatorPage()
{
	PageTitleText = LOCTEXT("PageTitleText", "Page Title");
}

UCharacterCreatorForm::UCharacterCreatorForm()
{
	CharacterVisualClass = ANarrativeCharacterVisual::StaticClass();
	bHideBaseMesh = true; 
}


#undef LOCTEXT_NAMESPACE