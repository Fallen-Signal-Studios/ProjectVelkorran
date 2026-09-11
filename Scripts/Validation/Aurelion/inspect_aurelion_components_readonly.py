"""Bounded read-only actual Aurelion PIE component census. No asset loads or writes."""
import collections
import json
from pathlib import Path
import unreal

ROLES = ("SecurityDrone", "ContaminatedDrone", "Enforcer", "Linkbound", "WallRunner", "Weaver", "Elite")
FAMILIES = ("SovWeakPointComponent", "SovDismembermentComponent", "SovStatusComponent",
            "NarrativeAbilitySystemComponent", "SovShieldComponent", "SovPoiseComponent")


def run(output_directory):
    out = Path(output_directory)
    out.mkdir(parents=True, exist_ok=True)
    report = {"read_only": True, "status": "failed", "roles": {role: [] for role in ROLES}}
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert world and "/Aurelion/Maps/UEDPIE_" in world.get_path_name(), "Requires retained Aurelion PIE"
        report["world"] = world.get_path_name()
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase):
            class_path = actor.get_class().get_path_name()
            matches = [role for role in ROLES if class_path ==
                       "/Game/Aurelion/Enemies/BP_Aurelion" + role + ".BP_Aurelion" + role + "_C"]
            if not matches:
                continue
            components = list(actor.get_components_by_class(unreal.ActorComponent))
            row = {"actor": actor.get_path_name(), "class": class_path,
                   "components": [{"name": c.get_name(), "class": c.get_class().get_path_name()}
                                  for c in components],
                   "creation_method_observable": False,
                   "class_counts": dict(sorted(collections.Counter(c.get_class().get_path_name() for c in components).items())),
                   "families": {name: [c.get_name() for c in components if isinstance(c, getattr(unreal, name))]
                                for name in FAMILIES}}
            if isinstance(actor, unreal.SovAurelionElite):
                core = actor.get_core_weak_points()
                row["native_core"] = core.get_path_name() if core else None
                row["native_core_valid"] = bool(core and core.has_valid_weak_point_configuration())
            report["roles"][matches[0]].append(row)
        report["actors"] = sum(len(rows) for rows in report["roles"].values())
        report["missing_roles"] = [role for role, rows in report["roles"].items() if not rows]
        report["status"] = "passed: read-only census; missing roles are reported, never spawned"
    except Exception as exc:
        report["error"] = str(exc)
        raise
    finally:
        (out / "aurelion-component-census.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report
