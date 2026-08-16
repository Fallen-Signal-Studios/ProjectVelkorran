// Copyright Epic Games, Inc. All Rights Reserved.

#include "NP_CodeTemplate.h"

#define LOCTEXT_NAMESPACE "FNP_CodeTemplateModule"

void FNP_CodeTemplateModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FNP_CodeTemplateModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNP_CodeTemplateModule, NP_CodeTemplate)