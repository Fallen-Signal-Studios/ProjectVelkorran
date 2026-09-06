// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovBlueprintAuthoringLibrary.h"
#include "Editor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/SecureHash.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

namespace
{
    bool IsProjectCopy(const UObject* Object)
    { return IsValid(Object) && Object->GetOutermost()->GetName().StartsWith(TEXT("/Game/")); }

    void MapClass(UClass* Source, UClass* Replacement, TMap<UObject*, UObject*>& Map)
    {
        if (!Source || !Replacement) { return; }
        Map.Add(Source, Replacement);
        if (UObject* SourceDefault = Source->GetDefaultObject(false))
        { Map.Add(SourceDefault, Replacement->GetDefaultObject()); }
        for (TFieldIterator<UFunction> It(Source, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        {
            if (UFunction* ReplacementFunction = Replacement->FindFunctionByName(It->GetFName()))
            { Map.Add(*It, ReplacementFunction); }
        }
    }
}

FString USovBlueprintAuthoringLibrary::FingerprintBlueprint(UObject* Asset)
{
    if (!IsInGameThread() || !IsValid(Cast<UBlueprint>(Asset))) { return TEXT("InvalidBlueprint"); }
    UPackage* Package = Asset->GetOutermost();
    TArray<UObject*> Objects;
    GetObjectsWithOuter(Package, Objects, true);
    Objects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
    FString Snapshot = FString::Printf(TEXT("Dirty=%d\n"), Package->IsDirty() ? 1 : 0);
    for (UObject* Object : Objects)
    {
        if (Object->GetOutermost() != Package || Object->HasAnyFlags(RF_Transient)) { continue; }
        FString Properties;
        for (TFieldIterator<FProperty> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
        {
            if (It->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient)) { continue; }
            FString Value;
            It->ExportText_InContainer(0, Value, Object, nullptr, Object, PPF_None);
            Properties += It->GetName() + TEXT("=") + Value + TEXT("\n");
        }
        FSHAHash Digest;
        FSHA1::HashBuffer(*Properties, Properties.Len() * sizeof(TCHAR), Digest.Hash);
        Snapshot += Object->GetPathName() + TEXT("|") + Object->GetClass()->GetPathName()
            + TEXT("|") + Digest.ToString() + TEXT("\n");
    }
    return Snapshot;
}

FSovBlueprintAuthoringResult USovBlueprintAuthoringLibrary::RemapProjectBlueprintReferences(
    const TArray<UObject*>& ProjectAssets, const TArray<UObject*>& SourceAssets, const TArray<UObject*>& ReplacementAssets)
{
    FSovBlueprintAuthoringResult Result;
    Result.bSucceeded = RemapProjectBlueprintReferencesInternal(ProjectAssets, SourceAssets, ReplacementAssets, Result.Report);
    return Result;
}

bool USovBlueprintAuthoringLibrary::RemapProjectBlueprintReferencesInternal(const TArray<UObject*>& ProjectAssets,
    const TArray<UObject*>& SourceAssets, const TArray<UObject*>& ReplacementAssets, FString& Report)
{
    Report.Reset();
    if (!IsInGameThread() || (GEditor && GEditor->PlayWorld))
    { Report = TEXT("End Play In Editor before remapping asset classes."); return false; }
    if (ProjectAssets.IsEmpty() || SourceAssets.Num() != ReplacementAssets.Num() || SourceAssets.IsEmpty())
    { Report = TEXT("Explicit project assets and equal nonempty source/replacement lists are required."); return false; }
    TArray<UBlueprint*> Blueprints;
    TSet<UPackage*> Packages;
    for (UObject* Asset : ProjectAssets)
    {
        UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
        if (!Blueprint || !IsProjectCopy(Blueprint))
        { Report = TEXT("Every writable asset must be an explicit /Game Blueprint copy."); return false; }
        Blueprints.AddUnique(Blueprint); Packages.Add(Blueprint->GetOutermost());
    }
    TMap<UClass*, UBlueprint*> WidgetClassCopies;
    TMap<UObject*, UObject*> Replacements;
    for (int32 Index = 0; Index < SourceAssets.Num(); ++Index)
    {
        UBlueprint* Source = Cast<UBlueprint>(SourceAssets[Index]);
        UBlueprint* Replacement = Cast<UBlueprint>(ReplacementAssets[Index]);
        if (!Source || !IsProjectCopy(Replacement) || Source == Replacement
            || !Blueprints.Contains(Replacement) || !Source->GeneratedClass || !Replacement->GeneratedClass)
        { Report = TEXT("Each source Blueprint requires a distinct generated /Game replacement in the writable list."); return false; }
        if (Source->GeneratedClass->IsChildOf(UWidget::StaticClass()))
        { WidgetClassCopies.Add(Source->GeneratedClass, Replacement); }
        Replacements.Add(Source, Replacement);
        MapClass(Source->GeneratedClass, Replacement->GeneratedClass, Replacements);
        MapClass(Source->SkeletonGeneratedClass, Replacement->SkeletonGeneratedClass, Replacements);
    }
    // The archive updates class-valued properties but cannot change a live widget's
    // UObject class. Replace authored WidgetTree templates with the engine's editor
    // operation, which preserves properties, parent slots, names, and graph references.
    // Referenced widgets present the normal confirmation dialog in an interactive editor.
    TMap<UWidgetBlueprint*, TMap<FName, UBlueprint*>> ExpectedWidgetCopies;
    for (UBlueprint* Blueprint : Blueprints)
    {
        UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
        if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree) { continue; }
        TArray<UWidget*> Widgets;
        WidgetBlueprint->WidgetTree->GetAllWidgets(Widgets);
        for (UWidget* Widget : Widgets)
        {
            if (Widget->GetOutermost() != Blueprint->GetOutermost())
            { Report += TEXT("A writable WidgetTree contains an external template; remap stopped.\n"); return false; }
            if (UBlueprint* const* Replacement = WidgetClassCopies.Find(Widget->GetClass()))
            { ExpectedWidgetCopies.FindOrAdd(WidgetBlueprint).Add(Widget->GetFName(), *Replacement); }
        }
    }
    for (const auto& BlueprintEntry : ExpectedWidgetCopies)
    {
        UWidgetBlueprint* WidgetBlueprint = BlueprintEntry.Key;
        for (const auto& WidgetEntry : BlueprintEntry.Value)
        {
            UWidget* ExistingWidget = WidgetBlueprint->WidgetTree->FindWidget(WidgetEntry.Key);
            UClass* ReplacementClass = WidgetEntry.Value->GeneratedClass;
            if (!ExistingWidget || !ReplacementClass)
            { Report += TEXT("Widget template or generated replacement disappeared.\n"); return false; }
            UE_LOG(LogTemp, Display, TEXT("Velkorran authoring: replace %s.%s with %s; confirm the widget replacement dialog if shown."),
                *WidgetBlueprint->GetPathName(), *WidgetEntry.Key.ToString(), *ReplacementClass->GetPathName());
            FWidgetBlueprintEditorUtils::ReplaceWidgets(WidgetBlueprint, { ExistingWidget }, ReplacementClass,
                FWidgetBlueprintEditorUtils::EReplaceWidgetNamingMethod::MaintainNameAndReferencesForUnmatchingClass);
            UWidget* ReplacedWidget = WidgetBlueprint->WidgetTree->FindWidget(WidgetEntry.Key);
            if (!ReplacedWidget || ReplacedWidget->GetClass() != WidgetEntry.Value->GeneratedClass)
            { Report += FString::Printf(TEXT("Widget replacement declined or failed: %s.%s\n"),
                *WidgetBlueprint->GetPathName(), *WidgetEntry.Key.ToString()); return false; }
            Report += FString::Printf(TEXT("Replaced widget %s.%s with %s.\n"), *WidgetBlueprint->GetPathName(),
                *WidgetEntry.Key.ToString(), *ReplacedWidget->GetClass()->GetPathName());
        }
    }
    // Widget replacement regenerates skeletons; rebuild the map using current classes.
    Replacements.Reset();
    for (int32 Index = 0; Index < SourceAssets.Num(); ++Index)
    {
        UBlueprint* Source = CastChecked<UBlueprint>(SourceAssets[Index]);
        UBlueprint* Replacement = CastChecked<UBlueprint>(ReplacementAssets[Index]);
        Replacements.Add(Source, Replacement);
        MapClass(Source->GeneratedClass, Replacement->GeneratedClass, Replacements);
        MapClass(Source->SkeletonGeneratedClass, Replacement->SkeletonGeneratedClass, Replacements);
    }
    // Generated classes and CDOs are package siblings of the Blueprint, so visit
    // every contained object explicitly. External plugin packages are never visited.
    int64 ChangedReferences = 0;
    for (UPackage* Package : Packages)
    {
        TArray<UObject*> Objects;
        GetObjectsWithOuter(Package, Objects, true);
        for (UObject* Object : Objects)
        {
            if (Object->GetOutermost() != Package) { continue; }
            // Rebuild compiler products through the Blueprint compiler. Rewriting a
            // generated class's SuperStruct in place leaves its live layout stale.
            if (Object->IsA<UClass>() || Object->GetTypedOuter<UClass>()) { continue; }
            Object->Modify();
            FArchiveReplaceObjectRef<UObject> Archive(Object, Replacements,
                EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
            ChangedReferences += Archive.GetCount();
        }
        Package->MarkPackageDirty();
    }
    Report += FString::Printf(TEXT("Remapped %lld references in %d project packages.\n"), ChangedReferences, Packages.Num());
    // Rebuild parents and referenced widget classes before their child/container graphs.
    TArray<UBlueprint*> Pending = Blueprints;
    bool bSucceeded = true;
    while (!Pending.IsEmpty())
    {
        int32 ReadyIndex = Pending.IndexOfByPredicate([&Pending](const UBlueprint* Blueprint)
        {
            UBlueprint* Parent = Blueprint->ParentClass ? Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy) : nullptr;
            if (Parent && Pending.Contains(Parent)) { return false; }
            // A container's generated member types come from its widget templates.
            // Compile those classes (and their new parents) before reconstructing
            // the container's pins, otherwise valid copied inheritance looks incompatible.
            const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
            bool bWidgetClassPending = false;
            if (WidgetBlueprint && WidgetBlueprint->WidgetTree)
            {
                WidgetBlueprint->WidgetTree->ForEachWidget([&Pending, &bWidgetClassPending](UWidget* Widget)
                {
                    UBlueprint* WidgetClass = Cast<UBlueprint>(Widget->GetClass()->ClassGeneratedBy);
                    bWidgetClassPending |= WidgetClass && Pending.Contains(WidgetClass);
                });
            }
            return !bWidgetClassPending;
        });
        if (ReadyIndex == INDEX_NONE)
        { Report += TEXT("Project copies contain a Blueprint parent or WidgetTree class dependency cycle.\n"); return false; }
        UBlueprint* Blueprint = Pending[ReadyIndex]; Pending.RemoveAt(ReadyIndex);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Results);
        Report += FString::Printf(TEXT("%s: errors=%d warnings=%d\n"), *Blueprint->GetPathName(), Results.NumErrors, Results.NumWarnings);
        for (const auto& Message : Results.Messages) { Report += Message->ToText().ToString() + TEXT("\n"); }
        bSucceeded &= Results.NumErrors == 0 && Blueprint->Status != BS_Error;
    }
    for (const auto& BlueprintEntry : ExpectedWidgetCopies)
    {
        for (const auto& WidgetEntry : BlueprintEntry.Value)
        {
            UWidget* Widget = BlueprintEntry.Key->WidgetTree->FindWidget(WidgetEntry.Key);
            if (!Widget || Widget->GetClass() != WidgetEntry.Value->GeneratedClass)
            {
                Report += FString::Printf(TEXT("Final widget class verification failed: %s.%s\n"),
                    *BlueprintEntry.Key->GetPathName(), *WidgetEntry.Key.ToString());
                bSucceeded = false;
            }
        }
    }
    return bSucceeded;
}
