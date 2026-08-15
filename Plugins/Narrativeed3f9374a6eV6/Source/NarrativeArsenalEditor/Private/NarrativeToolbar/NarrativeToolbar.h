// Copyright Narrative Tools 2025.

#pragma once

namespace NarrativeToolbar
{
	static FString WebsiteURL = TEXT("https://www.narrativetools.io/");
	static FString DocsURL = TEXT("https://docs.narrativetools.io/");

	// utility
	static bool HasPlayWorld() { return GEditor && GEditor->PlayWorld != nullptr; }
	static bool HasNoPlayWorld() { return !HasPlayWorld(); }

	// actual entry creation
	TSharedRef<SWidget> GetToolbarMenuDropDown();
	void CreateSection();

	static void OpenNavigationGenerateTool();
	static void OpenNarrativeProjectPluginSettings();
};
