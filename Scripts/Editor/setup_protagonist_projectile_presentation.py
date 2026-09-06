"""Add visible project-owned projectile presentation without gameplay graphs.

Run after setup_protagonist_native_kits.py inside Unreal Editor.
Existing Tarrik mesh/FX defaults are copied property-by-property onto fresh native
children; Selene gets readable cyan prototype meshes. Native lifecycle, collision,
targeting, costs, authority, and damage remain inherited without alteration.
"""
import json
import os
from pathlib import Path
import unreal

OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "WorkPCSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUT = OUTPUT_DIR / "projectile-presentation-setup.json"
ROOT = "/Game/Abilities/Presentation/"
TARRIK = "/NarrativePro/Pro/Core/Abilities/GameplayAbilities/Tarrik/"
report = {"created": [], "saved": [], "projectiles": {}, "ability_bindings": [],
          "warnings": [], "status": "preflight"}
owned = set()


def path(obj):
    return obj.get_path_name() if obj else None


def asset(name):
    result = unreal.load_asset(name)
    if not result:
        raise RuntimeError("Required asset missing: " + name)
    return result


def remember(obj):
    if not path(obj).startswith("/Game/Abilities/"):
        raise RuntimeError("Presentation may only save project ability assets")
    owned.add(path(obj))
    return obj


def save(obj, blueprint=False):
    if path(obj) not in owned:
        raise RuntimeError("Unowned save rejected: " + path(obj))
    if blueprint:
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError("Failed to save " + path(obj))
    report["saved"].append(path(obj))


def defaults(bp):
    cls = bp.generated_class()
    if not cls:
        raise RuntimeError("Missing generated class: " + path(bp))
    return unreal.get_default_object(cls)


def child(native, name):
    destination = ROOT + name
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        bp = asset(destination)
        parent = str(unreal.EditorAssetLibrary.find_asset_data(destination).get_tag_value("ParentClass"))
        expected = path(native.static_class())
        if parent not in (expected, "/Script/CoreUObject.Class'" + expected + "'"):
            raise RuntimeError("Expected direct-native projectile child: " + destination)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", native)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT.rstrip("/"), unreal.Blueprint, factory)
        if not bp:
            raise RuntimeError("Could not create " + destination)
        report["created"].append(destination)
    remember(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not isinstance(defaults(bp), native):
        raise RuntimeError("Invalid projectile parent " + destination)
    return bp


def copy_mesh(source, target):
    for prop in ("static_mesh", "relative_location", "relative_rotation", "relative_scale3d", "override_materials"):
        target.set_editor_property(prop, source.get_editor_property(prop))
    target.set_editor_property("hidden_in_game", False)
    target.set_editor_property("visible", True)
    target.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)


try:
    source_grenade = defaults(asset(TARRIK + "BP_CinderStickyGrenadeProjectile"))
    source_hunger = defaults(asset(TARRIK + "BP_VelkorransHungerProjectile"))
    sphere = asset("/Engine/BasicShapes/Sphere")
    cube = asset("/Engine/BasicShapes/Cube")
    bindings = [
        ("Tarrik", "CinderStickyGrenade", "grenade_class", "CinderGrenade"),
        ("Tarrik", "VelkorransHunger", "projectile_class", "VelkorransHunger"),
        ("Selene", "StillpointGrenade", "grenade_class", "Stillpoint"),
        ("Selene", "VeritysWake", "wave_class", "VeritysWake"),
        ("Selene", "Dispatch", "returning_verity_class", "Dispatch"),
    ]
    ability_assets = {suffix: remember(asset("/Game/Abilities/" + hero + "/GA_" + hero + "_" + suffix))
                      for hero, suffix, _, _ in bindings}
    report["status"] = "authoring cosmetic defaults"

    material_path = ROOT + "M_EchoCryo"
    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        cryo = asset(material_path)
    else:
        cryo = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_EchoCryo", ROOT.rstrip("/"), unreal.Material, unreal.MaterialFactoryNew())
        if not cryo:
            raise RuntimeError("Could not create cyan projectile material")
        report["created"].append(material_path)
        cryo.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        cryo.set_editor_property("two_sided", True)
        color = unreal.MaterialEditingLibrary.create_material_expression(cryo, unreal.MaterialExpressionConstant3Vector, -250, 0)
        color.set_editor_property("constant", unreal.LinearColor(0.02, 1.8, 3.0, 1.0))
        if not unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError("Could not connect cyan emissive material")
        unreal.MaterialEditingLibrary.recompile_material(cryo)
    remember(cryo)
    save(cryo)

    projectile_assets = {}
    for key, native, source, component, props in [
        ("CinderGrenade", unreal.SovCinderStickyGrenadeProjectile, source_grenade, "grenade_mesh",
         ["explosion_niagara_system", "explosion_niagara_scale", "explosion_decal_material", "explosion_decal_size",
          "explosion_decal_surface_search_distance", "explosion_decal_surface_offset", "explosion_decal_visible_duration", "explosion_decal_fade_duration"]),
        ("VelkorransHunger", unreal.SovVelkorransHungerProjectile, source_hunger, "projectile_mesh",
         ["impact_niagara_system", "impact_niagara_scale", "dissipation_niagara_system", "dissipation_niagara_scale"]),
    ]:
        bp = child(native, "BP_" + key + "Projectile")
        cdo = defaults(bp)
        copy_mesh(source.get_editor_property(component), cdo.get_editor_property(component))
        for prop in props:
            cdo.set_editor_property(prop, source.get_editor_property(prop))
        mesh = cdo.get_editor_property(component).get_editor_property("static_mesh")
        if not mesh:
            raise RuntimeError("Tarrik authored projectile mesh was empty: " + key)
        save(bp, blueprint=True)
        projectile_assets[key] = bp
        report["projectiles"][key] = {"asset": path(bp), "mesh": path(mesh), "presentation_template": path(source),
                                      "copied_cosmetic_fields": props, "native_parent": path(native.static_class())}

    # Readable temporary forms: a small grenade, flat advancing wave and long
    # returning blade. These transform only a NoCollision presentation mesh;
    # the native swept payload retains all of its existing world-unit tuning.
    for key, mesh, scale in [
        ("Stillpoint", sphere, unreal.Vector(0.22, 0.22, 0.22)),
        ("VeritysWake", sphere, unreal.Vector(1.8, 3.0, 0.12)),
        ("Dispatch", cube, unreal.Vector(1.3, 0.08, 0.025)),
    ]:
        bp = child(unreal.SovSeleneCombatProjectile, "BP_" + key + "Projectile")
        comp = defaults(bp).get_editor_property("presentation_mesh")
        comp.set_editor_property("static_mesh", mesh)
        comp.set_editor_property("relative_scale3d", scale)
        comp.set_editor_property("override_materials", [cryo])
        comp.set_editor_property("hidden_in_game", False)
        comp.set_editor_property("visible", True)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        save(bp, blueprint=True)
        projectile_assets[key] = bp
        report["projectiles"][key] = {"asset": path(bp), "mesh": path(mesh), "material": path(cryo),
                                      "scale": [scale.x, scale.y, scale.z], "native_parent": path(unreal.SovSeleneCombatProjectile.static_class()),
                                      "art_status": "visible prototype; native gameplay unaffected"}

    for hero, suffix, prop, key in bindings:
        bp = ability_assets[suffix]
        defaults(bp).set_editor_property(prop, projectile_assets[key].generated_class())
        save(bp, blueprint=True)
        observed = defaults(bp).get_editor_property(prop)
        if path(observed) != path(projectile_assets[key].generated_class()):
            raise RuntimeError("Projectile binding failed: " + suffix)
        report["ability_bindings"].append({"ability": path(bp), "property": prop, "projectile": path(observed)})

    report["warnings"].append("Selene's three meshes are visible prototype forms, not final Verity weapon art or authored field VFX.")
    report["warnings"].append("EchoBase does not automatically play an animation for AbilityAnimSetTag. Pure native ability children have no montage event graphs; montage choreography still needs a cosmetic hook. Primary weapon combo animations and native Deflection weapon montage remain on existing visuals.")
    report["status"] = "authored; verify visibility and effects in PIE"
except Exception as exc:
    report["error"] = str(exc)
    report["status"] = "stopped; inspect partial assets before rerun"
    raise
finally:
    report["package_manifest"] = sorted({name.split(".")[0] for name in report["created"] + report["saved"]})
    OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("VELKORRAN_PROJECTILE_PRESENTATION_REPORT " + str(OUT))
