// Copyright Narrative Tools 2025.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FSlateMaterialBrush;

/// simple widget that displays a time ruler 
class STimeRuler : public SLeafWidget
{
	// size of the hours text
	float FontSize = 10.0f;
	// internal widget padding, matches SSlider
	float InternalPadding = 7.0f;
	// step size
	float StepSize = 0.01f;
	// tick height for hours
	float MajorTickHeight = 10.0f;
	// tick height in between values 
	float MinorTickHeight = 8.0f;
	// range min
	TSlateAttribute<float> RangeMinValueAttribute;
	// range max
	TSlateAttribute<float> RangeMaxValueAttribute;
	// value for when range is not used
	TSlateAttribute<float> ValueAttribute;
	// color of hour ticks
	FLinearColor MajorTickColor = FLinearColor::White;
	// color of in between value ticks
	FLinearColor MinorTickColor = FLinearColor::White;
	// color of range display bar
	FLinearColor RangeDisplayColor = FLinearColor::White;
	FOnFloatValueChanged OnMinValueChanged;
	FOnFloatValueChanged OnMaxValueChanged;
	FOnFloatValueChanged OnValueChanged;
	// background brush behind the ticks
	TSharedPtr<FSlateMaterialBrush> BackgroundBrush;
	// cache the last cursor to return to
	EMouseCursor::Type CachedCursor;
	// true if the min slider handle being used
	bool bUsingMinSliderHandle;
	float HandleWidth = 4.0f;
	
	
public:
	
	SLATE_DECLARE_WIDGET_API(STimeRuler, SLeafWidget, NARRATIVEARSENALEDITOR_API)

	SLATE_BEGIN_ARGS(STimeRuler)
		: _FontSize(10.0f),
		_InternalPadding(7.0f),
		_StepSize(0.01f),
		_MajorTickHeight(10.0f),
		_MinorTickHeight(10.0f),
		_RangeMinValue(0.0f),
		_RangeMaxValue(1.0f),
		_Value(0.0f),
		_MajorTickColor(FLinearColor::White),
		_MinorTickColor(FLinearColor(1.0f,1.0f,1.0f, 0.6f)), // transparent white
		_RangeDisplayColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.75f)),
		_OnMinValueChanged(),
		_OnMaxValueChanged()
		{}
		SLATE_ARGUMENT(float, FontSize)
		SLATE_ARGUMENT(float, InternalPadding)
		SLATE_ARGUMENT(float, StepSize)
		SLATE_ARGUMENT(float, MajorTickHeight)
		SLATE_ARGUMENT(float, MinorTickHeight)
		SLATE_ATTRIBUTE(float, RangeMinValue)
		SLATE_ATTRIBUTE(float, RangeMaxValue)
		SLATE_ATTRIBUTE(float, Value)
		SLATE_ARGUMENT(FLinearColor, MajorTickColor)
		SLATE_ARGUMENT(FLinearColor, MinorTickColor)
		SLATE_ARGUMENT(FLinearColor, RangeDisplayColor)
		SLATE_EVENT(FOnFloatValueChanged, OnMinValueChanged)
		SLATE_EVENT(FOnFloatValueChanged, OnMaxValueChanged)
		SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
		
	SLATE_END_ARGS()

	STimeRuler();
	virtual ~STimeRuler() = default;
	void Construct(const FArguments& InArgs);
	
	void SetMinValue(const float InValue);
	void SetMaxValue(const float InValue);
	
private:

	/* SWidget */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	/* SWidget */

	/* SLeafWidget */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	/* SLeafWidget */

	bool IsRangeSlider() const;
	TSlateAttribute<float>& GetCurrentValueAttribute();
	void CommitValue(float NewValue);
	FVector2D GetBarSize(const FGeometry& AllottedGeometry) const;
	float GetMinValuePositionX(const FGeometry& AllottedGeometry) const;
	float GetMaxValuePositionX(const FGeometry& AllottedGeometry) const;
	float PositionToValue(const FGeometry& MyGeometry, const UE::Slate::FDeprecateVector2DParameter& AbsolutePosition);
	
};
