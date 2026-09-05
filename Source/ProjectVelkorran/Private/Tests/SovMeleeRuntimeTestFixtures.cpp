// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMeleeRuntimeTestFixtures.h"
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
USkeletalMeshComponent* USovMeleeRuntimeTestAbility::ResolveMeleeTraceMesh_Implementation() const
{ AActor* Source=GetAvatarActorFromActorInfo(); return Source?Source->FindComponentByClass<USovMeleeRuntimeTestMesh>():nullptr; }
