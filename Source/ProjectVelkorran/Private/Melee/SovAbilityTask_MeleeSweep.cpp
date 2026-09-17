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
    auto* Task=NewAbilityTask<USovAbilityTask_MeleeSweep>(Owner); Task->Mesh=TraceMesh; Task->Definition=Node; Task->Segments=Node.TraceSegments(); return Task;
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
bool USovAbilityTask_MeleeSweep::ResolveEdges(const FTransform& ComponentTransform,TArray<FEdge>& OutEdges) const
{
    OutEdges.Reset();
    if (!Mesh.IsValid()||Segments.IsEmpty()||ComponentTransform.ContainsNaN()) { return false; }
    for (const FSovMeleeTraceSegment& Segment:Segments)
    {
        FEdge& Edge=OutEdges.AddDefaulted_GetRef();
        if (!Segment.ResolveComponentSpace(*Mesh,Edge.Start,Edge.End)
            || !SovMelee::SpatialSamples(FVector::Distance(ComponentTransform.TransformPosition(Edge.Start),ComponentTransform.TransformPosition(Edge.End)),Definition.TraceRadius))
        { return false; }
    }
    return true;
}
void USovAbilityTask_MeleeSweep::Activate()
{
    Source=Ability?Ability->GetAvatarActorFromActorInfo():nullptr;
    const auto* Combat=Cast<UNarrativeCombatAbility>(Ability);
    if (!Source.IsValid()||!Source->HasAuthority()||!Mesh.IsValid()||!Combat||!Combat->GetSovAttackIdentity(Source.Get(),AttackId)
        ||!ContextValid()||!SovMelee::ValidWindows(Definition.Startup,Definition.Active,Definition.Recovery,Definition.BranchOpen,Definition.BranchClose))
    { StopInvalid(); return; }
    StartedAt=GetWorld()->GetTimeSeconds();
    PreviousTransform=Mesh->GetComponentTransform();
    if (!ResolveEdges(PreviousTransform,PreviousEdges)) { StopInvalid(); }
}
void USovAbilityTask_MeleeSweep::TickTask(float DeltaTime)
{
    Super::TickTask(DeltaTime);
    if (bStopped) { return; }
    if (!ContextValid()||!FMath::IsFinite(DeltaTime)||DeltaTime<0.f)
    { StopInvalid(); return; }
    const FTransform Current=Mesh->GetComponentTransform();
    TArray<FEdge> CurrentEdges;
    if (!ResolveEdges(Current,CurrentEdges)||CurrentEdges.Num()!=PreviousEdges.Num()
        || FVector::DistSquared(Current.GetLocation(),PreviousTransform.GetLocation())>FMath::Square(1500.f))
    { StopInvalid(); return; }
    const float PreviousTime=Elapsed; Elapsed=FMath::Max(GetWorld()->GetTimeSeconds()-StartedAt,Elapsed);
    DeltaTime=Elapsed-PreviousTime;
    const float ActiveEnd=Definition.Startup+Definition.Active;
    if (DeltaTime>SMALL_NUMBER && Elapsed>=Definition.Startup && PreviousTime<ActiveEnd)
    {
        const float From=FMath::Clamp((Definition.Startup-PreviousTime)/DeltaTime,0.f,1.f);
        const float To=FMath::Clamp((ActiveEnd-PreviousTime)/DeltaTime,0.f,1.f);
        Sweep(From,To,Current,CurrentEdges);
    }
    PreviousTransform=Current; PreviousEdges=MoveTemp(CurrentEdges);
    if (!ContextValid()||!ShouldBroadcastAbilityTaskDelegates()) { return; }
    OnStep.Broadcast(Elapsed);
    if (!ContextValid()||!ShouldBroadcastAbilityTaskDelegates()) { return; }
    if (Elapsed>=ActiveEnd+Definition.Recovery) { bStopped=true; OnFinished.Broadcast(); EndTask(); }
}
void USovAbilityTask_MeleeSweep::Sweep(float From,float To,const FTransform& Current,const TArray<FEdge>& CurrentEdges)
{
    if (To<From || !GetWorld() || CurrentEdges.Num()!=PreviousEdges.Num()) { return; }
    const float Radius=Definition.TraceRadius;
    const float Angle=FMath::RadiansToDegrees(PreviousTransform.GetRotation().AngularDistance(Current.GetRotation()));
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SovMeleeSweep),false,Source.Get()); Params.bReturnPhysicalMaterial=true;
    SovSelenePayload::IgnoreSource(Params,Source.Get());
    const ECollisionChannel Channel=UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel;
    // Every edge of a node shares one hit ledger: a double blade striking a target with both ends is one hit.
    for (int32 EdgeIndex=0;EdgeIndex<CurrentEdges.Num()&&!bStopped;++EdgeIndex)
    {
        const FEdge& Previous=PreviousEdges[EdgeIndex]; const FEdge& Now=CurrentEdges[EdgeIndex];
        const auto At=[&](float Alpha,float BladeAlpha)
        {
            FTransform Frame; Frame.Blend(PreviousTransform,Current,Alpha);
            return Frame.TransformPosition(FMath::Lerp(FMath::Lerp(Previous.Start,Now.Start,Alpha),FMath::Lerp(Previous.End,Now.End,Alpha),BladeAlpha));
        };
        const float Length=FMath::Max(FVector::Distance(At(From,0),At(From,1)),FVector::Distance(At(To,0),At(To,1)));
        const int32 Samples=SovMelee::SpatialSamples(Length,Radius); if (!Samples) { StopInvalid(); return; }
        const float Travel=FMath::Max(FVector::Distance(At(From,0),At(To,0)),FVector::Distance(At(From,1),At(To,1)));
        const int32 Temporal=SovMelee::TemporalSamples(Angle*(To-From),Travel,Radius);
        if (!Temporal) { StopInvalid(); return; }
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
}
void USovAbilityTask_MeleeSweep::OnDestroy(bool bAbilityEnded)
{ bStopped=true; HitActors.Reset(); EnvironmentContacts.Reset(); Super::OnDestroy(bAbilityEnded); }
