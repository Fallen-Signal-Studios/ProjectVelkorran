// Copyright Narrative Tools 2025.


#include "Details/ItemDetailsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Items/NarrativeItem.h"

#if WITH_EDITOR
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "Factories/Texture2dFactoryNew.h"
#endif 


#define LOCTEXT_NAMESPACE "FItemDetailsCustomization"

TSharedRef<IDetailCustomization> FItemDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FItemDetailsCustomization);
}

void FItemDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num() == 0) return;

	Target = Cast<UNarrativeItem>(Objects[0].Get());
	if (!Target.IsValid()) return;

	// Create a custom category at the top
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Editor Tools", FText::GetEmpty(), ECategoryPriority::Variable);

	// Create custom row and button for executing CreateNPCItem function
	Category.AddCustomRow(LOCTEXT("CreateItemThumb", "Item Utilities"))
	.WholeRowWidget
	[
		SNew(SButton)
		.Text(LOCTEXT("CreateNPCItemButton", "Generate Item Thumbnail"))
		.HAlign(HAlign_Center)
		.OnClicked_Lambda([&]()
		{
			GenerateThumbnail();
			return FReply::Handled();
		})
	];
}

void FItemDetailsCustomization::GenerateThumbnail()
{


	UNarrativeItem* Item = Target.Get();
	UE_LOG(LogTemp, Warning, TEXT("GEN THUMB for %s"), *GetNameSafe(Item));
	
	if (Item)
	{
		UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!EditorWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateThumbnail: No editor world available."));
			return;
		}

		FString ClassName = Item->GetClass() ? Item->GetClass()->GetName() : GetNameSafe(Item);
		ClassName.RemoveFromEnd("_C");
		
		const FString NewThumbName = FString::Printf(TEXT("T_%s_Thumb"), *ClassName);
		const FString NewThumbPath = "/Game/Inventory/Thumbnails"; //TODO this wants a settings config 
		
		//Create the new texture object. 
		if (UTexture2D* NewTexture = Cast<UTexture2D>(IAssetTools::Get().CreateAsset(NewThumbName, NewThumbPath, UTexture2D::StaticClass(), NewObject<UTexture2DFactoryNew>(), FName("ItemCapture"))))
		{
			if (UStaticMesh* PickupMeshAsset = Item->GetPickupMeshData(1).PickupMesh.LoadSynchronous())
			{
				//FObjectThumbnail ObjectThumbnail;
				//ThumbnailTools::RenderThumbnail(PickupMeshAsset, 256, 256, ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush, nullptr, &ObjectThumbnail);

				const int32 ThumbSize = 512;

				// 1. Create a render target to capture into
				UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
				RenderTarget->InitCustomFormat(ThumbSize, ThumbSize, PF_B8G8R8A8, false);
				//RenderTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f); // transparent bg
				RenderTarget->TargetGamma = 2.2f;
				RenderTarget->UpdateResourceImmediate(true);

				// 2. Spawn a temporary actor to hold the mesh + capture component
				FActorSpawnParameters SpawnParams;
				SpawnParams.ObjectFlags |= RF_Transient;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				AActor* RigActor = EditorWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
				if (!RigActor)
				{
					UE_LOG(LogTemp, Warning, TEXT("GenerateThumbnail: Failed to spawn capture rig actor."));
					return;
				}
				RigActor->SetFlags(RF_Transient);

				UStaticMeshComponent* PreviewMeshComp = NewObject<UStaticMeshComponent>(RigActor); 
				PreviewMeshComp->RegisterComponent();
				PreviewMeshComp->SetStaticMesh(PickupMeshAsset);
				PreviewMeshComp->SetWorldTransform(FTransform::Identity);

				int32 Idx = 0;
				for (auto& MeshMat : Item->GetPickupMeshData(1).PickupMeshMaterials)
				{
					if (UMaterialInterface* Mat = MeshMat.LoadSynchronous())
					{
						PreviewMeshComp->SetMaterial(Idx, Mat);
					}
					++Idx;
				}
				
				RigActor->SetRootComponent(PreviewMeshComp);

				USceneCaptureComponent2D* CaptureComp = NewObject<USceneCaptureComponent2D>(RigActor);
				CaptureComp->RegisterComponent();
				CaptureComp->AttachToComponent(PreviewMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
				CaptureComp->TextureTarget = RenderTarget;
				CaptureComp->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
				CaptureComp->ProjectionType = ECameraProjectionMode::Perspective;
				CaptureComp->FOVAngle = 30.f;
				CaptureComp->bCaptureEveryFrame = false;
				CaptureComp->bCaptureOnMovement = false;
				CaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
				CaptureComp->ShowOnlyComponent(PreviewMeshComp);
				CaptureComp->ShowFlags.SetTemporalAA(false);

				// After creating PreviewMeshComp and CaptureComp...

				// Add dedicated lights to the rig so lighting is always identical regardless of spawn location
				UDirectionalLightComponent* KeyLight = NewObject<UDirectionalLightComponent>(RigActor);
				KeyLight->RegisterComponent();
				KeyLight->SetWorldRotation(FRotator(-40.f, -45.f, 0.f)); // classic 3/4 key light angle
				KeyLight->Intensity = 3.f;
				KeyLight->LightColor = FColor::White;

				UDirectionalLightComponent* FillLight = NewObject<UDirectionalLightComponent>(RigActor);
				FillLight->RegisterComponent();
				FillLight->SetWorldRotation(FRotator(-20.f, 135.f, 0.f)); // opposite side, softer
				FillLight->Intensity = 1.f;
				FillLight->LightColor = FColor::White;

				// Restrict the capture to ONLY render the rig's own actor (mesh) - ignores all level
				// geometry, lighting, sky, post-process volumes etc, so results are 100% deterministic
				CaptureComp->ShowOnlyActors.Add(RigActor);
				CaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

				// Since ShowOnlyActors excludes the level's skylight/atmosphere contribution too,
				// you may want a small amount of unlit ambient fill so shadow-side isn't pure black:
				CaptureComp->PostProcessSettings.bOverride_AutoExposureMethod = true;
				CaptureComp->PostProcessSettings.AutoExposureMethod = AEM_Manual;
				CaptureComp->PostProcessSettings.bOverride_AutoExposureBias = true;
				CaptureComp->PostProcessSettings.AutoExposureBias = 0.f; // tune this - was likely too dark before
				
				const FBoxSphereBounds Bounds = PickupMeshAsset->GetBounds();
				const FVector Origin = Bounds.Origin;
				const float Radius = FMath::Max(Bounds.SphereRadius, 10.f);

				// Pick a clear 3/4 viewing angle
				const FVector ViewDir = FVector(-1.f, -1.f, 0.5f).GetSafeNormal();

				// Compute distance needed to fit the full bounding sphere in frame, with padding
				const float HalfFOVRadians = FMath::DegreesToRadians(CaptureComp->FOVAngle * 0.5f);
				const float Distance = (Radius / FMath::Sin(HalfFOVRadians)) * 1.f; // 1.5x padding so it's not edge-to-edge

				const FVector CameraLocation = Origin - (ViewDir * Distance);
				const FRotator CameraRotation = (Origin - CameraLocation).Rotation();

				CaptureComp->SetRelativeLocationAndRotation(CameraLocation, CameraRotation);
				
				// 3. Frame the camera based on the mesh bounds so it isn't cropped/tiny
				//const FVector CameraLocation = FVector(-200.f, 0.f, 0.f);
				//const FRotator CameraRotation = FRotator::ZeroRotator; //(Origin - CameraLocation).Rotation();

				//CaptureComp->SetRelativeLocationAndRotation(CameraLocation, CameraRotation);

				// 4. Fire the capture
				CaptureComp->CaptureScene();

				// 5. Read pixels back from the render target
				TArray<FColor> Pixels;
				FRenderTarget* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
				if (!RTResource || !RTResource->ReadPixels(Pixels))
				{
					UE_LOG(LogTemp, Warning, TEXT("GenerateThumbnail: Failed to read pixels from render target."));
					RigActor->Destroy();
					return;
				}

				// SCS_SceneColorHDR gives inverse opacity in alpha - flip it to normal alpha convention
				for (FColor& Pixel : Pixels)
				{
					Pixel.A = 255 - Pixel.A;
				}
				NewTexture->Source.Init(ThumbSize, ThumbSize, 1, 1, TSF_BGRA8, reinterpret_cast<uint8*>(Pixels.GetData()));
				NewTexture->SRGB = true;
				NewTexture->MipGenSettings = TMGS_NoMipmaps;
				NewTexture->UpdateResource();
				NewTexture->PostEditChange();
				NewTexture->MarkPackageDirty();

				// Assign to this item
				Item->Thumbnail = TSoftObjectPtr<UTexture2D>(NewTexture);
				Item->MarkPackageDirty();

				if (RigActor)
				{
					RigActor->Destroy();
				}
			}
		}
	}

}


#undef LOCTEXT_NAMESPACE
