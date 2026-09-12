// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

/**
 * Content paths that automation tests are permitted to depend on.
 *
 * Rule: a test may only load content that is **version-controlled**. The repository tracks project
 * source, `Content/Aurelion/`, and the customised Narrative fork under `Plugins/`. Anything else in
 * `Content/` is local to one machine, so a test that loads it cannot pass in any fresh clone and its
 * result says more about the machine than about the code.
 *
 * Four tests previously loaded `/Game/SciFi_Drone_1/Textures/NPC_ReformationCombatDrone`, which is
 * untracked and has never been committed. They used it for one thing — a real, shipped
 * `AbilityConfiguration` with startup attributes and granted abilities. The authored campaign drones
 * carry exactly that configuration (`AC_NPC_ReformationDrone`, `GE_DroneStartupAttributes`, four
 * default abilities) and are tracked, so pointing at them is both reproducible and closer to
 * production than the original fixture was.
 *
 * Declared prerequisites and their classification live in Scripts/Manifests/ContentPrerequisites.json;
 * Scripts/Check-ContentPrerequisites.py enforces it.
 */
namespace SovTrackedContentPaths
{
/**
 * Authored campaign drone. Tracked under Content/Aurelion/. Supplies a real
 * AbilityConfiguration -> DefaultAttributes plus four DefaultAbilities, verified by loading the
 * asset rather than assumed.
 */
inline constexpr const TCHAR* AuthoredCombatDroneDefinition =
	TEXT("/Game/Aurelion/Enemies/NPC_AurelionSecurityDrone.NPC_AurelionSecurityDrone");
}
