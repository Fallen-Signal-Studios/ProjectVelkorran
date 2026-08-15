// Copyright Narrative Tools 2025.

#pragma once

#include "Tickable.h"
#include "Tales/TalesComponent.h"

class UQuestGraphNode_Root;
class FQuestGraphEditor;

class FQuestDebugger final : public FTickableGameObject, public TSharedFromThis<FQuestDebugger>
{
	friend FQuestGraphEditor;
	
public:
	
	FQuestDebugger();
	~FQuestDebugger();
	
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FQuestDebuggerTickHelper, STATGROUP_Tickables); }

	void Setup(UQuestBlueprint* QuestBlueprint, TSharedRef<FQuestGraphEditor> OwnerEditor);

	void BindCommands(TSharedRef<FUICommandList> EditorCommandList);
	void RegisterDebuggerToolbar(UToolMenu* ToolMenu);

	static bool IsPIENotSimulating();

	bool IsDebuggerReady() const { return bIsPIEActive; }
	
private:
	
	void OnBeginPIE(const bool bIsSimulating);
	void OnEndPIE(const bool bIsSimulating);
	void OnQuestStarted(UTalesComponent* TalesComponent, UQuest* Quest, const FName& Name);
	void OnQuestEnd(UTalesComponent* TalesComponent, UQuest* Quest);

	void DebuggerJumpToNode();
	
	void SetStateActive(const FName& StateNodeID, const bool Active);
	void ResetActiveNode();
	
protected:

	uint8 bIsPIEActive : 1;
	
	TWeakObjectPtr<UTalesComponent> TalesComponentInstance;
	
	/// owning editor
	TWeakPtr<FQuestGraphEditor> EditorOwner;

	/// asset for debugging
	TWeakObjectPtr<UQuestBlueprint> QuestBlueprintAsset;

	TWeakObjectPtr<UQuest> ActiveQuest;

	TWeakObjectPtr<UQuestState> ActiveState;
	
};
