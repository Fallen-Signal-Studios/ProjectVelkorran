// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovWidgetTreeAuthoringLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_CallFunction.h"
#include "Materials/MaterialInterface.h"

FVector4 USovWidgetTreeAuthoringLibrary::GetWidgetPaintBounds(UWidget* Widget)
{
    if (!IsValid(Widget)) { return FVector4(0,0,0,0); }
    const FGeometry& Geometry = Widget->GetPaintSpaceGeometry();
    if (Geometry.GetLocalSize().IsNearlyZero()) { return FVector4(0,0,0,0); }
    const FVector2D Min = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D Max = Geometry.LocalToAbsolute(Geometry.GetLocalSize());
    return FVector4(Min.X,Min.Y,Max.X,Max.Y);
}

bool USovWidgetTreeAuthoringLibrary::ReplaceMaterialFactoryParent(UWidgetBlueprint* Blueprint,
    UMaterialInterface* Expected, UMaterialInterface* Replacement)
{
    if (!IsValid(Blueprint) || !IsValid(Expected) || !IsValid(Replacement)) { return false; }
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<UEdGraphPin*> Parents;
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            auto* Call = Cast<UK2Node_CallFunction>(Node);
            if (!Call || Call->FunctionReference.GetMemberName() != TEXT("CreateDynamicMaterialInstance")) { continue; }
            UEdGraphPin* Parent = Call->FindPin(TEXT("Parent"));
            if (!Parent || !Parent->LinkedTo.IsEmpty() || (Parent->DefaultObject != Expected && Parent->DefaultObject != Replacement)) { return false; }
            Parents.Add(Parent);
        }
    }
    if (Parents.Num() != 1) { return false; }
    UEdGraphPin* Parent = Parents[0];
    if (Parent->DefaultObject == Replacement) { return true; }
    Blueprint->Modify();
    Parent->GetOwningNode()->GetSchema()->TrySetDefaultObject(*Parent, Replacement);
    if (Parent->DefaultObject != Replacement) { return false; }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return true;
}

UWidget* USovWidgetTreeAuthoringLibrary::AddWidgetToTree(UWidgetBlueprint* Blueprint, UClass* WidgetClass,
	FName WidgetName, UWidget* Parent)
{
	if (!IsValid(Blueprint) || !Blueprint->WidgetTree || !IsValid(WidgetClass)
		|| !WidgetClass->IsChildOf(UWidget::StaticClass()) || WidgetClass->HasAnyClassFlags(CLASS_Abstract)
		|| WidgetName.IsNone()) { return nullptr; }
	UWidgetTree* const Tree = Blueprint->WidgetTree;
	// A duplicate name would silently rename the new widget and break the binding it was created for.
	if (Tree->FindWidget(WidgetName)) { return nullptr; }

	UPanelWidget* const Panel = Cast<UPanelWidget>(Parent);
	if (Parent && !Panel) { return nullptr; }
	if (!Parent && Tree->RootWidget) { return nullptr; }

	UWidget* const Created = Tree->ConstructWidget<UWidget>(WidgetClass, WidgetName);
	if (!Created) { return nullptr; }
	// Only a variable can satisfy a BindWidget property on the parent class.
	Created->bIsVariable = true;
	if (!Parent)
	{
		Tree->RootWidget = Created;
	}
	else if (!Panel->AddChild(Created))
	{
		// A content widget already holding a child refuses another; leave the tree as it was.
		Tree->RemoveWidget(Created);
		return nullptr;
	}
	Blueprint->OnVariableAdded(WidgetName);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return Created;
}

UWidget* USovWidgetTreeAuthoringLibrary::GetRootWidget(UWidgetBlueprint* Blueprint)
{
	return IsValid(Blueprint) && Blueprint->WidgetTree ? Blueprint->WidgetTree->RootWidget : nullptr;
}

UWidget* USovWidgetTreeAuthoringLibrary::FindWidgetInTree(UWidgetBlueprint* Blueprint, FName WidgetName)
{
	return IsValid(Blueprint) && Blueprint->WidgetTree ? Blueprint->WidgetTree->FindWidget(WidgetName) : nullptr;
}

bool USovWidgetTreeAuthoringLibrary::SetCanvasSlot(UWidget* Widget, FVector2D Position, FVector2D Size, int32 ZOrder)
{
	auto* const CanvasSlot = IsValid(Widget) ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
	if (!CanvasSlot || Position.ContainsNaN() || Size.ContainsNaN() || Size.X <= 0. || Size.Y <= 0.) { return false; }
	CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	CanvasSlot->SetAlignment(FVector2D::ZeroVector);
	CanvasSlot->SetAutoSize(false);
	CanvasSlot->SetOffsets(FMargin(static_cast<float>(Position.X), static_cast<float>(Position.Y),
		static_cast<float>(Size.X), static_cast<float>(Size.Y)));
	CanvasSlot->SetZOrder(ZOrder);
	return true;
}

TArray<FString> USovWidgetTreeAuthoringLibrary::DescribeWidgetTree(UWidgetBlueprint* Blueprint)
{
	TArray<FString> Lines;
	if (!IsValid(Blueprint) || !Blueprint->WidgetTree) { return Lines; }
	Blueprint->WidgetTree->ForEachWidget([&Lines](UWidget* Widget)
	{
		if (!Widget) { return; }
		const UWidget* const Parent = Widget->GetParent();
		Lines.Add(FString::Printf(TEXT("%s (%s) parent=%s variable=%d"), *Widget->GetName(),
			*Widget->GetClass()->GetName(), Parent ? *Parent->GetName() : TEXT("root"), Widget->bIsVariable ? 1 : 0));
	});
	Lines.Sort();
	return Lines;
}

TArray<FString> USovWidgetTreeAuthoringLibrary::DescribeWidgetBindings(UWidgetBlueprint* Blueprint)
{
	TArray<FString> Lines;
	const UClass* const Parent = IsValid(Blueprint) ? Blueprint->ParentClass.Get() : nullptr;
	if (!Parent || !Blueprint->WidgetTree) { return Lines; }
	for (TFieldIterator<FObjectPropertyBase> It(Parent); It; ++It)
	{
		const FObjectPropertyBase* const Property = *It;
		const bool bRequired = Property->HasMetaData(TEXT("BindWidget"));
		const bool bOptional = Property->HasMetaData(TEXT("BindWidgetOptional"));
		if (!bRequired && !bOptional) { continue; }
		const UWidget* const Found = Blueprint->WidgetTree->FindWidget(Property->GetFName());
		const bool bTypeMatches = Found && Property->PropertyClass && Found->IsA(Property->PropertyClass);
		Lines.Add(FString::Printf(TEXT("%s (%s%s) bound=%d typeMatches=%d"), *Property->GetName(),
			Property->PropertyClass ? *Property->PropertyClass->GetName() : TEXT("?"),
			bRequired ? TEXT(", required") : TEXT(", optional"), Found ? 1 : 0, bTypeMatches ? 1 : 0));
	}
	Lines.Sort();
	return Lines;
}
