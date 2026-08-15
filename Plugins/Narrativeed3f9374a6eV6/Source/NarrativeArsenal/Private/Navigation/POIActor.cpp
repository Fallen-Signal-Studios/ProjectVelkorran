// Copyright Narrative Tools 2025.


#include "Navigation/POIActor.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include <GameFramework/Controller.h>

#include "GameFramework/Pawn.h"
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Navigation/POIRenderingComponent.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "SubobjectDataSubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPOIActor, Log, All)

APOIActor::APOIActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

	auto POIIconFinder = ConstructorHelpers::FObjectFinder<UTexture2D>(TEXT("/Script/Engine.Texture2D'/NarrativePro/Pro/Core/UI/Textures/Icons/T_Marker_Location.T_Marker_Location'"));

#if WITH_EDITOR

		if (POIIconFinder.Succeeded() && GetSpriteComponent())
		{
			GetSpriteComponent()->SetSprite(POIIconFinder.Object);
			GetSpriteComponent()->SetRelativeScale3D_Direct(FVector(1.2f));
		}

		if (GetArrowComponent())
		{
			GetArrowComponent()->SetArrowSize(0.f);
		}

#endif 

#if WITH_EDITOR
	SetIsSpatiallyLoaded(true);
#endif

#if WITH_EDITORONLY_DATA
	RenderingComponent = CreateEditorOnlyDefaultSubobject<UPOIRenderingComponent>(TEXT("RenderComp"));
#endif
	if (POIIconFinder.Succeeded())
	{
		POIIcon = POIIconFinder.Object;
	}

	FastTravelCapsule = CreateDefaultSubobject<UCapsuleComponent>("FastTravelLocation");
	FastTravelCapsule->InitCapsuleSize(34.0f, 88.0f);
	FastTravelCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FastTravelCapsule->SetupAttachment(GetRootComponent());

	FastTravelCapsuleArrow = CreateDefaultSubobject<UArrowComponent>("FastTravelArrow");

	FastTravelCapsuleArrow->ArrowColor = FColor(150, 200, 255);
	FastTravelCapsuleArrow->ArrowSize = 1.f;
	FastTravelCapsuleArrow->bTreatAsASprite = true;
	FastTravelCapsuleArrow->SetupAttachment(FastTravelCapsule);
	FastTravelCapsuleArrow->bIsScreenSizeScaled = true;

	FastTravelCapsuleArrow->SetupAttachment(FastTravelCapsule);

	bCreateMapMarker = true;
	bSupportsFastTravel = true; 

}

void APOIActor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (APawn* PlayerChar = Cast<APawn>(OtherActor))
	{
		if (POITag.IsValid())
		{
			if (AController* PC = PlayerChar->GetController())
			{
				if (UNarrativeNavigationComponent* NavComp = Cast<UNarrativeNavigationComponent>(PC->GetComponentByClass(UNarrativeNavigationComponent::StaticClass())))
				{
					NavComp->DiscoverPOI(POITag);
				}
			}
		}
	}
}
#if WITH_EDITOR
void APOIActor::CreatePOIVolume()
{
	auto SubobjectSubsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
	check(SubobjectSubsystem);

	TArray<FSubobjectDataHandle> SubobjectData;
	SubobjectSubsystem->GatherSubobjectData(this, SubobjectData);
	checkf(SubobjectData.Num() > 0, TEXT("Actor is missing subobject data! Does it have a valid root component?"))

	const FScopedTransaction Transaction(FText::FromString(TEXT("Add Sphere Component")));
	Modify();
	auto SphereComp = NewObject<USphereComponent>(this, "Sphere", RF_Transactional);

	if (SphereComp)
	{
		SphereComp->SetSphereRadius(1000.f);
		SphereComp->SetupAttachment(GetRootComponent());
		SphereComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		SphereComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

		SphereComp->RegisterComponent();
		AddInstanceComponent(SphereComp);
		RerunConstructionScripts();

		FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.BroadcastComponentsEdited();
	}
	else
	{
		UE_LOG(LogPOIActor, Error, TEXT("Unable to create USphereComponent"))
	}
}

#endif
