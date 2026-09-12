// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

/**
 * Content paths that automation tests are permitted to depend on.
 *
 * Rule: a test may only load content whose **transitive** dependencies are all version-controlled.
 * The repository tracks project source, `Content/Aurelion/`, and the customised Narrative fork under
 * `Plugins/`. Anything else in `Content/` is local to one machine.
 *
 * "Transitive" is the part that bites. An earlier revision of this header pointed four tests at
 * `NPC_AurelionSecurityDrone` because it is tracked and is real campaign data. It is — but its
 * appearance `CA_AurelionSecurityDrone` targets `SKM_SciFi_Drone_1`, and its ability configuration
 * `AC_NPC_ReformationDrone` targets `GA_DroneGunfire` and `GA_DroneRocketAbility`, all inside the
 * untracked SciFi_Drone_1 marketplace pack. Loading the definition therefore emitted load failures
 * and an `ABP_RefDrone` Blueprint compile error during asynchronous loading, which the automation
 * framework attributed to whichever unrelated test happened to be running — a failure that moved
 * between runs and made the suite untrustworthy.
 *
 * The marketplace dependency itself is real and is deliberately **not** engineered around: the
 * authored SecurityDrone needs that pack to render and to grant its gunfire and rocket abilities. It
 * stays recorded as an external content prerequisite in Scripts/Manifests/ContentPrerequisites.json.
 * What changed is only which subject validation loads.
 */
namespace SovTrackedContentPaths
{
/**
 * Authored campaign Enforcer. Every transitive dependency is tracked: appearance
 * `CA_AurelionEnforcer` (Aurelion mesh, material and retarget ABP, plus the fork's `ABP_Biped` and
 * `SKM_Manny`), and configuration `AC_Enforcer` whose six DefaultAbilities and
 * `GE_DefaultEnforcerAttributes` all resolve from tracked plugin content.
 *
 * Verified by reading the assets' reference tables, not assumed: no reference outside
 * `/Game/Aurelion/` and `/NarrativePro/`.
 */
inline constexpr const TCHAR* AuthoredEnforcerDefinition =
	TEXT("/Game/Aurelion/Enemies/NPC_AurelionEnforcer.NPC_AurelionEnforcer");
}
