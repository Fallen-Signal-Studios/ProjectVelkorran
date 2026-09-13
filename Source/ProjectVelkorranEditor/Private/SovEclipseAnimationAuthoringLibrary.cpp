// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovEclipseAnimationAuthoringLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Editor.h"
#include "EditorAnimUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

FString USovEclipseAnimationAuthoringLibrary::AdaptCopiedAnimation(UAnimBlueprint* Blueprint, UBlendSpace* BlendSpace,
    USkeleton* Skeleton, const TArray<UAnimationAsset*>& Sources, const TArray<UAnimationAsset*>& Replacements)
{
    const FString EclipseAnimationFolder = TEXT("/Game/Aurelion/Enemies/Animation/");
    if (!GEditor || GEditor->PlayWorld || !Blueprint || !BlendSpace || !Skeleton
        || !Blueprint->GetOutermost()->GetName().StartsWith(EclipseAnimationFolder)
        || !BlendSpace->GetOutermost()->GetName().StartsWith(EclipseAnimationFolder)
        || Blueprint->GetName() == TEXT("ABP_EclipseLinkbound")
        || Sources.Num() == 0 || Sources.Num() != Replacements.Num())
    { return TEXT("ERROR: requires copied Aurelion animation assets outside PIE"); }
    TMap<UAnimationAsset*, UAnimationAsset*> Map;
    for (int32 Index = 0; Index < Sources.Num(); ++Index)
    {
        if (!Sources[Index] || !Replacements[Index]
            || (Replacements[Index] != BlendSpace && Replacements[Index]->GetSkeleton() != Skeleton))
        { return TEXT("ERROR: missing animation or replacement skeleton mismatch"); }
        Map.Add(Sources[Index], Replacements[Index]);
    }
    TArray<UAnimationAsset*> References;
    EditorAnimUtils::GetAllAnimationSequencesReferredInBlueprint(Blueprint, References);
    for (UAnimationAsset* Asset : References)
    {
        if (Asset && Asset->GetSkeleton() != Skeleton && !Map.Contains(Asset))
        { return FString::Printf(TEXT("ERROR: unmapped graph animation %s"), *Asset->GetPathName()); }
    }
    for (const FBlendSample& Sample : BlendSpace->GetBlendSamples())
    {
        if (Sample.Animation && !Map.Contains(Sample.Animation))
        { return TEXT("ERROR: unmapped blend sample"); }
    }
    Blueprint->Modify();
    BlendSpace->Modify();
    BlendSpace->SetSkeleton(Skeleton);
    BlendSpace->ReplaceReferredAnimations(Map);
    BlendSpace->PostEditChange();
    Blueprint->TargetSkeleton = Skeleton;
    EditorAnimUtils::ReplaceReferredAnimationsInBlueprint(Blueprint, Map);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Results);
    FString Report = FString::Printf(TEXT("%s: errors=%d warnings=%d\n"),
        Results.NumErrors == 0 && Blueprint->Status != BS_Error ? TEXT("OK") : TEXT("ERROR"),
        Results.NumErrors, Results.NumWarnings);
    for (const auto& Message : Results.Messages) { Report += Message->ToText().ToString() + TEXT("\n"); }
    return Report;
}

TArray<FName> USovEclipseAnimationAuthoringLibrary::MeshBones(USkeletalMesh* Mesh)
{
    TArray<FName> Names;
    if (Mesh)
    {
        for (const FMeshBoneInfo& Bone : Mesh->GetRefSkeleton().GetRefBoneInfo()) { Names.Add(Bone.Name); }
    }
    return Names;
}
