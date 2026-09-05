// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovNativeGameplayHUD.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "NarrativeGameplayTags.h"

TSharedRef<SWidget> USovNativeGameplayHUD::RebuildWidget()
{
    if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree")); }
    if (!WidgetTree->RootWidget)
    {
        auto* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("NarrativeLayers"));
        Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible); WidgetTree->RootWidget = Root;
        GameLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("GameLayer"));
        MenuLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("MenuLayer"));
        ModalLayer = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("ModalLayer"));
        for (UCommonActivatableWidgetStack* Layer : {GameLayer.Get(), MenuLayer.Get(), ModalLayer.Get()})
        {
            Layer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            UOverlaySlot* Slot = Root->AddChildToOverlay(Layer);
            Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill);
        }
    }
    return Super::RebuildWidget();
}
void USovNativeGameplayHUD::NativeConstruct()
{
    Super::NativeConstruct();
    const auto& Tags = FNarrativeGameplayTags::Get();
    if (GameLayer) { RegisterLayer(Tags.UI_Layer_Game, GameLayer); }
    if (MenuLayer) { RegisterLayer(Tags.UI_Layer_Menu, MenuLayer); }
    if (ModalLayer) { RegisterLayer(Tags.UI_Layer_Modal, ModalLayer); }
}
