// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectVelkorran.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"

// Legacy Narrative weapon Blueprints query this optional VFX debug switch.
static TAutoConsoleVariable<bool> CVarSovDrawWeaponVFX(
    TEXT("n.weapon.DrawVFX"),
    false,
    TEXT("Draw Narrative weapon VFX debug helpers."),
    ECVF_Default);

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, ProjectVelkorran, "ProjectVelkorran" );
