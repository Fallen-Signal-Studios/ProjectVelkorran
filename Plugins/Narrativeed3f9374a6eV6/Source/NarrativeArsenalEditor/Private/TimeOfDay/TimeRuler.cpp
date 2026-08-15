// Copyright Narrative Tools 2025.

#include "TimeRuler.h"

#include "SlateMaterialBrush.h"
#include "Fonts/FontMeasure.h"

#define LOCTEXT_NAMESPACE "STimeRuler"

SLATE_IMPLEMENT_WIDGET(STimeRuler)
void STimeRuler::PrivateRegisterAttributes(FSlateAttributeInitializer& AttributeInitializer)
{
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION_WITH_NAME(AttributeInitializer, "RangeMinValue", RangeMinValueAttribute, EInvalidateWidgetReason::Paint);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION_WITH_NAME(AttributeInitializer, "RangeMaxValue", RangeMaxValueAttribute, EInvalidateWidgetReason::Paint);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION_WITH_NAME(AttributeInitializer, "Value",         ValueAttribute,         EInvalidateWidgetReason::Paint);
	AttributeInitializer.OverrideInvalidationReason("EnabledState", FSlateAttributeDescriptor::FInvalidateWidgetReasonAttribute{EInvalidateWidgetReason::Paint});
	AttributeInitializer.OverrideInvalidationReason("Hovered",      FSlateAttributeDescriptor::FInvalidateWidgetReasonAttribute{EInvalidateWidgetReason::Paint});
}

STimeRuler::STimeRuler()
	: RangeMinValueAttribute(*this, 0.0f),
	RangeMaxValueAttribute(*this, 1.0f),
	ValueAttribute(*this, 1.0f),
	CachedCursor(),
	bUsingMinSliderHandle(false)
{
}

void STimeRuler::Construct(const FArguments& InArgs)
{
	FontSize = InArgs._FontSize;
	InternalPadding = InArgs._InternalPadding;
	StepSize = InArgs._StepSize;
	MajorTickHeight = InArgs._MajorTickHeight;
	MinorTickHeight = InArgs._MinorTickHeight;
	RangeMinValueAttribute.Assign(*this, InArgs._RangeMinValue);
	RangeMaxValueAttribute.Assign(*this, InArgs._RangeMaxValue);
	ValueAttribute.Assign(*this, InArgs._Value);
	MajorTickColor = InArgs._MajorTickColor;
	MinorTickColor = InArgs._MinorTickColor;
	RangeDisplayColor = InArgs._RangeDisplayColor;
	OnMinValueChanged = InArgs._OnMinValueChanged;
	OnMaxValueChanged = InArgs._OnMaxValueChanged;
	OnValueChanged = InArgs._OnValueChanged;
	
	// load background material and assign to brush
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/NarrativePro/Pro/Editor/Slate/M_TimeOfDay.M_TimeOfDay")))
	{
		BackgroundBrush = MakeShareable(new FSlateMaterialBrush(*Material, FVector2D(64.0f, 64.0f)));
	}
}

void STimeRuler::SetMinValue(const float InValue)
{
	RangeMinValueAttribute.Assign(*this, InValue);
}

void STimeRuler::SetMaxValue(const float InValue)
{
	RangeMaxValueAttribute.Assign(*this, InValue);
}

int32 STimeRuler::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FVector2D Size = AllottedGeometry.GetLocalSize();
	FVector2D BackgroundPosition(InternalPadding, 0);
	const FVector2D BarSize = GetBarSize(AllottedGeometry);
	FPaintGeometry BackgroundGeometry = AllottedGeometry.ToPaintGeometry(BarSize, FSlateLayoutTransform(BackgroundPosition));
	
    // draw background darker with material
    FSlateDrawElement::MakeBox(
        OutDrawElements,
        ++LayerId,
        BackgroundGeometry,
        BackgroundBrush.Get(),
        ESlateDrawEffect::None,
        IsRangeSlider()? FLinearColor::Gray : FLinearColor::White
    );

	// number of pixels each hour represents
	const float PixelsPerHour = (Size.X - 2 * InternalPadding) / 24.0f;
	for (int32 Hour = 0; Hour <= 24; ++Hour)
	{
		const float XSpacing = InternalPadding + Hour * PixelsPerHour;
		const bool bIsMajor = Hour % 6 == 0;
		const float NewTickHeight = bIsMajor? MajorTickHeight : MinorTickHeight;

		// draw tick line
		FSlateDrawElement::MakeLines( OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
			{ FVector2D(XSpacing, 0), FVector2D(XSpacing, NewTickHeight) }, ESlateDrawEffect::None, MinorTickColor,
			true, bIsMajor? 2.0f : 1.0f);

		// draw text below major ticks
		if (bIsMajor)
		{
			// get font width
			FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("NormalFont");
			FontInfo.Size = FontSize;
			const TSharedRef<FSlateFontMeasure>& FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			
			const FText TimeLabel = FText::Format(LOCTEXT("TimeRuler_MajorTick_Label_Format", "{0}:00"), FText::AsNumber(Hour == 24? 24 : Hour % 24));
			const FVector2D TextSize = FontMeasureService->Measure(TimeLabel, FontInfo);

			// offset text inwards when on edges to avoid clipping ->|<-
			const float HalfTextWidth = TextSize.X / 2.0f;
			float TextHorizontalOffset = HalfTextWidth;
			if (Hour == 0)
			{
				TextHorizontalOffset -= HalfTextWidth;
			}
			else if (Hour == 24)
			{
				TextHorizontalOffset += HalfTextWidth;
			}

			// create time hour text
			const FVector2D LabelPos = FVector2D(XSpacing - TextHorizontalOffset, NewTickHeight + 2.0f);
			const FPaintGeometry LabelGeometry = AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(LabelPos));
			FSlateDrawElement::MakeText(OutDrawElements, ++LayerId, LabelGeometry, TimeLabel, FontInfo, ESlateDrawEffect::None, MajorTickColor);
		}
	}

	const FVector2D InitialBarLocation(InternalPadding, BarSize.Y);
	const float RangeMakerHeight = MajorTickHeight + 4.0f;
	// range display
	if (IsRangeSlider())
	{
		// cache the current value, it will not change during paint
		const float CurrentMinRangeValue = RangeMinValueAttribute.Get();
		const float CurrentMaxRangeValue = RangeMaxValueAttribute.Get();
		// does the range span over the nighttime? (i.e. 18:00 - 02:00)
		const bool bSpansNight = CurrentMinRangeValue > CurrentMaxRangeValue;
		// initial reused values
		const float MinPositionX = InitialBarLocation.X + (BarSize.X * CurrentMinRangeValue);
		const float MaxPositionX = BarSize.X * (CurrentMaxRangeValue - CurrentMinRangeValue);
		// both arrays set with X being 0.0 as it is the only value changed
		TArray<FVector2D> MinLinePoints = {
			{0.0f, -3.0f},
			{0.0f, RangeMakerHeight}
		};
		TArray<FVector2D> MaxLinePoints = {
			{0.0f, -3.0f},
			{0.0f, RangeMakerHeight}
		};
		
		// |##---##|
		if (bSpansNight)
		{
			// draw start to max
			// |##-----|
			const FVector2D MaxBarStart(InitialBarLocation.X, BarSize.Y);
			const FVector2D MaxBarEnd = BarSize * FVector2D{CurrentMaxRangeValue, 1.0f};
			const FPaintGeometry MaxGeometry = AllottedGeometry.ToPaintGeometry( MaxBarEnd, FSlateLayoutTransform(MaxBarStart));
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				MaxGeometry,
				FCoreStyle::Get().GetBrush("GenericWhiteBox"),
				ESlateDrawEffect::None,
				RangeDisplayColor
			);

			// draw min to end box
			// |-----##|
			const FVector2D MinBarStart(MinPositionX, BarSize.Y);
			const FVector2D MinBarEnd = BarSize - FVector2D{MinBarStart.X, 0.0f};
			const FPaintGeometry MinGeometry = AllottedGeometry.ToPaintGeometry(
				MinBarEnd + FVector2D{InternalPadding, 0.0f}, FSlateLayoutTransform(MinBarStart));
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				MinGeometry,
				FCoreStyle::Get().GetBrush("GenericWhiteBox"),
				ESlateDrawEffect::None,
				RangeDisplayColor
			);
			
			MinLinePoints[0].X = MinBarStart.X;
			MinLinePoints[1].X = MinBarStart.X;
			MaxLinePoints[0].X = MaxBarStart.X + MaxBarEnd.X;
			MaxLinePoints[1].X = MaxBarStart.X + MaxBarEnd.X;
		}
		else
		{
			// draw normal range
			// |--###--|
			const FVector2D BarStart(MinPositionX, BarSize.Y);
			const FVector2D BarEnd(MaxPositionX, BarSize.Y);
			const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry(
				BarEnd, FSlateLayoutTransform(BarStart));
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				Geometry,
				FCoreStyle::Get().GetBrush("GenericWhiteBox"),
				ESlateDrawEffect::None,
				RangeDisplayColor
			);

			MinLinePoints[0].X = MinPositionX;
			MinLinePoints[1].X = MinPositionX;
			MaxLinePoints[0].X = MinPositionX + MaxPositionX;
			MaxLinePoints[1].X = MinPositionX + MaxPositionX;
		}

		// draw green marker for max range
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			MinLinePoints,
			ESlateDrawEffect::None,
			FLinearColor::Green,
			false,
			HandleWidth
		);
		
		// draw red marker for max range
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			MaxLinePoints,
			ESlateDrawEffect::None,
			FLinearColor(0.7f, 0.04f, 0.02f, 1.0f),
			false,
			HandleWidth
		);		
	}
	else // non range display
	{
		const float CurrentValue = ValueAttribute.Get();
		const float ValueMarkerPosition = InitialBarLocation.X + (BarSize.X * CurrentValue);
		TArray<FVector2D> ValueMarkerLinePoints = {
			{ValueMarkerPosition, -3.0f},
			{ValueMarkerPosition, RangeMakerHeight}
		};
		TArray<FVector2D> MaxLinePoints = {
			{ValueMarkerPosition, -3.0f},
			{ValueMarkerPosition, RangeMakerHeight}
		};

		// draw green marker for value
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			ValueMarkerLinePoints,
			ESlateDrawEffect::None,
			FLinearColor::Green,
			false,
			HandleWidth
		);
	}

    return ++LayerId;
}

FVector2D STimeRuler::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(0, MajorTickHeight + 2.0f + (FontSize / 2.0f));
}

FReply STimeRuler::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton))
	{
		CachedCursor = GetCursor().Get(EMouseCursor::Default);
		//OnMouseCaptureBegin.ExecuteIfBound();
		const FVector2D LocalMousePos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		const float MinPosX = GetMinValuePositionX(MyGeometry);
		const float MaxPosX = GetMaxValuePositionX(MyGeometry);
		const float DistToMin = FMath::Abs(LocalMousePos.X - MinPosX);
		const float DistToMax = FMath::Abs(LocalMousePos.X - MaxPosX);
		bUsingMinSliderHandle = DistToMin < DistToMax;
		
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply STimeRuler::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && HasMouseCaptureByUser(MouseEvent.GetUserIndex(), MouseEvent.GetPointerIndex()))
	{
		SetCursor(CachedCursor);
				
		return FReply::Handled().ReleaseMouseCapture();	
	}

	return FReply::Unhandled();
}

FReply STimeRuler::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (HasMouseCaptureByUser(MouseEvent.GetUserIndex(), MouseEvent.GetPointerIndex()))
	{
		SetCursor(EMouseCursor::ResizeLeftRight);
		CommitValue(PositionToValue(MyGeometry, MouseEvent.GetScreenSpacePosition()));
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

bool STimeRuler::IsRangeSlider() const
{
	if (!GetParentWidget().Get())
	{
		return false;
	}
	
	return RangeMinValueAttribute.IsBound(*this) && RangeMaxValueAttribute.IsBound(*this);
}

SWidget::TSlateAttribute<float>& STimeRuler::GetCurrentValueAttribute()
{
	if (IsRangeSlider())
	{
		return bUsingMinSliderHandle? RangeMinValueAttribute : RangeMaxValueAttribute;
	}
	
	return ValueAttribute;
}

void STimeRuler::CommitValue(float NewValue)
{
	TSlateAttribute<float>& Attribute = bUsingMinSliderHandle? RangeMinValueAttribute : RangeMaxValueAttribute; 
	const float OldValue = Attribute.Get();

	if (NewValue != OldValue)
	{
		if (!Attribute.IsBound(*this))
		{
			Attribute.Assign(*this, NewValue);
		}

		Invalidate(EInvalidateWidgetReason::Paint);

		if (IsRangeSlider())
		{
			if (bUsingMinSliderHandle)
			{
				OnMinValueChanged.ExecuteIfBound(NewValue);
			}
			else
			{
				OnMaxValueChanged.ExecuteIfBound(NewValue);
			}
		}
		
		OnValueChanged.ExecuteIfBound(NewValue);
	}
}

FVector2D STimeRuler::GetBarSize(const FGeometry& AllottedGeometry) const
{
	FVector2D Size = AllottedGeometry.GetLocalSize();
	return {Size.X - InternalPadding * 2.0f, IsRangeSlider()? MajorTickHeight / 2.0f : MajorTickHeight};
}

float STimeRuler::GetMinValuePositionX(const FGeometry& AllottedGeometry) const
{
	FVector2D BarSize = GetBarSize(AllottedGeometry);
	const FVector2D InitialBarLocation(InternalPadding, 0.0f);
	return InitialBarLocation.X + (BarSize.X * RangeMinValueAttribute.Get());	
}

float STimeRuler::GetMaxValuePositionX(const FGeometry& AllottedGeometry) const
{
	FVector2D BarSize = GetBarSize(AllottedGeometry);
	return BarSize.X * RangeMaxValueAttribute.Get();
}

float STimeRuler::PositionToValue( const FGeometry& MyGeometry, const UE::Slate::FDeprecateVector2DParameter& AbsolutePosition )
{
	// get correct attribute
	TSlateAttribute<float>& ValueSlateAttribute = bUsingMinSliderHandle ? RangeMinValueAttribute : RangeMaxValueAttribute;
	// get relative value, then calculate direction
	const FVector2f LocalPosition = MyGeometry.AbsoluteToLocal(AbsolutePosition);
	const float Denominator = MyGeometry.Size.X - HandleWidth;
	float RelativeValue = (Denominator != 0.f) ? (LocalPosition.X - (HandleWidth / 2.0f)) / Denominator : 0.f;
	RelativeValue = FMath::Clamp(RelativeValue, 0.0f, 1.0f);
	const float Direction = ValueSlateAttribute.Get() - RelativeValue;
	if (StepSize <= 0)
	{
		// invalid step size, keep current value
		return ValueSlateAttribute.Get();
	}
	float Steps = FMath::Abs(Direction) / StepSize;
	Steps = FMath::RoundHalfFromZero(Steps);
	const float ClampedDist = Steps * StepSize;
	if (Direction > StepSize / 2.0f)
	{
		return FMath::Clamp(ValueSlateAttribute.Get() - ClampedDist, 0.0f, 1.0f);
	}
	if (Direction < StepSize / -2.0f)
	{
		return FMath::Clamp(ValueSlateAttribute.Get() + ClampedDist, 0.0f, 1.0f);
	}
	return ValueSlateAttribute.Get();
}

#undef LOCTEXT_NAMESPACE
