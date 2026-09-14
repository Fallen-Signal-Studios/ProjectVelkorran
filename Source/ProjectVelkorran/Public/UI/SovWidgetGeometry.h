// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
class UWidget;

namespace SovWidgetGeometry
{
    /** Absolute rendered bounds, including DPI, safe area and render transforms.
     * Native widgets can be visible without populated paint/tick caches. */
    PROJECTVELKORRAN_API bool FindRenderedBounds(const UWidget* Widget, FSlateRect& Out);
}
