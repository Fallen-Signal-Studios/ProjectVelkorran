// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Melee/SovMeleeAttackDefinition.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
bool USovMeleeRuntimeTestMesh::DoesSocketExist(FName Name) const
{ return Name==TEXT("blade_root")||Name==TEXT("blade_tip"); }
FTransform USovMeleeRuntimeTestMesh::GetSocketTransform(FName Name,ERelativeTransformSpace Space) const
{
    FTransform Local(FVector(0,Name==TEXT("blade_root")?-80.f:80.f,0));
    return Space==RTS_Component||Space==RTS_ParentBoneSpace?Local:Local*GetComponentTransform();
}
USovMeleeRuntimeTestAbility::USovMeleeRuntimeTestAbility()
{
    bAllowUnarmed=true;
    AttackDefinition=CreateDefaultSubobject<USovMeleeAttackDefinition>(TEXT("TestFiniteMelee"));
    FSovMeleeAttackNode First; First.Damage=20.f; First.PoiseDamage=10.f;
    First.AttackClassifications.AddTag(FSovGameplayTags::Get().Damage_Heavy);
    First.FollowUpInput=FNarrativeGameplayTags::Get().Narrative_Input_Attack; First.NextNode=1;
    AttackDefinition->Nodes.Add(First); First.NextNode=INDEX_NONE; AttackDefinition->Nodes.Add(First);
}
USovMeleeRuntimeTestSegmentAbility::USovMeleeRuntimeTestSegmentAbility()
{
    // The test mesh's sockets sit at Y=-80 (root) and Y=+80 (tip). The primary edge covers only
    // Y=-80..-20 through an offset from the root; the added edge covers Y=+20..+80 from the tip.
    FSovMeleeAttackNode Node; Node.Damage=20.f; Node.PoiseDamage=10.f;
    Node.StartSocket=TEXT("blade_root"); Node.EndSocket=TEXT("blade_root"); Node.EndOffset=FVector(0,60,0);
    FSovMeleeTraceSegment Far; Far.StartSocket=TEXT("blade_tip"); Far.EndSocket=TEXT("blade_tip"); Far.StartOffset=FVector(0,-60,0);
    Node.AdditionalSegments.Add(Far);
    AttackDefinition->Nodes.Reset(); AttackDefinition->Nodes.Add(Node);
}
USkeletalMeshComponent* USovMeleeRuntimeTestAbility::ResolveMeleeTraceMesh_Implementation() const
{ AActor* Source=GetAvatarActorFromActorInfo(); return Source?Source->FindComponentByClass<USovMeleeRuntimeTestMesh>():nullptr; }

ETeamAttitude::Type ASovMeleeRuntimeTestPlayer::GetTeamAttitudeTowards(const AActor& Other) const
{
    const auto* TestCharacter = Cast<ASovAxiomRuntimeTestCharacter>(&Other);
    return &Other == this || (TestCharacter && TestCharacter->TestTeam == 0) ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}