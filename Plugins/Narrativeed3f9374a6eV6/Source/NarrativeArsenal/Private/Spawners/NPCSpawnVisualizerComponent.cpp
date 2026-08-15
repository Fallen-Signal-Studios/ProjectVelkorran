// Copyright Narrative Tools 2025.


#include "Spawners/NPCSpawnVisualizerComponent.h"
#include "SceneManagement.h"
#include "ShowFlags.h"
#include "PrimitiveSceneProxy.h"
#include "Spawners/NPCSpawnComponent.h"
#include "SceneView.h"

UNPCSpawnVisualizerComponent::UNPCSpawnVisualizerComponent()
{
	SetHiddenInGame(true);
}

#if UE_ENABLE_DEBUG_DRAWING

namespace NarrativeArsenal {
	namespace Editor {
		TCustomShowFlag<EShowFlagShippingValue::ForceDisabled> ShowNPCSpawns(
			TEXT("NPCSpawnPoints"),
			true /*DefaultEnabled*/,
			SFG_Developer,
			FText::FromString("NPC Spawn Points"));
	}
}

FPrimitiveSceneProxy* UNPCSpawnVisualizerComponent::CreateSceneProxy()
{
	class FNPCSpawnSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		FNPCSpawnSceneProxy(UNPCSpawnVisualizerComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent),
			Component(InComponent)
		{
			ViewFlagIndex = static_cast<uint32>(FEngineShowFlags::FindIndexByName(TEXT("NPCSpawnPoints")));
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_UNPCSpawnVisualizerComponent_GetDynamicMeshElements);

			if (IsSelected())
			{
				return;
			}

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];
					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

					const FMatrix& LocalToWorld = GetLocalToWorld();

					// Taking into account the min and maximum drawing distance
					const float DistanceSqr = (View->ViewMatrices.GetViewOrigin() - LocalToWorld.GetOrigin()).SizeSquared();
					if (DistanceSqr < FMath::Square(GetMinDrawDistance()) || DistanceSqr > FMath::Square(GetMaxDrawDistance()))
					{
						continue;
					}

					TArray<UNPCSpawnComponent*> Components;
					Component->GetOwner()->GetComponents<TArray<UNPCSpawnComponent*>::AllocatorType, UNPCSpawnComponent>(UNPCSpawnComponent::StaticClass(), Components);

					for (UNPCSpawnComponent* NPCSpawnComponent : Components)
					{
						FTransform Transform = NPCSpawnComponent->GetComponentTransform();
						DrawWireCapsule(PDI, Transform.GetLocation(), Transform.GetRotation().GetAxisX(), Transform.GetRotation().GetAxisY(), Transform.GetRotation().GetAxisZ(), FColor::Silver, 40.f, 88.f, 20, 0, 1.f, 1.f, false);
					}
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance =  !IsSelected() && IsShown(View) && View->Family->EngineShowFlags.GetSingleFlag(ViewFlagIndex);
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = IsShadowCast(View);
			Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Result;
		}

		virtual uint32 GetMemoryFootprint(void) const override { return sizeof *this + GetAllocatedSize(); }
		uint32 GetAllocatedSize(void) const { return FPrimitiveSceneProxy::GetAllocatedSize(); }
		
	protected:
		uint32 ViewFlagIndex;
		TWeakObjectPtr<UNPCSpawnVisualizerComponent> Component;
	};
	
	return new FNPCSpawnSceneProxy(this);
}

FBoxSphereBounds UNPCSpawnVisualizerComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox NewBounds = FBox::BuildAABB(GetComponentLocation(), FVector(100));
	TArray<UNPCSpawnComponent*> Components;
	GetOwner()->GetComponents<TArray<UNPCSpawnComponent*>::AllocatorType, UNPCSpawnComponent>(UNPCSpawnComponent::StaticClass(), Components);
	for (UNPCSpawnComponent* Component : Components)
	{
		NewBounds += FBox::BuildAABB(Component->GetComponentLocation(), FVector(100));
	}
	return NewBounds;
}
#endif

