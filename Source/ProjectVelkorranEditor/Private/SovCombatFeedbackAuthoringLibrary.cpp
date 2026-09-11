// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovCombatFeedbackAuthoringLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraRendererProperties.h"
#include "UObject/Package.h"

FSovFeedbackSystemReadback USovCombatFeedbackAuthoringLibrary::InspectFeedbackSystem(UNiagaraSystem* System)
{
    FSovFeedbackSystemReadback Result;
    if (!IsValid(System)) { return Result; }
    Result.AssetPath = System->GetPathName();
    Result.bValid = System->IsValid();
    Result.bReady = System->IsReadyToRun();
    Result.bCompiling = System->HasOutstandingCompilationRequests(true);
    TArray<FNiagaraVariable> Parameters;
    System->GetExposedParameters().GetParameters(Parameters);
    for (const FNiagaraVariable& Parameter : Parameters)
    {
        Result.UserParameterNames.Add(Parameter.GetName().ToString());
        Result.UserParameterTypes.Add(Parameter.GetType().GetName());
        if (Parameter.GetType() == FNiagaraTypeDefinition::GetFloatDef())
        { Result.FloatParameters.Add(Parameter.GetName().ToString(), System->GetExposedParameters().GetParameterValue<float>(Parameter)); }
        if (Parameter.GetType() == FNiagaraTypeDefinition::GetColorDef())
        { Result.ColorParameters.Add(Parameter.GetName().ToString(), System->GetExposedParameters().GetParameterValue<FLinearColor>(Parameter)); }
    }
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        FSovFeedbackEmitterReadback Emitter;
        Emitter.Name = Handle.GetName().ToString();
        Emitter.bEnabled = Handle.GetIsEnabled();
        const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
        Emitter.bOwnedBySystem = IsValid(Instance.Emitter) && Instance.Emitter->IsIn(System);
        if (const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
        {
            Emitter.bGPU = Data->SimTarget == ENiagaraSimTarget::GPUComputeSim;
            for (const UNiagaraRendererProperties* Renderer : Data->GetRenderers())
            {
                if (!IsValid(Renderer)) { continue; }
                Emitter.RendererClasses.Add(Renderer->GetClass()->GetPathName());
                Emitter.RendererObjects.Add(Renderer->GetPathName());
                Emitter.LightRendererCount += Renderer->IsA<UNiagaraLightRendererProperties>() ? 1 : 0;
            }
        }
        Result.Emitters.Add(Emitter);
    }
    return Result;
}

bool USovCombatFeedbackAuthoringLibrary::PrepareOwnedFeedbackSystem(UNiagaraSystem* System,
    const TArray<FString>& DisabledEmitterNames, const TArray<FString>& RequiredSignalEmitterNames,
    const TMap<FString, float>& FloatParameters, const TMap<FString, FLinearColor>& ColorParameters, FString& Error)
{
    Error.Reset();
    if (!IsValid(System) || !System->GetPathName().StartsWith(TEXT("/Game/Aurelion/VFX/NS_Aurelion_"))
        || System->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor) || RequiredSignalEmitterNames.IsEmpty())
    { Error = TEXT("Requires an owned editor system and explicitly reviewed primary signal emitters."); return false; }
    TSet<FString> Names;
    for (const auto& Handle : System->GetEmitterHandles())
    {
        const auto Instance = Handle.GetInstance();
        if (!IsValid(Instance.Emitter) || !Instance.Emitter->IsIn(System) || !Handle.GetEmitterData())
        { Error = TEXT("A handle is not a standard emitter owned by this exact duplicated system."); return false; }
        const FString Name = Handle.GetName().ToString();
        if (Names.Contains(Name)) { Error = TEXT("Duplicate emitter name."); return false; }
        Names.Add(Name);
    }
    for (const FString& Name : DisabledEmitterNames)
    { if (!Names.Contains(Name)) { Error = TEXT("Disabled emitter not present: ") + Name; return false; } }
    for (const FString& Name : RequiredSignalEmitterNames)
    {
        if (!Names.Contains(Name) || DisabledEmitterNames.Contains(Name))
        { Error = TEXT("Required primary signal missing or disabled: ") + Name; return false; }
        const FNiagaraEmitterHandle* Handle = System->GetEmitterHandles().FindByPredicate(
            [&Name](const auto& Entry) { return Entry.GetName().ToString() == Name; });
        bool HasVisual = false;
        for (const auto* Renderer : Handle->GetEmitterData()->GetRenderers())
        { HasVisual |= IsValid(Renderer) && Renderer->GetIsEnabled() && !Renderer->IsA<UNiagaraLightRendererProperties>(); }
        if (!Handle->GetIsEnabled() || !HasVisual) { Error = TEXT("Primary signal has no enabled visual renderer."); return false; }
    }
    auto& Store = System->GetExposedParameters();
    for (const auto& Entry : FloatParameters)
    {
        if (!Entry.Key.StartsWith(TEXT("User.")) || !FMath::IsFinite(Entry.Value)
            || Store.IndexOf(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(*Entry.Key))) == INDEX_NONE)
        { Error = TEXT("Exact exposed float does not exist: ") + Entry.Key; return false; }
    }
    for (const auto& Entry : ColorParameters)
    {
        if (!Entry.Key.StartsWith(TEXT("User.")) || !FMath::IsFinite(Entry.Value.R) || !FMath::IsFinite(Entry.Value.G)
            || !FMath::IsFinite(Entry.Value.B) || !FMath::IsFinite(Entry.Value.A)
            || Store.IndexOf(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), FName(*Entry.Key))) == INDEX_NONE)
        { Error = TEXT("Exact exposed linear color does not exist: ") + Entry.Key; return false; }
    }
    // Every name/type is source-export verified by the caller and checked against the actual duplicated parameter store.
    System->Modify();
    for (const auto& Entry : FloatParameters)
    { Store.SetParameterValue(Entry.Value, FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(*Entry.Key)), false); }
    for (const auto& Entry : ColorParameters)
    { Store.SetParameterValue(Entry.Value, FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), FName(*Entry.Key)), false); }
    for (auto& Handle : System->GetEmitterHandles())
    {
        auto Instance = Handle.GetInstance();
        const TArray<UNiagaraRendererProperties*> Renderers = Handle.GetEmitterData()->GetRenderers();
        Instance.Emitter->Modify();
        for (auto* Renderer : Renderers)
        {
            if (IsValid(Renderer) && Renderer->IsA<UNiagaraLightRendererProperties>())
            { Instance.Emitter->RemoveRenderer(Renderer, Instance.Version); }
        }
        if (DisabledEmitterNames.Contains(Handle.GetName().ToString())) { Handle.SetIsEnabled(false, *System, false); }
    }
    System->MarkPackageDirty();
    System->RequestCompile(true);
    return true;
}

bool USovCombatFeedbackAuthoringLibrary::FinishFeedbackCompilation(UNiagaraSystem* System)
{
    if (!IsValid(System)) { return false; }
    System->WaitForCompilationComplete(true, false);
    return System->IsValid() && System->IsReadyToRun() && !System->HasOutstandingCompilationRequests(true);
}
