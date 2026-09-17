// Copyright Fallen Signal Studios. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "Items/WeaponItem.h"
#include "Melee/SovGameplayAbility_Melee.h"
#include "Melee/SovMeleeAttackDefinition.h"
#include "NarrativeGameplayTags.h"
#include "UObject/UnrealType.h"
#include "Weapons/WeaponVisual.h"

namespace SovProtagonistMeleeContent
{
    // The shipped kit, not a fixture: these are the weapon items Tarrik and Selene actually wield.
    struct FWeapon { const TCHAR* Hero; const TCHAR* ItemClass; };
    const FWeapon Weapons[] = {
        { TEXT("Tarrik"), TEXT("/Game/Items/Weapons/WI_Velkorran.WI_Velkorran_C") },
        { TEXT("Selene"), TEXT("/Game/Items/Weapons/WI_Verity.WI_Verity_C") },
    };

    template <typename T>
    const T* ReadProperty(const UObject* Object, FName Name)
    {
        for (UClass* Class = Object ? Object->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
        {
            if (const FProperty* Property = Class->FindPropertyByName(Name)) { return Property->ContainerPtrToValuePtr<T>(Object); }
        }
        return nullptr;
    }

    TArray<TSubclassOf<UNarrativeGameplayAbility>> Grants(const UWeaponItem* Item, FName Field)
    {
        const auto* Value = ReadProperty<TArray<TSubclassOf<UNarrativeGameplayAbility>>>(Item, Field);
        return Value ? *Value : TArray<TSubclassOf<UNarrativeGameplayAbility>>();
    }

    FGameplayTag InputOf(TSubclassOf<UNarrativeGameplayAbility> Ability)
    {
        const auto* Tag = Ability ? ReadProperty<FGameplayTag>(Ability->GetDefaultObject(), TEXT("InputTag")) : nullptr;
        return Tag ? *Tag : FGameplayTag();
    }

    const USovMeleeAttackDefinition* DefinitionOf(TSubclassOf<UNarrativeGameplayAbility> Ability)
    {
        const auto* Definition = Ability ? ReadProperty<TObjectPtr<USovMeleeAttackDefinition>>(Ability->GetDefaultObject(), TEXT("AttackDefinition")) : nullptr;
        return Definition ? Definition->Get() : nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtagonistWeaponsUseNativeMeleeTest,
    "ProjectVelkorran.Campaign.Melee.ProtagonistWeaponsUseNativeMelee", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtagonistWeaponsUseNativeMeleeTest::RunTest(const FString& Parameters)
{
    using namespace SovProtagonistMeleeContent;
    const auto& Tags = FNarrativeGameplayTags::Get();
    const FGameplayTag Heavy = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Input.Attack.Heavy"));

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient world for the weapon trace meshes"), World)) { return false; }
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

    for (const FWeapon& Weapon : Weapons)
    {
        const FString Hero(Weapon.Hero);
        UClass* ItemClass = LoadClass<UWeaponItem>(nullptr, Weapon.ItemClass);
        if (!TestNotNull(*(Hero + TEXT(" signature melee weapon item loads")), ItemClass)) { continue; }
        const UWeaponItem* Item = GetDefault<UWeaponItem>(ItemClass);

        // Narrative grants MainhandWeaponAbilities for this hand rule and WeaponAbilities otherwise; both must agree.
        TArray<TSubclassOf<UNarrativeGameplayAbility>> Native;
        for (FName Field : { FName(TEXT("WeaponAbilities")), FName(TEXT("MainhandWeaponAbilities")), FName(TEXT("EquipmentAbilities")) })
        {
            int32 Light = 0, HeavyCount = 0;
            for (TSubclassOf<UNarrativeGameplayAbility> Ability : Grants(Item, Field))
            {
                if (!Ability) { continue; }
                const bool bMelee = Ability->GetDefaultObject<UGameplayAbility>()->GetAssetTags().HasTag(Tags.Ability_MeleeAttack);
                const bool bNative = Ability->IsChildOf(USovGameplayAbility_Melee::StaticClass());
                TestFalse(*FString::Printf(TEXT("%s %s grants no Narrative Blueprint melee combo (%s)"), *Hero, *Field.ToString(), *Ability->GetName()), bMelee && !bNative);
                if (!bNative) { continue; }
                Native.AddUnique(Ability);
                const FGameplayTag Input = InputOf(Ability);
                Light += Input.MatchesTagExact(Tags.Narrative_Input_Attack) ? 1 : 0;
                HeavyCount += Input.MatchesTagExact(Heavy) ? 1 : 0;
            }
            if (Field != TEXT("EquipmentAbilities"))
            {
                TestEqual(*FString::Printf(TEXT("%s %s has exactly one native light attack"), *Hero, *Field.ToString()), Light, 1);
                TestEqual(*FString::Printf(TEXT("%s %s has exactly one native heavy attack"), *Hero, *Field.ToString()), HeavyCount, 1);
            }
        }

        // Every edge of every node must lie on the actual weapon blade in its reference pose.
        UClass* VisualClass = Item->GetWeaponVisualClass().LoadSynchronous();
        const AWeaponVisual* Visual = VisualClass ? GetDefault<AWeaponVisual>(VisualClass) : nullptr;
        USkeletalMesh* MeshAsset = Visual && Visual->WeaponMesh ? Visual->WeaponMesh->GetSkeletalMeshAsset() : nullptr;
        if (!TestNotNull(*(Hero + TEXT(" weapon visual carries its blade mesh")), MeshAsset)) { continue; }
        USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(World->GetWorldSettings());
        Mesh->SetSkeletalMeshAsset(MeshAsset); Mesh->RegisterComponentWithWorld(World);
        const float Reach = MeshAsset->GetBounds().SphereRadius + 5.f;
        const FVector Center = MeshAsset->GetBounds().Origin;
        for (TSubclassOf<UNarrativeGameplayAbility> Ability : Native)
        {
            const USovMeleeAttackDefinition* Definition = DefinitionOf(Ability);
            FString Error;
            if (!TestNotNull(*FString::Printf(TEXT("%s %s has an attack definition"), *Hero, *Ability->GetName()), Definition)) { continue; }
            TestTrue(*FString::Printf(TEXT("%s %s definition passes native validation: %s"), *Hero, *Ability->GetName(), *Error), Definition->Validate(Error));
            for (int32 Index = 0; Index < Definition->Nodes.Num(); ++Index)
            {
                const FSovMeleeAttackNode& Node = Definition->Nodes[Index];
                TestNotNull(*FString::Printf(TEXT("%s %s node %d plays a montage"), *Hero, *Ability->GetName(), Index), Node.Montage.Get());
                for (const FSovMeleeTraceSegment& Segment : Node.TraceSegments())
                {
                    FVector Start, End;
                    const bool bResolved = Segment.ResolveComponentSpace(*Mesh, Start, End);
                    TestTrue(*FString::Printf(TEXT("%s %s node %d edge resolves on the weapon mesh"), *Hero, *Ability->GetName(), Index), bResolved);
                    if (!bResolved) { continue; }
                    TestTrue(*FString::Printf(TEXT("%s %s node %d edge is blade length"), *Hero, *Ability->GetName(), Index),
                        FMath::IsWithinInclusive(FVector::Distance(Start, End), 50., 130.));
                    TestTrue(*FString::Printf(TEXT("%s %s node %d edge lies within the blade mesh"), *Hero, *Ability->GetName(), Index),
                        FVector::Distance(Start, Center) <= Reach && FVector::Distance(End, Center) <= Reach);
                }
            }
        }
        Mesh->UnregisterComponent();
    }
    return true;
}
#endif
