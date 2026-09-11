// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovExplosionReadabilityAuthoringLibrary.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterBase.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRendererProperties.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace SovExplosionAuthoring
{
    bool IsLarge(const UNiagaraSystem* System)
    { return System && (System->GetName() == TEXT("NS_Explosion") || System->GetName() == TEXT("NS_Aurelion_DroneExplosion_Readable")); }

    bool IsAdmitted(const UNiagaraSystem* System)
    {
        if (!IsValid(System)) { return false; }
        const FString Package = System->GetOutermost()->GetName();
        return Package == TEXT("/NarrativePro/Pro/Demo/VFX/Epic/Niagara/FX_Explosions/NS_Explosion")
            || Package == TEXT("/NarrativePro/Pro/Demo/VFX/Epic/Niagara/FX_Explosions/NS_Explosion_Small")
            || Package == TEXT("/Game/Aurelion/VFX/NS_Aurelion_DroneExplosion_Readable")
            || Package == TEXT("/Game/Aurelion/VFX/NS_Aurelion_DroneImpact_Readable");
    }

    void Properties(UObject* Object, const FString& Prefix, TMap<FString,FString>& Result)
    {
        for (TFieldIterator<FProperty> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
        {
            if (It->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient)) { continue; }
            for (int32 Index=0; Index<It->ArrayDim; ++Index)
            {
                FString Value; It->ExportText_InContainer(Index,Value,Object,nullptr,Object,PPF_None);
                Result.Add(Prefix+It->GetName()+FString::Printf(TEXT("[%d]"),Index),Value);
            }
        }
    }

    TArray<UNiagaraRendererProperties*> Renderers(const FNiagaraEmitterHandle& Handle)
    {
        if (Handle.GetEmitterMode()==ENiagaraEmitterMode::Standard)
        { return Handle.GetEmitterData() ? Handle.GetEmitterData()->GetRenderers() : TArray<UNiagaraRendererProperties*>(); }
        // Read the reflected array through the public emitter base, avoiding private Niagara headers.
        UObject* Base=Handle.GetEmitterBase();
        const FArrayProperty* Property=Base ? FindFProperty<FArrayProperty>(Base->GetClass(),TEXT("RendererProperties")) : nullptr;
        const FObjectPropertyBase* Inner=Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
        TArray<UNiagaraRendererProperties*> Result;
        if (Property && Inner)
        {
            FScriptArrayHelper Array(Property,Property->ContainerPtrToValuePtr<void>(Base));
            for (int32 Index=0; Index<Array.Num(); ++Index)
            { Result.Add(Cast<UNiagaraRendererProperties>(Inner->GetObjectPropertyValue(Array.GetRawPtr(Index)))); }
        }
        return Result;
    }
}

TMap<FString,FString> USovExplosionReadabilityAuthoringLibrary::InspectExplosionSystem(UNiagaraSystem* System)
{
    TMap<FString,FString> Result;
    if (!IsInGameThread() || !SovExplosionAuthoring::IsAdmitted(System)) { return Result; }
    Result.Add(TEXT("HandleCount"),FString::FromInt(System->GetEmitterHandles().Num()));
    if (const FProperty* Property=FindFProperty<FProperty>(System->GetClass(),TEXT("ExposedParameters")))
    {
        FString Value; Property->ExportText_InContainer(0,Value,System,nullptr,System,PPF_None);
        Result.Add(TEXT("ExposedParameters"),Value);
    }
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        const FString Prefix=TEXT("Handle/")+Handle.GetName().ToString()+TEXT("/");
        Result.Add(Prefix+TEXT("Enabled"),Handle.GetIsEnabled()?TEXT("1"):TEXT("0"));
        Result.Add(Prefix+TEXT("Id"),Handle.GetId().ToString());
        Result.Add(Prefix+TEXT("Mode"),Handle.GetEmitterMode()==ENiagaraEmitterMode::Stateless?TEXT("Stateless"):TEXT("Standard"));
        Result.Add(Prefix+TEXT("Version"),Handle.GetInstance().Version.ToString());
        const UObject* Base=Handle.GetEmitterBase();
        Result.Add(Prefix+TEXT("Emitter"),GetPathNameSafe(Base));
        Result.Add(Prefix+TEXT("Owned"),IsValid(Base)&&Base->IsIn(System)?TEXT("1"):TEXT("0"));
        const auto Renderers=SovExplosionAuthoring::Renderers(Handle);
        Result.Add(Prefix+TEXT("RendererCount"),FString::FromInt(Renderers.Num()));
        for (int32 Index=0; Index<Renderers.Num(); ++Index)
        {
            auto* Renderer=Renderers[Index];
            const FString Key=Prefix+FString::Printf(TEXT("Renderer/%d/"),Index);
            Result.Add(Key+TEXT("Path"),GetPathNameSafe(Renderer));
            Result.Add(Key+TEXT("Owned"),IsValid(Renderer)&&Renderer->IsIn(System)?TEXT("1"):TEXT("0"));
            if (IsValid(Renderer))
            {
                Result.Add(Key+TEXT("Class"),Renderer->GetClass()->GetPathName());
                SovExplosionAuthoring::Properties(Renderer,Key+TEXT("Property/"),Result);
            }
        }
    }
    return Result;
}

bool USovExplosionReadabilityAuthoringLibrary::PrepareOwnedExplosionSystem(UNiagaraSystem* System,FString& Error)
{
    Error.Reset();
    if (!IsInGameThread() || (GEditor && GEditor->PlayWorld) || !SovExplosionAuthoring::IsAdmitted(System)
        || !System->GetOutermost()->GetName().StartsWith(TEXT("/Game/Aurelion/VFX/"))
        || System->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
    { Error=TEXT("Requires one exact owned editor explosion system outside PIE."); return false; }
    TSet<FString> Expected={TEXT("Explosion"),TEXT("GroundDust"),TEXT("NE_PostProcess"),TEXT("Decal_Light_Flash"),TEXT("Debris"),TEXT("SparkDebris")};
    if (SovExplosionAuthoring::IsLarge(System)) { Expected.Add(TEXT("Streamers")); Expected.Add(TEXT("Streamers_Source")); }
    TSet<FString> Actual;
    for (const auto& Handle:System->GetEmitterHandles())
    {
        const FString Name=Handle.GetName().ToString(); const UObject* Base=Handle.GetEmitterBase();
        if (!Expected.Contains(Name) || Actual.Contains(Name) || !Handle.GetIsEnabled() || !IsValid(Base) || !Base->IsIn(System)
            || (Handle.GetEmitterMode()==ENiagaraEmitterMode::Stateless)!=(Name==TEXT("Decal_Light_Flash")))
        { Error=TEXT("Unexpected name, mode, ownership, or initial enabled state."); return false; }
        const auto Renderers=SovExplosionAuthoring::Renderers(Handle);
        if (Renderers.IsEmpty()) { Error=TEXT("Every preserved emitter must retain its actual renderers."); return false; }
        for (const auto* Renderer:Renderers)
        { if (!IsValid(Renderer)||!Renderer->IsIn(System)) { Error=TEXT("Renderer is not owned by exact duplicate."); return false; } }
        Actual.Add(Name);
    }
    if (Actual.Num()!=Expected.Num()) { Error=TEXT("Missing expected emitter."); return false; }
    auto ExpectedAfter=InspectExplosionSystem(System);
    ExpectedAfter.FindChecked(TEXT("Handle/Debris/Enabled"))=TEXT("0");
    ExpectedAfter.FindChecked(TEXT("Handle/GroundDust/Enabled"))=TEXT("0");
    System->Modify();
    for (auto& Handle:System->GetEmitterHandles())
    { if (Handle.GetName()==FName(TEXT("Debris"))||Handle.GetName()==FName(TEXT("GroundDust"))) { Handle.SetIsEnabled(false,*System,false); } }
    const auto After=InspectExplosionSystem(System);
    bool bMatches=ExpectedAfter.Num()==After.Num();
    for (const auto& Pair:ExpectedAfter)
    { const FString* Value=After.Find(Pair.Key); bMatches &= Value && *Value==Pair.Value; }
    if (!bMatches)
    { Error=TEXT("Unexpected handle, input, or renderer change; duplicate must not be saved."); return false; }
    System->MarkPackageDirty(); System->RequestCompile(true);
    return true;
}

TMap<FString,FString> USovExplosionReadabilityAuthoringLibrary::InspectExplosionBindingDefaults(UBlueprint* Blueprint)
{
    TMap<FString,FString> Result;
    if (!IsInGameThread() || !IsValid(Blueprint) || !Blueprint->GeneratedClass) { return Result; }
    const FString Package=Blueprint->GetOutermost()->GetName();
    if (Package!=TEXT("/Game/SciFi_Drone_1/Textures/BP_DroneRocketProjectile")
        && Package!=TEXT("/Game/SciFi_Drone_1/Textures/BP_DroneGunshotPresentation")) { return Result; }
    UObject* Default=Blueprint->GeneratedClass->GetDefaultObject();
    if (IsValid(Default)) { SovExplosionAuthoring::Properties(Default,TEXT(""),Result); }
    return Result;
}

bool USovExplosionReadabilityAuthoringLibrary::CompileExplosionBindingBlueprint(UBlueprint* Blueprint,FString& Error)
{
    Error.Reset();
    if (!IsInGameThread() || (GEditor && GEditor->PlayWorld) || InspectExplosionBindingDefaults(Blueprint).IsEmpty())
    { Error=TEXT("Requires exact rocket or gunshot Blueprint outside PIE."); return false; }
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint,EBlueprintCompileOptions::SkipSave,&Results);
    if (Results.NumErrors || !Blueprint->GeneratedClass || !Blueprint->IsUpToDate())
    { Error=FString::Printf(TEXT("Blueprint compile failed: %d errors, %d warnings."),Results.NumErrors,Results.NumWarnings); return false; }
    return true;
}
