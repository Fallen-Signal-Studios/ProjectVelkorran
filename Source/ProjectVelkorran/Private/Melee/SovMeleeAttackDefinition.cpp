// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Melee/SovMeleeAttackDefinition.h"
#include "Melee/SovMeleePolicy.h"
#include "Exertion/SovGameplayAbility_Exertion.h"
#include "Sovereign/SovGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
namespace
{
    constexpr double MaximumSocketOffset=300.;
    constexpr int32 MaximumAdditionalSegments=3;
    bool OffsetValid(const FVector& Offset) { return !Offset.ContainsNaN()&&Offset.Size()<=MaximumSocketOffset; }
}
bool FSovMeleeTraceSegment::ResolveComponentSpace(const USkeletalMeshComponent& Mesh,FVector& OutStart,FVector& OutEnd) const
{
    if (StartSocket.IsNone()||EndSocket.IsNone()||!Mesh.DoesSocketExist(StartSocket)||!Mesh.DoesSocketExist(EndSocket)) { return false; }
    OutStart=Mesh.GetSocketTransform(StartSocket,RTS_Component).TransformPosition(StartOffset);
    OutEnd=Mesh.GetSocketTransform(EndSocket,RTS_Component).TransformPosition(EndOffset);
    return !OutStart.ContainsNaN()&&!OutEnd.ContainsNaN();
}
TArray<FSovMeleeTraceSegment> FSovMeleeAttackNode::TraceSegments() const
{
    TArray<FSovMeleeTraceSegment> Segments;
    Segments.Reserve(1+AdditionalSegments.Num());
    FSovMeleeTraceSegment& Primary=Segments.AddDefaulted_GetRef();
    Primary.StartSocket=StartSocket; Primary.EndSocket=EndSocket; Primary.StartOffset=StartOffset; Primary.EndOffset=EndOffset;
    Segments.Append(AdditionalSegments);
    return Segments;
}
bool USovMeleeAttackDefinition::Validate(FString& Error) const
{
    Error.Reset();
    if (Nodes.IsEmpty()||Nodes.Num()>8) { Error=TEXT("A melee definition requires one to eight finite nodes."); return false; }
    const auto& T=FSovGameplayTags::Get();
    FGameplayTagContainer Channels;
    for (const FGameplayTag Tag : {T.Damage_Channel_Kinetic,T.Damage_Channel_Edge,T.Damage_Channel_Thermal,T.Damage_Channel_Echo,
        T.Damage_Channel_Disruption,T.Damage_Channel_Corruption,T.Damage_Channel_Environmental}) { Channels.AddTag(Tag); }
    FGameplayTagContainer Classes;
    for (const FGameplayTag Tag : {T.Damage_Heavy,T.Damage_Unblockable,T.Damage_GuardClass_Standard,T.Damage_GuardClass_Heavy,
        T.Damage_GuardClass_Unblockable}) { Classes.AddTag(Tag); }
    for (int32 I=0;I<Nodes.Num();++I)
    {
        const auto& N=Nodes[I];
        if (!SovMelee::ValidWindows(N.Startup,N.Active,N.Recovery,N.BranchOpen,N.BranchClose)
            || N.StartSocket.IsNone()||N.EndSocket.IsNone()||!SovMelee::SpatialSamples(0,N.TraceRadius)
            || N.AdditionalSegments.Num()>MaximumAdditionalSegments
            || N.TraceSegments().ContainsByPredicate([](const FSovMeleeTraceSegment& Segment)
                { return Segment.StartSocket.IsNone()||Segment.EndSocket.IsNone()||!OffsetValid(Segment.StartOffset)||!OffsetValid(Segment.EndOffset); })
            || !FMath::IsFinite(N.Damage)||N.Damage<0.f||!FMath::IsFinite(N.PoiseDamage)||N.PoiseDamage<0.f
            || !FMath::IsFinite(N.ShieldCoefficient)||N.ShieldCoefficient<0.f||!FMath::IsFinite(N.HealthCoefficient)||N.HealthCoefficient<0.f
            || !FMath::IsFinite(N.HitConfirmAdvance)||N.HitConfirmAdvance<0.f||N.HitConfirmAdvance>.1f
            || !FMath::IsFinite(N.DefensiveCancelCost)||N.DefensiveCancelCost<0.f
            || !FMath::IsFinite(N.MaximumAimCorrection)||N.MaximumAimCorrection<0.f||N.MaximumAimCorrection>25.f
            || !Channels.HasAllExact(N.DamageChannels)||!Classes.HasAllExact(N.AttackClassifications)
            || (N.DefensiveInput.IsValid() && N.DefensiveInput==N.FollowUpInput && N.NextNode!=INDEX_NONE)
            || (N.NextNode!=INDEX_NONE && (!SovMelee::ValidFollowUp(I,N.NextNode,Nodes.Num())||!N.FollowUpInput.IsValid()))
            || (N.DefensiveInput.IsValid()!=bool(N.DefensiveAbility))
            || (N.DefensiveAbility && !N.DefensiveAbility->IsChildOf(USovGameplayAbility_Evade::StaticClass()))
            || (N.bCharged && (!FMath::IsFinite(N.FullChargeSeconds)||N.FullChargeSeconds<.1f||N.FullChargeSeconds>2.f
                || !FMath::IsFinite(N.MaximumChargeSeconds)||N.MaximumChargeSeconds<N.FullChargeSeconds||N.MaximumChargeSeconds>3.f
                || !FMath::IsFinite(N.ChargedMultiplier)||N.ChargedMultiplier<1.f||N.ChargedMultiplier>4.f)))
        { Error=FString::Printf(TEXT("Melee node %d has invalid geometry, windows, cost or an unbounded branch."),I); return false; }
    }
    return true;
}
