// Copyright Narrative Tools 2022. 


#include "DialogueEditorSettings.h"
#include "UObject/ConstructorHelpers.h"
#include "Tales/Dialogue.h"
#include "Tales/DialogueSM.h"
#include "DialogueNodeUserWidget.h"

UDialogueEditorSettings::UDialogueEditorSettings()
{
	RootNodeColor = FLinearColor(0.1f, 0.1f, 0.1f);
	PlayerNodeColor = FLinearColor(0.65f, 0.28f, 0.f);
	NPCNodeColor = FLinearColor(0.2f, 0.2f, 0.2f);
	BacklinkWireColor = FLinearColor(0.254970f, 0.548547f, 1.000000f, 0.800000f);

	DefaultNPCDialogueClass = UDialogueNode_NPC::StaticClass();
	DefaultPlayerDialogueClass = UDialogueNode_Player::StaticClass();
	DefaultDialogueClass = UDialogue::StaticClass();

	auto DialogueNodeUserWidgetFinder = ConstructorHelpers::FClassFinder<UDialogueNodeUserWidget>(TEXT("/Script/UMGEditor.WidgetBlueprint'/NarrativePro/Pro/Editor/UI/Widgets/Tales/WBP_DefaultDialogueNode.WBP_DefaultDialogueNode_C'"));
	if (DialogueNodeUserWidgetFinder.Succeeded())
	{
		DefaultDialogueWidgetClass = DialogueNodeUserWidgetFinder.Class;
	}

	bEnableWarnings = true;
	bWarnMissingSoundCues = true;

	ForwardSplineHorizontalDeltaRange = 1000.f;
	ForwardSplineVerticalDeltaRange = 1000.f;
	BackwardSplineHorizontalDeltaRange = 200.f;
	BackwardSplineVerticalDeltaRange = 200.f;

	ForwardSplineTangentFromHorizontalDelta = FVector2D(1.f, 0.f);
	ForwardSplineTangentFromVerticalDelta = FVector2D(1.f, 0.f);

	BackwardSplineTangentFromVerticalDelta = FVector2D(1.5f, 0.f);
	BackwardSplineTangentFromHorizontalDelta = FVector2D(2.f, 0.f);
}
