// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovWidgetGeometry.h"
#include "Components/Widget.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"

bool SovWidgetGeometry::FindRenderedBounds(const UWidget* Widget, FSlateRect& Out)
{
    if (!IsValid(Widget) || !Widget->IsRendered()) { return false; }
    const auto ValidGeometry = [](const FGeometry& Geometry)
    {
        const auto Size = Geometry.GetLocalSize();
        return !Size.ContainsNaN() && Size.X > 0.f && Size.Y > 0.f;
    };
    const FGeometry& Cached = Widget->GetCachedGeometry();
    if (ValidGeometry(Cached)) { Out = Cached.GetRenderBoundingRect(); return true; }
    if (!FSlateApplication::IsInitialized()) { return false; }
    const auto SlateWidget = Widget->GetCachedWidget();
    if (!SlateWidget.IsValid()) { return false; }
    FWidgetPath Path;
    if (!FSlateApplication::Get().GeneratePathToWidgetUnchecked(SlateWidget.ToSharedRef(), Path)) { return false; }
    const auto Arranged = Path.FindArrangedWidget(SlateWidget.ToSharedRef());
    if (!Arranged.IsSet() || !ValidGeometry(Arranged->Geometry)) { return false; }
    Out = Arranged->Geometry.GetRenderBoundingRect();
    return true;
}
