"""Read-only: locate the authored GameplayCameras rig the camera arbiter drives.

Nothing is opened for edit and nothing is saved. This exists so the rig's real location is a
looked-up fact rather than an assumed folder - the protagonists use UE GameplayCameras through
Narrative's own camera assets, not UNarrativeCameraComponent.

Python cannot enumerate a Blueprint interface's functions, so this reports where the interface
and enums live and confirms they load; the function signatures have to be read in the editor.
"""
import unreal

TARGETS = ("BPI_GameplayCamera", "E_CameraMode", "E_CameraStyle",
           "CameraAsset_SandboxCharacter", "CameraDirector_SandboxCharacter")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
found = {}
for data in registry.get_all_assets(include_only_on_disk_assets=False):
    name = str(data.asset_name)
    if name in TARGETS:
        found.setdefault(name, []).append((str(data.package_name), str(data.asset_class_path.asset_name)))

for name in TARGETS:
    for package, cls in found.get(name, []):
        asset = unreal.EditorAssetLibrary.load_asset(package)
        unreal.log("RIG %s = %s (%s) loaded=%s" % (name, package, cls, bool(asset)))
    if name not in found:
        unreal.log_warning("RIG %s NOT FOUND" % name)

unreal.log("CAMERA RIG INSPECTION COMPLETE")
