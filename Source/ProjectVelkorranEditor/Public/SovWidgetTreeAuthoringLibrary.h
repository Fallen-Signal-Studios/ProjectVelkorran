// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovWidgetTreeAuthoringLibrary.generated.h"

class UWidget;
class UWidgetBlueprint;

/** Editor-only widget tree construction, which script bindings cannot reach: UWidgetTree::ConstructWidget
 * is a template and UMG offers no reflected equivalent. Authors assets only; never runs a widget. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovWidgetTreeAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Adds a widget of WidgetClass named WidgetName to the Blueprint's tree.
	 *
	 * A null Parent makes it the root, which requires the tree to have none. The new widget is a
	 * variable, so a name matching a BindWidget property on the parent class binds to it.
	 * Returns null and leaves the tree untouched if the name is taken or the parent cannot hold it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Velkorran|Editor|Widgets")
	static UWidget* AddWidgetToTree(UWidgetBlueprint* Blueprint, UClass* WidgetClass, FName WidgetName, UWidget* Parent);

	/** The tree's root, which a freshly created Widget Blueprint already has. Null when the tree is empty. */
	UFUNCTION(BlueprintCallable, Category = "Velkorran|Editor|Widgets")
	static UWidget* GetRootWidget(UWidgetBlueprint* Blueprint);

	/** A widget in the tree by name, or null. */
	UFUNCTION(BlueprintCallable, Category = "Velkorran|Editor|Widgets")
	static UWidget* FindWidgetInTree(UWidgetBlueprint* Blueprint, FName WidgetName);

	/** Positions a widget already held by a canvas panel. Sizes are in the canvas's own units. */
	UFUNCTION(BlueprintCallable, Category = "Velkorran|Editor|Widgets")
	static bool SetCanvasSlot(UWidget* Widget, FVector2D Position, FVector2D Size, int32 ZOrder);

	/** One line per widget in the tree: name, class and parent, for verifying an authored result. */
	UFUNCTION(BlueprintCallable, Category = "Velkorran|Editor|Widgets")
	static TArray<FString> DescribeWidgetTree(UWidgetBlueprint* Blueprint);

	/** Which BindWidget/BindWidgetOptional properties on the parent class the tree currently satisfies. */
	UFUNCTION(BlueprintCallable, Category = "Velkorran|Editor|Widgets")
	static TArray<FString> DescribeWidgetBindings(UWidgetBlueprint* Blueprint);
};
