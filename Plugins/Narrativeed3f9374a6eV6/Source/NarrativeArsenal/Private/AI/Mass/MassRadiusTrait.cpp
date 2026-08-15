// Copyright Narrative Tools 2025.


#include "AI/Mass/MassRadiusTrait.h"

#include "MassEntityTemplateRegistry.h"

void UMassRadiusTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment(FConstStructView::Make(RadiusFragment));
}
