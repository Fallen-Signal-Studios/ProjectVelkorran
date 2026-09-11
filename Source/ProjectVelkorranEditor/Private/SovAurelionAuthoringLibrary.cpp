// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionAuthoringLibrary.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "UObject/Package.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieScenePossessable.h"
#include "UnrealFramework/NarrativeCharacter.h"
FSovBlueprintAuthoringResult USovAurelionAuthoringLibrary::SetAurelionStrings(UStringTable* Table, const TMap<FString, FString>& Entries)
{
    FSovBlueprintAuthoringResult Result;
    const FString RequiredPath(TEXT("/Game/Aurelion/Data/ST_AurelionText.ST_AurelionText"));
    if (!IsValid(Table) || Table->GetPathName() != RequiredPath || Table->GetStringTableId() != FName(*RequiredPath)
        || Table->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient) || Table->GetOutermost() == GetTransientPackage())
    { Result.Report = TEXT("Only the exact project-owned Aurelion string table may be authored."); return Result; }
    if (Entries.IsEmpty() || Entries.Num() > 1024)
    { Result.Report = TEXT("Expected one to 1024 Aurelion strings."); return Result; }
    for (const auto& Entry : Entries)
    {
        if (Entry.Key.IsEmpty() || Entry.Key.Len() > 128 || Entry.Value.IsEmpty() || Entry.Value.Len() > 8192
            || Entry.Key.Contains(TEXT("\n")) || Entry.Key.Contains(TEXT("\r")) || Entry.Key.Contains(TEXT("\t")))
        { Result.Report = TEXT("Invalid Aurelion string key or source length."); return Result; }
    }
    Table->Modify();
    const FStringTableRef Mutable = Table->GetMutableStringTable();
    Mutable->SetNamespace(TEXT("Sov.Aurelion"));
    for (const auto& Entry : Entries) { Mutable->SetSourceString(Entry.Key, Entry.Value); }
    Table->MarkPackageDirty();
    Result.bSucceeded = true;
    Result.Report = FString::Printf(TEXT("Updated %d Aurelion entries; package is not saved."), Entries.Num());
    return Result;
}

FSovBlueprintAuthoringResult USovAurelionAuthoringLibrary::TagAurelionSequenceBinding(ULevelSequence* Sequence, FGuid Binding, FName Tag)
{
    FSovBlueprintAuthoringResult Result;
    UMovieScene* Movie = IsValid(Sequence) ? Sequence->GetMovieScene() : nullptr;
    if (!Movie || !Sequence->GetOutermost()->GetName().StartsWith(TEXT("/Game/Aurelion/Cinematics/"))
        || Sequence->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient)
        || Movie->GetOutermost() != Sequence->GetOutermost() || !Binding.IsValid() || !Movie->FindBinding(Binding)
        || Tag.IsNone() || Tag.ToString().Len() > 128)
    { Result.Report = TEXT("Expected an existing root binding in a persistent Aurelion cinematic and a bounded tag."); return Result; }
    const UE::MovieScene::FFixedObjectBindingID Fixed(Binding, MovieSceneSequenceID::Root);
    const FMovieSceneObjectBindingID Expected(Fixed);
    if (const auto* Existing = Movie->AllTaggedBindings().Find(Tag))
    {
        if (Existing->IDs.Num() != 1 || Existing->IDs[0] != Expected)
        { Result.Report = TEXT("This tag already names a different or ambiguous binding."); return Result; }
        Result.bSucceeded = true; Result.Report = TEXT("Binding tag already matches; no mutation."); return Result;
    }
    // Hero and Partner must not silently collapse onto the same root object binding.
    for (const auto& Pair : Movie->AllTaggedBindings())
    {
        if ((Pair.Key == TEXT("Hero") || Pair.Key == TEXT("Partner")) && (Tag == TEXT("Hero") || Tag == TEXT("Partner"))
            && Pair.Key != Tag && Pair.Value.IDs.Contains(Expected))
        { Result.Report = TEXT("Hero and Partner must name distinct object bindings."); return Result; }
    }
    Sequence->Modify(); Movie->Modify(); Movie->TagBinding(Tag, Fixed); Sequence->MarkPackageDirty();
    Result.bSucceeded = true; Result.Report = TEXT("Root binding tagged; package is not saved."); return Result;
}

FSovAurelionBindingResult USovAurelionAuthoringLibrary::AddAurelionCharacterBinding(ULevelSequence* Sequence, FName Tag)
{
    FSovAurelionBindingResult Result;
    UMovieScene* Movie = IsValid(Sequence) ? Sequence->GetMovieScene() : nullptr;
    if (!Movie || !Sequence->GetOutermost()->GetName().StartsWith(TEXT("/Game/Aurelion/Cinematics/"))
        || Sequence->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient)
        || Movie->GetOutermost() != Sequence->GetOutermost() || Tag.IsNone() || Tag.ToString().Len() > 128)
    { Result.Report = TEXT("Expected a persistent Aurelion cinematic and bounded participant tag."); return Result; }
    if (const auto* Existing = Movie->AllTaggedBindings().Find(Tag))
    {
        if (Existing->IDs.Num() != 1 || Existing->IDs[0].GetRelativeSequenceID() != MovieSceneSequenceID::Root)
        { Result.Report = TEXT("Existing participant tag is ambiguous or not a root binding."); return Result; }
        const auto* Possessable = Movie->FindPossessable(Existing->IDs[0].GetGuid());
        const UClass* Class = Possessable ? Possessable->GetPossessedObjectClass() : nullptr;
        if (!Class || !Class->IsChildOf(ANarrativeCharacter::StaticClass()))
        { Result.Report = TEXT("Existing participant tag does not identify a Narrative-character possessable."); return Result; }
        Result.bSucceeded = true; Result.Binding = Existing->IDs[0].GetGuid(); Result.Report = TEXT("Existing participant binding retained."); return Result;
    }
    Sequence->Modify(); Movie->Modify();
    Result.Binding = Movie->AddPossessable(Tag.ToString(), ANarrativeCharacter::StaticClass());
    Movie->TagBinding(Tag, UE::MovieScene::FFixedObjectBindingID(Result.Binding, MovieSceneSequenceID::Root));
    Sequence->MarkPackageDirty(); Result.bSucceeded = Result.Binding.IsValid();
    Result.Report = TEXT("Unbound Narrative-character participant created and tagged; package is not saved."); return Result;
}
