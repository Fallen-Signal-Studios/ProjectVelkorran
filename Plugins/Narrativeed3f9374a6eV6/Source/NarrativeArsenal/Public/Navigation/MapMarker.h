// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include <GameplayTagContainer.h>
#include "AI/Navigation/NavigationTypes.h"
#include "Blueprint/UserWidget.h"

#include "UObject/NoExportTypes.h"
#include "Engine/TimerHandle.h"
#include "MapMarker.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRefreshRequired);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelected, class UNarrativeNavigationComponent*, Selector);

USTRUCT(BlueprintType)
struct FZoneDrawData
{

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Points")
	TArray<FVector> Points;

};

USTRUCT(BlueprintType)
struct FNavigationMarkerSettings
{

	GENERATED_BODY()

	FNavigationMarkerSettings()
	{
		bOverride_LocationDisplayName = false;
		bOverride_LocationIcon = false;
		bOverride_IconTint = false;
		bOverride_IconSize = false;
		bOverride_IconOffset = false;
		bOverride_bShowActorRotation = false;

		MarkerTitleText = NSLOCTEXT("NavigationMarkerSettings", "NavigatorLocationDisplayName", "Location Marker");
		MarkerSubtitleText = FText::GetEmpty();
		LocationIcon = nullptr;
		IconTint = FLinearColor(1.f, 1.f, 1.f);
		IconSize = FVector2D(20.f, 20.f);
		IconOffset = FVector::ZeroVector;
		bShowActorRotation = false;
	};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_LocationDisplayName:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_LocationIcon : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_IconTint : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_IconSize : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_IconOffset : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_bShowActorRotation : 1;

	//The display name to use for this location 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup")
	FText MarkerTitleText;
	
	//The display name to use for this location 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup")
	FText MarkerSubtitleText;

	//The icon to use for this location 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (EditCondition="bOverride_LocationIcon"))
	TObjectPtr<class UTexture2D> LocationIcon;

	//Tint for the icon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (EditCondition = "bOverride_IconTint"))
	FLinearColor IconTint;

	//The icons size on the compass, map, screen marker, whereever the icon is being used 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (EditCondition = "bOverride_IconSize"))
	FVector2D IconSize;

	//Add an offset in the actors space - more efficient than making markers scene components with a transform 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (EditCondition = "bOverride_IconOffset"))
	FVector IconOffset;

	//Whether this markers rotation should be shown on the map, for any navigator widgets that use this feature
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (EditCondition = "bOverride_bShowActorRotation"))
	bool bShowActorRotation;
};

USTRUCT(BlueprintType)
struct FMarkerOnPaintData
{
	GENERATED_BODY()

	// paint space geometry of the root widget 
	UPROPERTY(BlueprintReadWrite, Category="PaintGeometry")
	FGeometry ParentGeometry;

	// paint space geometry of the map
	UPROPERTY(BlueprintReadWrite, Category="PaintGeometry")
	FGeometry MapGeometry;

	// paint space geometry of the marker icon widget 
	UPROPERTY(BlueprintReadWrite, Category="PaintGeometry")
	FGeometry IconGeometry;

	// map center location in world space 
	UPROPERTY(BlueprintReadWrite, Category="PaintGeometry")
	FVector2D MapOrigin;

	// current map pan in map space
	UPROPERTY(BlueprintReadWrite, Category="PaintGeometry")
	FVector2D MapPan;

	// the map domain that this marker is displayed in
	UPROPERTY(BlueprintReadWrite, Category="PaintGeometry", meta=(Categories="Navigator.NavigatorTypes"))
	FGameplayTag Domain;

	FMarkerOnPaintData()
	: MapOrigin(TNumericLimits<double>::Max()), MapPan(TNumericLimits<double>::Max())
	{}
};

/**
 * Base class for a navigation marker in Narrative Navigator. 
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVEARSENAL_API UMapMarker : public UObject
{
	GENERATED_BODY()
	
protected:

	// Allows the Object to get a valid UWorld from it's outer.
	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}

		UObject* Outer = GetOuter();

		while (Outer)
		{
			UWorld* World = Outer->GetWorld();
			if (World)
			{
				return World;
			}

			Outer = Outer->GetOuter();
		}

		return nullptr;
	}

	// Sets default values for this component's properties
	UMapMarker(const FObjectInitializer& ObjectInitializer);

public:
	
	//Add or remove the marker from all navigators. To remove from a specific navigator, use the Add/RemoveDomain functions. 
	UFUNCTION(BlueprintCallable, Category = "Map Marker")
	virtual void RegisterMarker();

	UFUNCTION(BlueprintCallable, Category = "Map Marker")
	virtual void RemoveMarker();

	UFUNCTION(BlueprintCallable, Category = "Map Marker")
	void SetDrawMarkerPathEnabled(const bool Enabled);

	UFUNCTION()
	virtual void OnOwnerDestroyed(AActor* DestroyedActor);

	UFUNCTION(BlueprintNativeEvent, Category = "Marker")
	void OnMarkerAdded(class UNarrativeNavigationComponent* OwnerNavComp);
	virtual void OnMarkerAdded_Implementation(class UNarrativeNavigationComponent* OwnerNavComp);

	UFUNCTION(BlueprintNativeEvent, Category = "Marker")
	void OnMarkerRemoved(class UNarrativeNavigationComponent* OwnerNavComp);
	virtual void OnMarkerRemoved_Implementation(class UNarrativeNavigationComponent* OwnerNavComp);

	//The properties the navigation marker should use 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup")
	FNavigationMarkerSettings DefaultMarkerSettings;

	//If the world map needs any settings changed from the defaults, override them in here! 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (DisplayName="Override Settings (World Map)"))
	FNavigationMarkerSettings WorldMapOverrideSettings;

	//If the minimap needs any settings changed from the defaults, override them in here! 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (DisplayName="Override Settings (Minimap)"))
	FNavigationMarkerSettings MinimapOverrideSettings;

	//If the compass needs any settings changed from the defaults, override them in here! 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (DisplayName="Override Settings (Compass)"))
	FNavigationMarkerSettings CompassOverrideSettings;

	//If the screen space marker need any settings changed from the defaults, override them in here! 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (DisplayName="Override Settings (Screen Space)"))
	FNavigationMarkerSettings ScreenspaceOverrideSettings;

	
	//Where the marker should show up in the world 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (ExposeOnSpawn=true))
	FTransform MarkerTransform; 

	//The actor this marker represents 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (ExposeOnSpawn=true))
	TObjectPtr<class AActor> ActorOwner;

		// If true, MarkerOnPaint is called
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup")
	uint8 bWantsOnPaint:1;

protected:

	//Text the marker should show as its action text in the world map when hovered
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup")
	FText DefaultMarkerActionText;

	//ZOrder UI should draw marker on. greater = higher priority
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup")
	int32 ZOrder;

	//Whether this marker should always be on the edge of the map, rather than going beyond the maps border 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup")
	bool bPinToMapEdge;

	//Compass only - The distance at which the compass begins to fade the marker out
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Setup")
	float MarkerStartFadeOutDistance;

	//Compass - only The distance at which we the compass begins to fade the marker in
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Setup")
	float MarkerStartFadeInDistance;

	//What navigation widgets will this navigation marker show up on?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", meta = (Categories = "Navigator.NavigatorTypes"))
	FGameplayTagContainer MarkerDomain;


	
	//Any values in here will be drawn by the map onto the UI. Really useful for zones like hostile zones, quest zones etc.
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup", AdvancedDisplay)
	//FZoneDrawData Zone;

public:

	//Grab the settings for the nav marker and any overrides - this is just called once on init for marker icons
	UFUNCTION(BlueprintPure, Category = "Marker")
	FNavigationMarkerSettings GetMarkerSettings(UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) const FGameplayTag& NavigatorType) const ;

	/** Whether this marker is currently enabled for the exact navigator domain. */
	bool HasDomain(const FGameplayTag& Domain) const { return MarkerDomain.HasTagExact(Domain); }

	//Call this function whenever you've changed some settings on the marker! It will inform the UI widgets they need to refresh themselves. 
	UFUNCTION(BlueprintCallable, Category = "Marker")
	void RefreshMarker();

	UPROPERTY(BlueprintAssignable, Category = "Marker")
	FOnRefreshRequired OnRefreshRequired;

	//Called when the marker is selected in the world map 
	UPROPERTY(BlueprintAssignable, Category = "Marker")
	FOnSelected OnSelected;
	
	//Text for what happens when we click on the marker 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Marker")
	FText GetMarkerActionText(class UNarrativeNavigationComponent* Selector) const;
	virtual FText GetMarkerActionText_Implementation(class UNarrativeNavigationComponent* Selector) const;

	//Display text for the marker, override if you need dynamic 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Marker")
	FText GetMarkerDisplayText(class UNarrativeNavigationComponent* Selector, UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) const FGameplayTag& NavigatorType, FText& OutSubtitleText) const;
	virtual FText GetMarkerDisplayText_Implementation(class UNarrativeNavigationComponent* Selector, UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) const FGameplayTag& NavigatorType, FText& OutSubtitleText) const;

	//Display color for the marker, override if you need dynamic 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Marker")
	FLinearColor GetMarkerColor(class UNarrativeNavigationComponent* Selector, UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) const FGameplayTag& NavigatorType) const;
	virtual FLinearColor GetMarkerColor_Implementation(class UNarrativeNavigationComponent* Selector, UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) const FGameplayTag& NavigatorType) const;

	//Handle what the marker should be clickable or hoverable, or if it is just an icon with no interaction
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Marker")
	bool CanInteract(class UNarrativeNavigationComponent* Selector) const;
	virtual bool CanInteract_Implementation(class UNarrativeNavigationComponent* Selector) const;

	//Handle what the marker should do when clicked in the map
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Marker")
	void OnSelect(class UNarrativeNavigationComponent* Selector);
	virtual void OnSelect_Implementation(class UNarrativeNavigationComponent* Selector);

	UFUNCTION(BlueprintCallable, Category = "Marker")
	void SetDefaultDomains(UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) const FGameplayTagContainer& NewMarkerDomain);

	UFUNCTION(BlueprintCallable, Category = "Marker")
	void SetZOrder(const int32 NewZOrder);

	// Check if this marker should be shown in the given location 
	UFUNCTION(BlueprintCallable, Category = "Marker")
	virtual void SetDomains(UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) const FGameplayTagContainer& NewMarkerDomain);

	// addd domains 
	UFUNCTION(BlueprintCallable, Category = "Marker")
	virtual void AddDomains(UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) const FGameplayTagContainer& NewMarkerDomains);
	
	//remove domains 
	UFUNCTION(BlueprintCallable, Category = "Marker")
	virtual void RemoveDomains(UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) const FGameplayTagContainer& RemoveDomains);

	//Gets the transform of the marker
	UFUNCTION(BlueprintCallable, Category = "Marker")
	virtual FTransform GetMarkerTransform() const;

	//Gets the desired Zorder of the marker
	UFUNCTION(BlueprintCallable, Category = "Marker")
	virtual int32 GetMarkerZOrder() const;

	// Gets the position of the marker in map space
	UFUNCTION(BlueprintPure, Category="Marker")
	FVector2D GetMarkerMapLocalPosition(const FVector2D MapOrigin, const FVector2D MapPan) const;

	// Gets the position of the marker in local paint space
	UFUNCTION(BlueprintPure, Category="Marker")
	FVector2D GetMarkerTopLeftLocalPosition(UPARAM(Ref) FMarkerOnPaintData& OnPaintData) const;

	// when bWantsOnPaint is true, this function is called
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Marker|Drawing")
	void MarkerOnPaint(UPARAM(Ref) FPaintContext& Context, UPARAM(Ref) FMarkerOnPaintData& OnPaintData) const;
	
#pragma region Breadcrumbs

public:

	// Whether we draw breadcrumbs on the UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Marker Setup|Breadcrumbs")
	bool bDrawBreadcrumbs = true;

	// The distance threshhold in which we will regenerate the breadcrumb path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup|Breadcrumbs")
	double UpdateNavDistanceThreshhold = 200.f;

	// The distance between points to use when querying the nav path spline
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup|Breadcrumbs")
	float DistanceBetweenPoints = 300.f;

	// The rate that we check for breadcrumb nav path updates
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup|Breadcrumbs")
	float UpdateNavPathRate = 4.f;

	// The color of the bread crumb trail
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup|Breadcrumbs")
	FLinearColor BreadcrumbColor = FLinearColor::Gray;

	// The thickness of the breadcrumb trail
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup|Breadcrumbs")
	float BreadcrumbThickness = 5.f;

	// The dash length (in pixels) of the breadcrumb trail
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker Setup|Breadcrumbs")
	float BreadcrumbDashLength = 10.f; 

	// Changes whether we apply breadcrumbs for this MapMarker
	UFUNCTION(BlueprintCallable, Category = "Marker|Breadcrumbs")
	void SetDrawBreadcrumbs(bool bCanDrawBreadcrumbs);

protected:
	virtual void InitializeBreadcrumb(AActor* NavActor);
	virtual void CleanupBreadcrumb();

	void AsyncPathFinished(uint32 QueryID, ENavigationQueryResult::Type QueryResult, TSharedPtr<FNavigationPath> NavigationPath);
	
	// Uses navmesh to calculate path to destination
	virtual void UpdateBreadcrumbNavPath();

	virtual void GeneratePath(ENavigationQueryResult::Type QueryResult, TSharedPtr<FNavigationPath> NavigationPath);
	virtual void DrawBreadcrumb(FPaintContext& Context, FMarkerOnPaintData& OnPaintData) const;

private:
	TWeakObjectPtr<AActor> OwnerNavActor;
	FTimerHandle RefreshPathTimerHandle;

	FVector CachedLocation;
	uint32 NavRequestID = INVALID_NAVQUERYID;

	FInterpCurveVector PathCurve;
	TArray<FVector> NavPath;
	
#pragma endregion
};
