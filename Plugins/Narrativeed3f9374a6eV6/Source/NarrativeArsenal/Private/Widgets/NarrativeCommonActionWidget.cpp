// Copyright Narrative Tools 2025.


#include "Widgets/NarrativeCommonActionWidget.h"
#include "CommonInputSubsystem.h"

void UNarrativeCommonActionWidget::UpdateActionWidget()
{
	if (GetWorld())
	{
		const UCommonInputSubsystem* CommonInputSubsystem = GetInputSubsystem();
		if (IsDesignTime() || (GetGameInstance() && CommonInputSubsystem && CommonInputSubsystem->ShouldShowInputKeys()))
		{
			Super::UpdateActionWidget();
		}
	}
}
