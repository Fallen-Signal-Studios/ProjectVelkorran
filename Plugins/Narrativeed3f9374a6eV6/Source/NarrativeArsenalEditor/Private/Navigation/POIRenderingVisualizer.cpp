// Copyright Narrative Tools 2025.


#include "Navigation/POIRenderingVisualizer.h"

#include "CanvasTypes.h"
#include "SEditorViewport.h"
#include "Editor/UnrealEdEngine.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/POIActor.h"
#include "Navigation/POIRenderingComponent.h"

FPOILink::FPOILink(APOIActor* InPOIActor, APOIActor* InOtherPOIActor)
{
	POIActor = InPOIActor;
	OtherPOIActor = InOtherPOIActor;
}

IMPLEMENT_HIT_PROXY(HPOILinkVisProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HPOIVisProxy, HComponentVisProxy);

const float POIVisRadius = 1000.f;

void FPOIRenderingVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View,
                                               FPrimitiveDrawInterface* PDI)
{
	const UPOIRenderingComponent* SpawnComp = Cast<UPOIRenderingComponent>(Component);
	if (SpawnComp == nullptr)
	{
		return;
	}

	APOIActor* POIActor = SpawnComp->GetOwner<APOIActor>();

	if(POIActor == nullptr)
	{
		return;
	}
	
	SelectedPOI = POIActor;
	if (Component != GetEditedComponent()) { return; } // Can only display visualization for the currently selected POI

	// Draw other POIs that can be connected
	TArray<AActor*> POIActors;
	UGameplayStatics::GetAllActorsOfClass(POIActor, APOIActor::StaticClass(), POIActors);
	
	// green = our own POI
	// yellow = other POI
	for (AActor* OtherActor : POIActors)
	{
		auto OtherPOIActor = Cast<APOIActor>(OtherActor);
		if (!OtherPOIActor) { continue; }
		
		const FColor Color = OtherPOIActor == POIActor ? FColor::Green : FColor::Yellow;
		
		const FTransform Transform = OtherPOIActor->GetTransform();

		// Cant click our own POI
		if (OtherPOIActor != POIActor)
		{
			PDI->SetHitProxy(new HPOIVisProxy(SpawnComp, OtherPOIActor));
		}
		
		DrawCircle(PDI, Transform.GetLocation(), Transform.GetRotation().GetAxisX(), Transform.GetRotation().GetAxisY(), Color, POIVisRadius, 8, SDPG_Foreground, 1.5f, 0, true);
		PDI->SetHitProxy(NULL);

		// Draw links
		auto LinkPOI = [&](APOIActor* POI, APOIActor* OtherPOI, float Offset)
        {
        	FVector ToTarget = OtherPOI->GetActorLocation() - POI->GetActorLocation();
        	FVector Dir = ToTarget.GetSafeNormal();
        	FVector DirRight = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
        	PDI->SetHitProxy(new HPOILinkVisProxy(Component, FPOILink(POI, OtherPOI)));
        	DrawDashedLine(PDI, POI->GetActorLocation() + Dir * POIVisRadius + DirRight * Offset, OtherPOI->GetActorLocation() - Dir * POIVisRadius + DirRight * Offset, FColor::Green, 40.f, SDPG_Foreground);

        	FVector MidPoint = POI->GetActorLocation() + ToTarget/2;
        	const float ArrowSize = 600.f;
        
        	FPrimitiveSceneProxy::DrawArrowHead(PDI, MidPoint + DirRight * Offset, MidPoint - Dir * ArrowSize + DirRight * Offset, ArrowSize, FColor::Green, SDPG_Foreground, 2, true);
        
        	PDI->SetHitProxy(NULL);
        };
		
        bool bPOIToOther = POIActor->LinkedPOIs.Contains(OtherPOIActor);
        bool bOtherToPOI = OtherPOIActor->LinkedPOIs.Contains(POIActor);
        float Offset = bPOIToOther && bOtherToPOI ? 200.f : 0.f;
        
        if (bPOIToOther)
        {
        	LinkPOI(POIActor, OtherPOIActor, Offset);
        }
        if (bOtherToPOI)
        {
        	LinkPOI(OtherPOIActor, POIActor, Offset);
        }
	}
}

void FPOIRenderingVisualizer::ConnectPOI()
{
	if (SelectedPOILink.IsValid())
	{
		const FScopedTransaction Transaction(FText::FromString("Link POI Actor"));
		SelectedPOILink.POIActor->Modify();
		SelectedPOILink.POIActor->LinkedPOIs.AddUnique(SelectedPOILink.OtherPOIActor.Get());
	}
}

void FPOIRenderingVisualizer::TwoWayConnectPOI()
{
	if (SelectedPOILink.IsValid())
	{
		const FScopedTransaction Transaction(FText::FromString("Two-way Link POI Actors"));
		SelectedPOILink.POIActor->Modify();
		SelectedPOILink.POIActor->LinkedPOIs.AddUnique(SelectedPOILink.OtherPOIActor.Get());
		
		SelectedPOILink.OtherPOIActor->Modify();
		SelectedPOILink.OtherPOIActor->LinkedPOIs.AddUnique(SelectedPOILink.POIActor.Get());
	}
}

void FPOIRenderingVisualizer::TwoWayDisconnectPOI()
{
	if (SelectedPOILink.IsValid())
	{
		const FScopedTransaction Transaction(FText::FromString("Two-way Unlink POI Actors"));
		SelectedPOILink.POIActor->Modify();
		SelectedPOILink.POIActor->LinkedPOIs.Remove(SelectedPOILink.OtherPOIActor.Get());
		
		SelectedPOILink.OtherPOIActor->Modify();
		SelectedPOILink.OtherPOIActor->LinkedPOIs.Remove(SelectedPOILink.POIActor.Get());
	}
}

void FPOIRenderingVisualizer::DisconnectPOI()
{
	if (SelectedPOILink.IsValid())
	{
		const FScopedTransaction Transaction(FText::FromString("Unlink POI Actor"));
		SelectedPOILink.POIActor->Modify();
		SelectedPOILink.POIActor->LinkedPOIs.Remove(SelectedPOILink.OtherPOIActor.Get());
	}
}

bool FPOIRenderingVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy,
                                                  const FViewportClick& Click)
{
	auto Component = const_cast<UActorComponent*>(VisProxy->Component.Get());
	if (Component == nullptr) { return false; }
	
	APOIActor* POIActor = Component->GetOwner<APOIActor>();
	if (POIActor == nullptr) { return false; }
	
	// show options on right click
	if (VisProxy && VisProxy->IsA(HPOIVisProxy::StaticGetType()))
	{
		auto SelectedPOIProxy = static_cast<HPOIVisProxy*>(VisProxy);
		if (Click.GetKey() == EKeys::RightMouseButton)
		{
			SelectedPOILink = FPOILink(POIActor, Cast<APOIActor>(SelectedPOIProxy->OtherPOI.Get()));
			
			FMenuBuilder MenuBuilder(true, nullptr);

			bool bOwnerPOIHasLink = SelectedPOILink.POIActor->LinkedPOIs.Contains(SelectedPOILink.OtherPOIActor.Get());
			bool bOtherPOIHasLink = SelectedPOILink.OtherPOIActor->LinkedPOIs.Contains(SelectedPOILink.POIActor.Get());
			if (!bOwnerPOIHasLink)
			{
				// Connect POI
				MenuBuilder.AddMenuEntry(
					FText::FromString("Link POI"),
					FText::FromString("Connect selected POI to the clicked POI"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateRaw(this, &FPOIRenderingVisualizer::ConnectPOI))
				);
			}
			if (!bOwnerPOIHasLink || !bOtherPOIHasLink)
			{
				// Two-way link
				MenuBuilder.AddMenuEntry(
					FText::FromString("Two-way Link POI"),
					FText::FromString("Create a bi-directional link between POIs"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateRaw(this, &FPOIRenderingVisualizer::TwoWayConnectPOI))
				);
			}
			
			if (bOwnerPOIHasLink)
			{
				// Disconnect POI
				MenuBuilder.AddMenuEntry(
					FText::FromString("Unlink POI"),
					FText::FromString("Unlink POI point"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateRaw(this, &FPOIRenderingVisualizer::DisconnectPOI))
				);

				if (bOtherPOIHasLink)
				{
					// Disconnect bi-directional POI
					MenuBuilder.AddMenuEntry(
						FText::FromString("Two-way Unlink POI"),
						FText::FromString("Unlink bi-directional POI link"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FPOIRenderingVisualizer::TwoWayDisconnectPOI))
					);
				}
			}

			// Convert click position to screen space
			FVector2D ScreenPosition = FSlateApplication::Get().GetCursorPos();

			// Show the context menu
			TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
			TSharedPtr<SEditorViewport> ViewportWidget = InViewportClient->GetEditorViewportWidget();
			FSlateApplication::Get().PushMenu(
				ViewportWidget.ToSharedRef(),
				FWidgetPath(),
				MenuWidget.ToSharedRef(),
				ScreenPosition,
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
			return true;
		}
	}

	// Break link if we click on the dashed line
	if (VisProxy && VisProxy->IsA(HPOILinkVisProxy::StaticGetType()))
	{
		if (Click.IsAltDown())
		{
			auto POILinkProxy = static_cast<HPOILinkVisProxy*>(VisProxy);
			const FScopedTransaction Transaction(FText::FromString("Link POI Actors"));
			POILinkProxy->POILink.POIActor->Modify();
			POILinkProxy->POILink.POIActor->LinkedPOIs.Remove(POILinkProxy->POILink.OtherPOIActor.Get());
			return true;
		}
	}
	
	
	return false;
}

void FPOIRenderingVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport,
	const FSceneView* View, FCanvas* Canvas)
{
		if (Component && Component == GetEditedComponent())
		{
			const FIntRect CanvasRect = Canvas->GetViewRect();

			static const FText LinkPOIText = FText::FromString("Right click another POI to link/unlink from the currently selected POI");
			static const FText UnlinkLineText = FText::FromString("Alt + Click a POI line to unlink the currently selected POI from the other POI");
			
			auto DisplayHelpText = [&](const FText& SnapHelpText, double YOffset)
			{
				int32 XL;
				int32 YL;
				StringSize(GEngine->GetLargeFont(), XL, YL, *SnapHelpText.ToString());
				const double DrawPositionX = FMath::FloorToDouble(CanvasRect.Min.X + (CanvasRect.Width() - XL) * 0.5);
				const double DrawPositionY = CanvasRect.Min.Y + 50.0 + YOffset;
				Canvas->DrawShadowedString(DrawPositionX, DrawPositionY, *SnapHelpText.ToString(), GEngine->GetLargeFont(), FLinearColor::Yellow);
			};

			DisplayHelpText(LinkPOIText, 0);
			DisplayHelpText(UnlinkLineText, 20);
		}
}

UActorComponent* FPOIRenderingVisualizer::GetEditedComponent() const
{
	return SelectedPOI.IsValid() ? SelectedPOI->GetComponentByClass<UPOIRenderingComponent>() : nullptr;
}
