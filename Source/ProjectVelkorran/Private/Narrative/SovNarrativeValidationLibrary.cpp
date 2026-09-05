// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Narrative/SovNarrativeValidationLibrary.h"
#include "Narrative/SovNarrativeGraphPolicy.h"
#include "Narrative/SovCampaignNarrativeAdapters.h"
#include "Tales/Dialogue.h"
#include "Tales/DialogueSM.h"
#include "Sound/SoundBase.h"
#include "Internationalization/Text.h"

bool USovNarrativeValidationLibrary::ValidateDialogue(UDialogue* Dialogue, USovCampaignDefinition* Mission,
    bool bShippingValidation, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
    OutErrors.Reset(); OutWarnings.Reset();
    if (!IsValid(Dialogue) || !IsValid(Dialogue->RootDialogue))
    { OutErrors.Add(TEXT("Dialogue requires a valid Narrative graph and root node.")); return false; }
    const auto Nodes = Dialogue->GetNodes();
    if (Nodes.IsEmpty() || Nodes.Num() > 4096 || !Nodes.Contains(Dialogue->RootDialogue))
    { OutErrors.Add(TEXT("Dialogue root must belong to a bounded graph of 1..4096 nodes.")); return false; }
    TMap<const UDialogueNode*, int32> Indices;
    TSet<FName> Ids, Speakers;
    Speakers.Add(Dialogue->PlayerSpeakerInfo.GetSpeakerID());
    for (const auto& Speaker : Dialogue->Speakers)
    {
        const FName Id = Speaker.GetSpeakerID();
        if (Id.IsNone() || Speakers.Contains(Id)) { OutErrors.Add(TEXT("Dialogue speaker roles must be named and unique.")); }
        Speakers.Add(Id);
    }
    for (int32 Index = 0; Index < Nodes.Num(); ++Index)
    {
        const auto* Node = Nodes[Index];
        if (!IsValid(Node) || Indices.Contains(Node) || Node->GetID().IsNone() || Ids.Contains(Node->GetID()))
        { OutErrors.Add(TEXT("Dialogue nodes must be valid, unique and have unique stable IDs.")); continue; }
        Indices.Add(Node, Index); Ids.Add(Node->GetID());
    }
    if (Indices.Num() != Nodes.Num()) { return false; }
    std::vector<std::vector<std::size_t>> Edges(static_cast<std::size_t>(Nodes.Num()));
    const auto CheckCondition = [&OutErrors, &OutWarnings, Mission](const UNarrativeCondition* Condition)
    {
        if (!IsValid(Condition)) { OutErrors.Add(TEXT("Dialogue has a missing condition object.")); return; }
        if (const auto* Native = Cast<USovCampaignNarrativeCondition>(Condition))
        { FString Error; if (!Native->ValidateConfiguration(Mission, Error)) { OutErrors.Add(Error); } }
        else { OutWarnings.AddUnique(TEXT("Non-campaign conditions require their own semantic knowledge/character-presence review.")); }
    };
    for (int32 Index = 0; Index < Nodes.Num(); ++Index)
    {
        const auto* Node = Nodes[Index];
        const FString Label = Node->GetID().ToString();
        for (const auto* Condition : Node->Conditions) { CheckCondition(Condition); }
        for (const auto* Event : Node->Events)
        {
            if (!IsValid(Event)) { OutErrors.Add(Label + TEXT(": missing event object.")); continue; }
            for (const auto* Condition : Event->Conditions) { CheckCondition(Condition); }
            if (const auto* Native = Cast<USovCampaignNarrativeEvent>(Event))
            { FString Error; if (!Native->ValidateConfiguration(Mission, Error)) { OutErrors.Add(Label + TEXT(": ") + Error); } }
            else { OutWarnings.AddUnique(TEXT("Non-campaign events require their own write and save-refire audit.")); }
        }
        if (const auto* NPC = Cast<UDialogueNode_NPC>(Node); NPC && !NPC->IsRoutingNode() && !Speakers.Contains(NPC->GetSpeakerID()))
        { OutErrors.Add(Label + TEXT(": spoken line references an absent speaker role.")); }
        if (const auto* NPC = Cast<UDialogueNode_NPC>(Node))
        {
            if (!FMath::IsFinite(NPC->ReplyPressureSeconds) || NPC->ReplyPressureSeconds < 0.f || NPC->ReplyPressureSeconds > 300.f)
            { OutErrors.Add(Label + TEXT(": response pressure must be finite and within 0..300 seconds; zero means no timer.")); }
            if (NPC->ReplyPressureSeconds > 0.f)
            {
                const UDialogueNode_Player* Silence = nullptr;
                int32 SilenceMatches = 0;
                for (const UDialogueNode_Player* Reply : NPC->PlayerReplies)
                {
                    if (IsValid(Reply) && !NPC->SilenceReplyID.IsNone() && Reply->GetID() == NPC->SilenceReplyID)
                    { Silence = Reply; ++SilenceMatches; }
                }
                if (SilenceMatches != 1 || !Silence || Silence->IsAutoSelect() || !Silence->Conditions.IsEmpty()
                    || Silence->GetOptionText(Dialogue).IsEmpty())
                { OutErrors.Add(Label + TEXT(": pressure requires exactly one direct, unconditional, non-auto-selected silence reply with readable intent text.")); }
            }
        }
        if (!Node->DirectedAtSpeakerID.IsNone() && !Speakers.Contains(Node->DirectedAtSpeakerID))
        { OutErrors.Add(Label + TEXT(": directed listener is not a declared participant.")); }
        bool bHasFallbackLine = false;
        const auto CheckLine = [&](const FDialogueLine& Line)
        {
            bHasFallbackLine |= Line.Conditions.IsEmpty();
            for (const auto& Condition : Line.Conditions) { CheckCondition(Condition.Get()); }
            if (bShippingValidation && !Line.Text.IsEmpty())
            {
                FName Table; FString Key;
                if (!FTextInspector::GetTableIdAndKey(Line.Text, Table, Key) || Table.IsNone() || Key.IsEmpty())
                { OutErrors.Add(Label + TEXT(": shipping text requires a stable string-table entry.")); }
            }
            if (Line.DialogueSound && Line.Text.IsEmpty())
            { OutErrors.Add(Label + TEXT(": voiced line requires complete subtitle text.")); }
            if (Line.Duration == ELineDuration::LD_WhenAudioEnds && !Line.DialogueSound)
            { OutErrors.Add(Label + TEXT(": audio-driven line has no audio.")); }
            if (Line.Duration == ELineDuration::LD_Never && (Dialogue->bUnskippable || !Node->bIsSkippable))
            { OutErrors.Add(Label + TEXT(": infinite unskippable line blocks progression.")); }
            if (Line.Duration == ELineDuration::LD_AfterDuration)
            {
                if (!FMath::IsFinite(Line.DurationSecondsOverride) || Line.DurationSecondsOverride <= 0.0f)
                { OutErrors.Add(Label + TEXT(": explicit line duration must be finite and positive.")); }
                if (Line.DialogueSound)
                {
                    const float SpokenSeconds = Line.DialogueSound->GetDuration();
                    if (FMath::IsFinite(SpokenSeconds) && SpokenSeconds > 0.0f && SpokenSeconds < 3600.0f
                        && Line.DurationSecondsOverride + 0.01f < SpokenSeconds)
                    { OutErrors.Add(Label + TEXT(": subtitle duration ends before the complete spoken line.")); }
                }
            }
            if (Line.Shot) { OutWarnings.AddUnique(TEXT("Sequence bindings, blocking and timing require authored participant validation in the actual map.")); }
        };
        CheckLine(Node->Line);
        for (const auto& Alternative : Node->AlternativeLines) { CheckLine(Alternative); }
        if (!bHasFallbackLine) { OutErrors.Add(Label + TEXT(": conditional line variants require an unconditional fallback.")); }
        bool bHasFallbackExit = false;
        const auto AddEdge = [&](const UDialogueNode* Next)
        {
            const int32* NextIndex = Indices.Find(Next);
            if (!NextIndex) { OutErrors.Add(Label + TEXT(": reply edge leaves the declared graph.")); return; }
            Edges[static_cast<std::size_t>(Index)].push_back(static_cast<std::size_t>(*NextIndex));
            bHasFallbackExit |= Next->Conditions.IsEmpty();
        };
        for (const auto* Next : Node->NPCReplies) { AddEdge(Next); }
        for (const auto* Next : Node->PlayerReplies) { AddEdge(Next); }
        if ((!Node->NPCReplies.IsEmpty() || !Node->PlayerReplies.IsEmpty()) && !bHasFallbackExit)
        { OutErrors.Add(Label + TEXT(": all outgoing replies are conditional; author an unconditional legal fallback.")); }
    }
    const auto Graph = SovNarrativeGraphPolicy::Analyze(Edges, static_cast<std::size_t>(Indices.FindChecked(Dialogue->RootDialogue)));
    if (!Graph.ValidEdges) { OutErrors.Add(TEXT("Dialogue exceeds the native graph bound or contains an invalid edge.")); }
    else
    {
        for (int32 Index = 0; Index < Nodes.Num(); ++Index)
        {
            if (!Graph.Reachable[static_cast<std::size_t>(Index)]) { OutErrors.Add(Nodes[Index]->GetID().ToString() + TEXT(": node is unreachable from the root.")); }
            else if (!Graph.CanExit[static_cast<std::size_t>(Index)]) { OutErrors.Add(Nodes[Index]->GetID().ToString() + TEXT(": path enters a cycle without an exit.")); }
        }
    }
    return OutErrors.IsEmpty();
}
