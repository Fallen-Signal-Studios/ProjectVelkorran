// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SovHolographicHUDSurface.generated.h"

class UCanvasPanel;
class UImage;
class UPanelWidget;
class UProgressBar;
class UTextBlock;

/** Which authored art a pip stands for. Mirrors ESovAbilityHUDIcon so a widget can switch on it. */
UENUM(BlueprintType)
enum class ESovHolographicHUDIcon : uint8
{
	Unknown, CinderGrenade, Hunger, Judgement, Slam, Requiem,
	Stillpoint, Wake, Staccato, NullPulse, Dispatch
};

/** How an ability reads right now, in the same terms the painted pips used. */
UENUM(BlueprintType)
enum class ESovHolographicHUDPipState : uint8
{
	EchoReady, NeedsEcho, Cooldown, Active, InputLocked, Unbound, WeaponRequired, Unavailable
};

/** One resource, already divided so the surface never has to guard against a zero maximum. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovHolographicHUDBar
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") float Current = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") float Maximum = 0.f;
	/** Zero when the resource has no maximum, so an absent resource reads as empty rather than full. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") float Fraction = 0.f;
};

/** One ability pip: what it is, how it is bound, and whether it can be spent. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovHolographicHUDPip
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FText Name;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FText Binding;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FText Status;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") ESovHolographicHUDIcon Icon = ESovHolographicHUDIcon::Unknown;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") ESovHolographicHUDPipState State = ESovHolographicHUDPipState::Unavailable;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") int32 SemanticSlot = 0;
	/** 1 when off cooldown, falling to 0 at the moment it was spent. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") float CooldownFraction = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") bool bReady = false;
};

/** One radar contact, with its position already resolved into the radar disc's own space. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovHolographicHUDContact
{
	GENERATED_BODY()

	/** Offset from the radar centre in safe-area pixels, so a widget can place it without trigonometry. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FVector2D Offset = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") float BearingDegrees = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") float NormalisedRange = 0.f;
	/** Full for a live sighting, fading while only remembered. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") float Alpha = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") bool bLiveSighting = false;
};

/** The colours the painted surface derived from the protagonist and the accessibility settings. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovHolographicHUDPalette
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FLinearColor Accent = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FLinearColor Glow = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FLinearColor ShieldFrom = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FLinearColor ShieldTo = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FLinearColor HealthFrom = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FLinearColor HealthTo = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FLinearColor Backing = FLinearColor::Transparent;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FLinearColor Line = FLinearColor::White;
	/** Opaque backings and colour-independent identity are required, not optional, when this is set. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") bool bHighContrast = false;
};

/**
 * Everything an authored HUD needs for one frame, in the safe area's own coordinates.
 *
 * The rectangles are the same ones SovHolographicHUDLayout gives the painter and gives the subtitle
 * and caption surfaces to avoid, so an authored widget that honours them is automatically in the
 * place the rest of the presentation already expects the HUD to be.
 */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovHolographicHUDView
{
	GENERATED_BODY()

	/** False when there is nothing to show; the surface should collapse rather than draw empty frames. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FGameplayTag Protagonist;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Resources") FSovHolographicHUDBar Health;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Resources") FSovHolographicHUDBar Shield;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Resources") FSovHolographicHUDBar Stamina;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Resources") FSovHolographicHUDBar Poise;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Resources") FSovHolographicHUDBar Echo;

	/** False when the wielded weapon has no magazine, so the readout is omitted rather than showing zero. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Ammo") bool bHasAmmo = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Ammo") int32 AmmoInClip = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Ammo") int32 AmmoReserve = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") TArray<FSovHolographicHUDPip> Pips;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") TArray<FSovHolographicHUDContact> Contacts;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD") FSovHolographicHUDPalette Palette;

	/** The accessibility UI scale already clamped to the range the layout accepts. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") float Scale = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") FVector2D SafeSize = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") FBox2D Plate = FBox2D(ForceInit);
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") FBox2D Ammo = FBox2D(ForceInit);
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") FBox2D Arc = FBox2D(ForceInit);
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") FBox2D Radar = FBox2D(ForceInit);
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") FVector2D ArcStart = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") FVector2D ArcControl = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") FVector2D ArcEnd = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") FVector2D RadarCentre = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") float RadarRadius = 0.f;
	/** Seconds of real time the radar sweep has run, for an authored rotation that does not stutter. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Layout") float SweepSeconds = 0.f;
};

/**
 * The base class an authored holographic HUD derives from.
 *
 * The C++ HUD keeps ownership of what is true - reading the resources, the grants, the contacts and the
 * accessibility settings once per frame, and computing the layout the rest of the presentation avoids.
 * A widget deriving from this decides only how that reads on screen, which is the part that wants
 * materials and authored art rather than drawn lines.
 *
 * Nothing here activates an ability, spends a resource or writes gameplay state.
 */
UCLASS(Abstract, Blueprintable)
class PROJECTVELKORRAN_API USovHolographicHUDSurface : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The current frame. Valid inside OnHolographicHUDUpdated and for the whole frame after it. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|HUD")
	const FSovHolographicHUDView& GetHolographicHUDView() const { return View; }

	/** Called once per HUD refresh, after View is current. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|HUD")
	void OnHolographicHUDUpdated(const FSovHolographicHUDView& UpdatedView);

	/** A point along the authored Echo arc, T running 0 to 1 from its start to its end. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|HUD")
	FVector2D ArcPoint(float T) const;

	/** Publishes a frame. Called by USovHolographicHUDWidget; a surface never reads gameplay itself. */
	void ApplyHolographicHUDView(const FSovHolographicHUDView& InView);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD")
	FSovHolographicHUDView View;

	/**
	 * Optional bindings. A widget that names a child after one of these has it placed and filled from the
	 * frame automatically, so an author styles the art and never rebuilds the arithmetic. Every one is
	 * optional: delete the child and that piece simply stops being driven.
	 *
	 * The four region widgets are positioned into the layout rectangles the rest of the presentation
	 * already avoids, so an authored HUD lands where subtitles and captions expect it without effort.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> PlateRegion;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> AmmoRegion;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ArcRegion;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RadarRegion;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ShieldBar;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> StaminaBar;
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> EchoBar;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText;

	/** A material-driven readout. Scalar "Fill" is set on its dynamic instance from the Echo fraction. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|HUD|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ArcFill;

	/** The parameter name driven on ArcFill's material instance. */
	UPROPERTY(EditDefaultsOnly, Category = "Sovereign|HUD")
	FName ArcFillParameter = TEXT("Fill");

	/** Whether the base places the bound region widgets. Clear it to lay the HUD out by hand. */
	UPROPERTY(EditDefaultsOnly, Category = "Sovereign|HUD")
	bool bPlaceRegionsFromLayout = true;

	virtual void NativeConstruct() override;

	/** Places and fills whatever is bound. Runs before the Blueprint event, so authored logic wins. */
	virtual void ApplyBoundWidgets();

private:
	void PlaceRegion(UPanelWidget* Region, const FBox2D& Box) const;
	/** Per-widget material instances; accessibility must never mutate a shared content asset. */
	void InitializeAccessibilityMaterials();
	UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInstanceDynamic>> AccessibilityMaterials;
	TOptional<bool> AppliedHighContrast;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> ArcMaterial;
};
