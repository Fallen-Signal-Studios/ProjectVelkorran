"""Read-only: which ability Blueprints derive from the native melee framework, and what the
protagonists derive from instead.

Nothing is opened for edit and nothing is saved.

PC2-01 claims the native melee framework is used only by enemy abilities while both protagonists
inherit Narrative's demo combo. That is a claim about parentage, so it is answered from the asset
registry's own ParentClass and NativeParentClass tags.

Note for anyone extending this: searching package dependencies for the class name does NOT work. A
Blueprint deriving from a C++ class depends on /Script/ProjectVelkorran, not on a package named after
the class, so that search returns zero for every native parent and looks like a finding.
"""
import unreal

NATIVE_ABILITY = "SovGameplayAbility_Melee"
NATIVE_DEFINITION = "SovMeleeAttackDefinition"
PROTAGONIST_ABILITIES = ("GA_Attack_Melee_Sword_1H_Tarrik", "GA_SovVerityTwinAttack")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
all_assets = list(registry.get_all_assets(include_only_on_disk_assets=False))
unreal.log("Assets in registry: %d" % len(all_assets))


def tag(data, name):
    value = data.get_tag_value(name)
    return str(value) if value else ""


unreal.log("")
unreal.log("=== Blueprints whose native parent is the melee framework ===")
native_users = []
for data in all_assets:
    parent = tag(data, "NativeParentClass") + " " + tag(data, "ParentClass")
    if NATIVE_ABILITY in parent:
        native_users.append((str(data.package_name), parent.strip()))
for package, parent in sorted(native_users):
    unreal.log("  %s" % package)
unreal.log("  total: %d" % len(native_users))

unreal.log("")
unreal.log("=== assets of the native melee definition type ===")
definitions = [str(d.package_name) for d in all_assets if NATIVE_DEFINITION in str(d.asset_class_path.asset_name)]
for package in sorted(definitions):
    unreal.log("  %s" % package)
unreal.log("  total: %d" % len(definitions))

unreal.log("")
unreal.log("=== the protagonists' melee abilities ===")
found = 0
for data in all_assets:
    if str(data.asset_name) not in PROTAGONIST_ABILITIES:
        continue
    found += 1
    unreal.log("  %s" % data.package_name)
    unreal.log("    ParentClass       : %s" % (tag(data, "ParentClass") or "<none>"))
    unreal.log("    NativeParentClass : %s" % (tag(data, "NativeParentClass") or "<none>"))
    asset = unreal.EditorAssetLibrary.load_asset(str(data.package_name))
    generated = asset.generated_class() if asset else None
    cdo = unreal.get_default_object(generated) if generated else None
    if cdo:
        # isinstance is the only reliable ancestry test Python exposes for a UClass.
        unreal.log("    derives from the native melee ability: %s"
                   % isinstance(cdo, unreal.SovGameplayAbility_Melee))
        unreal.log("    is a Narrative combat ability        : %s"
                   % isinstance(cdo, unreal.NarrativeCombatAbility))
for name in PROTAGONIST_ABILITIES:
    if not any(str(d.asset_name) == name for d in all_assets):
        unreal.log_warning("  %s was not found in the registry at all" % name)

unreal.log("")
unreal.log("MELEE FRAMEWORK INSPECTION COMPLETE")
