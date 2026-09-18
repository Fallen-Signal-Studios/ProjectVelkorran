"""Read-only: which melee abilities are actually granted, the native-framework ones or the legacy ones.

Nothing is opened for edit and nothing is saved.

Both sets exist. PC2-01 only bites if the abilities the weapons grant are the legacy ones, so this
asks the asset registry who references each. Referencing a Blueprint asset IS a package dependency,
unlike deriving from a C++ class, so a dependency search is the right tool here.
"""
import unreal

NATIVE_SET = ("GA_Tarrik_MeleeLight", "GA_Tarrik_MeleeHeavy", "GA_Selene_MeleeLight", "GA_Selene_MeleeHeavy")
LEGACY_SET = ("GA_Attack_Melee_Sword_1H_Tarrik", "GA_SovVerityTwinAttack")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
all_assets = list(registry.get_all_assets(include_only_on_disk_assets=False))


def referencers(short_name):
    """Who depends on the package whose asset is called short_name."""
    packages = [str(d.package_name) for d in all_assets if str(d.asset_name) == short_name]
    found = []
    for package in packages:
        refs = registry.get_referencers(
            unreal.Name(package),
            unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,
                                                  include_soft_package_references=True,
                                                  include_hard_management_references=True))
        for ref in (refs or []):
            found.append(str(ref))
    return packages, sorted(set(found))


for title, names in (("NATIVE FRAMEWORK ABILITIES", NATIVE_SET), ("LEGACY NARRATIVE-COMBO ABILITIES", LEGACY_SET)):
    unreal.log("")
    unreal.log("=== %s ===" % title)
    for name in names:
        packages, refs = referencers(name)
        if not packages:
            unreal.log_warning("  %s : NOT IN REGISTRY" % name)
            continue
        unreal.log("  %s" % name)
        for package in packages:
            unreal.log("    at %s" % package)
        if refs:
            for ref in refs:
                unreal.log("    referenced by %s" % ref)
        else:
            unreal.log("    referenced by NOTHING - granted by no item or definition")

# The weapon items the audit named, so their grants can be read directly rather than inferred.
unreal.log("")
unreal.log("=== weapon items ===")
for data in all_assets:
    name = str(data.asset_name)
    if name not in ("WI_Velkorran", "WI_Verity"):
        continue
    unreal.log("  %s at %s" % (name, data.package_name))
    deps = registry.get_dependencies(
        unreal.Name(str(data.package_name)),
        unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,
                                              include_soft_package_references=True))
    for dep in sorted(str(d) for d in (deps or [])):
        if "/GA_" in dep or "/DA_" in dep:
            unreal.log("    grants/uses %s" % dep)

unreal.log("")
unreal.log("MELEE GRANT INSPECTION COMPLETE")
