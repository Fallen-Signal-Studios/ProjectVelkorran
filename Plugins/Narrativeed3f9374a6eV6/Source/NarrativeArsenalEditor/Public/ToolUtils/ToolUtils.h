// Copyright Narrative Tools 2025.

#pragma once

class FRestartRequiredNotification;

namespace NarrativeToolUtils
{

	void OpenMapWithSaveCheck(const FSoftObjectPath& MapSoftObjPath);
	
	void NotifyRequiredRestart();
	void ClearRequiredRestart();
	
}
