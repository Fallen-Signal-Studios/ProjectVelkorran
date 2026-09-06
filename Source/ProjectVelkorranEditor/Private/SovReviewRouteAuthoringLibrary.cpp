// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovReviewRouteAuthoringLibrary.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "UObject/Package.h"
FSovBlueprintAuthoringResult USovReviewRouteAuthoringLibrary::SetReviewObjectiveStrings(UStringTable* Table, const TMap<FString, FString>& Entries)
{
    FSovBlueprintAuthoringResult Result;
    const FString RequiredPath(TEXT("/Game/Campaign/Development/TDDReview/ST_ReviewObjectives.ST_ReviewObjectives"));
    if (!IsValid(Table) || Table->GetPathName() != RequiredPath || Table->GetStringTableId() != FName(*RequiredPath)
        || Table->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient) || Table->GetOutermost() == GetTransientPackage())
    { Result.Report = TEXT("Only the exact project-owned technical-review string table may be authored."); return Result; }
    if (Entries.IsEmpty() || Entries.Num() > 32)
    { Result.Report = TEXT("Expected one to 32 review strings."); return Result; }
    for (const auto& Entry : Entries)
    {
        if (Entry.Key.IsEmpty() || Entry.Key.Len() > 128 || Entry.Value.IsEmpty() || Entry.Value.Len() > 2048
            || Entry.Key.Contains(TEXT("\n")) || Entry.Key.Contains(TEXT("\r")))
        { Result.Report = TEXT("Invalid review string key or source length."); return Result; }
    }
    Table->Modify();
    const FStringTableRef Mutable = Table->GetMutableStringTable();
    Mutable->SetNamespace(TEXT("Sov.TDDReview"));
    for (const auto& Entry : Entries) { Mutable->SetSourceString(Entry.Key, Entry.Value); }
    Table->MarkPackageDirty();
    Result.bSucceeded = true;
    Result.Report = FString::Printf(TEXT("Updated %d technical-review entries; package is not saved."), Entries.Num());
    return Result;
}
