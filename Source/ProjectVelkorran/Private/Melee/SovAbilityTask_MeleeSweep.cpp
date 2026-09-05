// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Melee/SovAbilityTask_MeleeSweep.h"
#include "Melee/SovMeleePolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Combat/SovSelenePayload.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeCombatAbility.h"
USovAbilityTask_MeleeSweep::USovAbilityTask_MeleeSweep() { bTickingTask=true; }
USovAbilityTask_MeleeSweep* USovAbilityTask_MeleeSweep::SweepMeleeSockets(UGameplayAbility* Owner,USkeletalMeshComponent* TraceMesh,const FSovMeleeAttackNode& Node)
{
    auto* Task=NewAbilityTask<USovAbilityTask_MeleeSweep>(Owner); Task->Mesh=TraceMesh; Task->Definition=Node; return Task;
}
void USovAbilityTask_MeleeSweep::StopInvalid()
{
    if (bStopped) { return; }
    bStopped=true;
    const auto* Combat=Cast<UNarrativeCombatAbility>(Ability); FGuid CurrentId;
    // An old task must never end a newer node or a reactivated instance through its delegates.
    const bool bOwnsAction=!AttackId.IsValid() || (Combat && Combat->GetSovAttackIdentity(Source.Get(),CurrentId) && CurrentId==AttackId);
    if (bOwnsAction && ShouldBroadcastAbilityTaskDelegates()) { OnInvalidated.Broadcast(); }
    EndTask();
}
bool USovAbilityTask_MeleeSweep::ContextValid() const
{
    const auto* Combat=Cast<UNarrativeCombatAbility>(Ability);
    FGuid CurrentId;
    return !bStopped && Source.IsValid() && !Source->IsActorBeingDestroyed() && Mesh.IsValid()
        && Combat && Combat->CanDispatchNativeAttack() && Combat->GetSovAttackIdentity(Source.Get(),CurrentId)
        && CurrentId==AttackId && SovSelenePayload::ResolveTarget(Mesh->GetOwner())==Source.Get();
}
void USovAbilityTask_MeleeSweep::Activate()
{
    Source=Ability?Ability->GetAvatarActorFromActorInfo():nullptr;
    const auto* Combat=Cast<UNarrativeCombatAbility>(Ability);
    if (!Source.IsValid()||!Source->HasAuthority()||!Mesh.IsValid()||!Mesh->DoesSocketExist(Definition.StartSocket)
        ||!Mesh->DoesSocketExist(Definition.EndSocket)||!Combat||!Combat->GetSovAttackIdentity(Source.Get(),AttackId)
        ||!ContextValid()||!SovMelee::ValidWindows(Definition.Startup,Definition.Active,Definition.Recovery,Definition.BranchOpen,Definition.BranchClose))
    { StopInvalid(); return; }
    StartedAt=GetWorld()->GetTimeSeconds();
    PreviousTransform=Mesh->GetComponentTransform();
    PreviousLocalStart=Mesh->GetSocketTransform(Definition.StartSocket,RTS_Component).GetLocation();
    PreviousLocalEnd=Mesh->GetSocketTransform(Definition.EndSocket,RTS_Component).GetLocation();
    if (PreviousTransform.ContainsNaN() || PreviousLocalStart.ContainsNaN() || PreviousLocalEnd.ContainsNaN()
        || !SovMelee::SpatialSamples(FVector::Distance(PreviousTransform.TransformPosition(PreviousLocalStart),PreviousTransform.TransformPosition(PreviousLocalEnd)),Definition.TraceRadius)) { StopInvalid(); }
}
void USovAbilityTask_MeleeSweep::TickTask(float DeltaTime)
{
    Super::TickTask(DeltaTime);
    if (bStopped) { return; }
    if (!ContextValid()||!FMath::IsFinite(DeltaTime)||DeltaTime<0.f)
    { StopInvalid(); return; }
    const FTransform Current=Mesh->GetComponentTransform();
    const FVector LocalStart=Mesh->GetSocketTransform(Definition.StartSocket,RTS_Component).GetLocation();
    const FVector LocalEnd=Mesh->GetSocketTransform(Definition.EndSocket,RTS_Component).GetLocation();
    if (Current.ContainsNaN()||LocalStart.ContainsNaN()||LocalEnd.ContainsNaN()
        || FVector::DistSquared(Current.GetLocation(),PreviousTransform.GetLocation())>FMath::Square(1500.f)
        || !SovMelee::SpatialSamples(FVector::Distance(Current.TransformPosition(LocalStart),Current.TransformPosition(LocalEnd)),Definition.TraceRadius))
    { StopInvalid(); return; }
    const float PreviousTime=Elapsed; Elapsed=FMath::Max(GetWorld()->GetTimeSeconds()-StartedAt,Elapsed);
    DeltaTime=Elapsed-PreviousTime;
    const float ActiveEnd=Definition.Startup+Definition.Active;
    if (DeltaTime>SMALL_NUMBER && Elapsed>=Definition.Startup && PreviousTime<ActiveEnd)
    {
        const float From=FMath::Clamp((Definition.Startup-PreviousTime)/DeltaTime,0.f,1.f);
        const float To=FMath::Clamp((ActiveEnd-PreviousTime)/DeltaTime,0.f,1.f);
        Sweep(From,To,Current,LocalStart,LocalEnd);
    }
    PreviousTransform=Current; PreviousLocalStart=LocalStart; PreviousLocalEnd=LocalEnd;
    if (!ContextValid()||!ShouldBroadcastAbilityTaskDelegates()) { return; }
    OnStep.Broadcast(Elapsed);
    if (!ContextValid()||!ShouldBroadcastAbilityTaskDelegates()) { return; }
    if (Elapsed>=ActiveEnd+Definition.Recovery) { bStopped=true; OnFinished.Broadcast(); EndTask(); }
}
void USovAbilityTask_MeleeSweep::Sweep(float From,float To,const FTransform& Current,const FVector& Start,const FVector& End)
{
    if (To<From || !GetWorld()) { return; }
    const float Radius=Definition.TraceRadius;
    const auto At=[&](float Alpha,float BladeAlpha)
    {
        FTransform Frame; Frame.Blend(PreviousTransform,Current,Alpha);
        return Frame.TransformPosition(FMath::Lerp(FMath::Lerp(PreviousLocalStart,Start,Alpha),FMath::Lerp(PreviousLocalEnd,End,Alpha),BladeAlpha));
    };
    const float Length=FMath::Max(FVector::Distance(At(From,0),At(From,1)),FVector::Distance(At(To,0),At(To,1)));
    const int32 Samples=SovMelee::SpatialSamples(Length,Radius); if (!Samples) { StopInvalid(); return; }
    const float Angle=FMath::RadiansToDegrees(PreviousTransform.GetRotation().AngularDistance(Current.GetRotation()));
    const float Travel=FMath::Max(FVector::Distance(At(From,0),At(To,0)),FVector::Distance(At(From,1),At(To,1)));
    const int32 Temporal=SovMelee::TemporalSamples(Angle*(To-From),Travel,Radius);
    if (!Temporal) { StopInvalid(); return; }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SovMeleeSweep),false,Source.Get()); Params.bReturnPhysicalMaterial=true;
    SovSelenePayload::IgnoreSource(Params,Source.Get());
    const ECollisionChannel Channel=UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel;
    for (int32 Step=0;Step<Temporal&&!bStopped;++Step)
    {
        const float A=FMath::Lerp(From,To,float(Step)/Temporal),B=FMath::Lerp(From,To,float(Step+1)/Temporal);
        for (int32 Sample=0;Sample<Samples&&!bStopped;++Sample)
        {
            const float Blade=Samples==1?0.f:float(Sample)/(Samples-1);
            TArray<FHitResult> Hits;
            GetWorld()->SweepMultiByChannel(Hits,At(A,Blade),At(B,Blade),FQuat::Identity,Channel,FCollisionShape::MakeSphere(Radius),Params);
            for (const FHitResult& Hit:Hits)
            {
                if (!ContextValid()||!ShouldBroadcastAbilityTaskDelegates()) { return; }
                AActor* Target=SovSelenePayload::ResolveTarget(Hit.GetActor());
                if (Target && Target!=Source.Get() && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
                {
                    if (HitActors.Contains(Target)) { continue; }
                    // A moving animation can put a blade sample beyond a wall. Verify the physical
                    // source-to-contact lane as well as the previous/current socket sweep.
                    FHitResult Cover;
                    FCollisionQueryParams CoverParams=Params;
                    CoverParams.AddIgnoredActor(Target);
                    TArray<AActor*> Attached; Target->GetAttachedActors(Attached,true,true); CoverParams.AddIgnoredActors(Attached);
                    if (GetWorld()->LineTraceSingleByChannel(Cover,Source->GetActorLocation(),Hit.ImpactPoint,Channel,CoverParams))
                    {
                        if (Cover.GetComponent() && !EnvironmentContacts.Contains(Cover.GetComponent())
                            && !UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SovSelenePayload::ResolveTarget(Cover.GetActor())))
                        { EnvironmentContacts.Add(Cover.GetComponent()); OnEnvironmentContact.Broadcast(Cover,nullptr); }
                        continue;
                    }
                    HitActors.Add(Target); OnContact.Broadcast(Hit,Target);
                }
                else if (Hit.GetComponent() && !EnvironmentContacts.Contains(Hit.GetComponent()))
                { EnvironmentContacts.Add(Hit.GetComponent()); OnEnvironmentContact.Broadcast(Hit,nullptr); }
            }
        }
    }
}
void USovAbilityTask_MeleeSweep::OnDestroy(bool bAbilityEnded)
{ bStopped=true; HitActors.Reset(); EnvironmentContacts.Reset(); Super::OnDestroy(bAbilityEnded); }
