"""Read-only: check the protagonist Blueprints after ApplyCameraState is wired (handoff task 3).

Nothing is opened for edit and nothing is saved.

Two questions, and the second is the one that matters. Is ApplyCameraState implemented at all - if it
is not, the arbiter resolves correctly and the camera simply never changes, which looks like the
feature not working rather than not being wired. And does anything ELSE in the graph still drive
camera mode or style - because if the old logic stays alongside the new event there are two owners
again, which is the exact defect USovCameraControlComponent was built to remove, and it presents as
an intermittent camera fight rather than as a wiring mistake.

Blueprint graphs cannot be read through the Python API, so the second check reads the asset's export
table via the asset registry's tag data and its package dependencies. That finds a reference to the
camera interface or the camera enums; it cannot tell you where in the graph it is used. Treat a
report of leftover references as "look here", not as a verdict.
"""
import unreal

PROTAGONISTS = [
    "/Game/PlayerCharacters/BP_SovTarrik",
    "/Game/PlayerCharacters/BP_SovSelene",
]
CAMERA_INTERFACE = "BPI_GameplayCamera"
CAMERA_ENUMS = ("E_CameraMode", "E_CameraStyle")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
problems = 0


def implements_apply_camera_state(blueprint):
    """The event exists as a function on the generated class once it is implemented in the graph."""
    generated = blueprint.generated_class()
    if not generated:
        return None
    cdo = unreal.get_default_object(generated)
    # A BlueprintImplementableEvent that the Blueprint overrides becomes callable on the instance.
    return hasattr(cdo, "apply_camera_state") or hasattr(generated, "apply_camera_state")


for path in PROTAGONISTS:
    unreal.log("")
    unreal.log("=== %s ===" % path)
    blueprint = unreal.EditorAssetLibrary.load_asset(path)
    if not blueprint:
        unreal.log_warning("MISSING: %s" % path)
        problems += 1
        continue

    # The component itself is a C++ default subobject, so its absence would be a source problem.
    generated = blueprint.generated_class()
    cdo = unreal.get_default_object(generated) if generated else None
    camera = cdo.get_editor_property("CameraControlComponent") if cdo else None
    unreal.log("  camera arbiter present: %s" % bool(camera))
    if not camera:
        unreal.log_warning("  the pawn has no USovCameraControlComponent; this is a source problem, not content")
        problems += 1

    implemented = implements_apply_camera_state(blueprint)
    if implemented:
        unreal.log("  ApplyCameraState: implemented")
    else:
        unreal.log_warning("  ApplyCameraState: NOT implemented - the arbiter decides, but nothing applies it")
        problems += 1

    dependencies = registry.get_dependencies(
        unreal.Name(path.rsplit("/", 1)[0] + "/" + path.rsplit("/", 1)[-1]),
        unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,
                                              include_soft_package_references=True))
    referenced = [str(d) for d in (dependencies or [])]
    interface_refs = [d for d in referenced if CAMERA_INTERFACE in d]
    enum_refs = [d for d in referenced if any(e in d for e in CAMERA_ENUMS)]

    unreal.log("  references %s: %s" % (CAMERA_INTERFACE, "yes" if interface_refs else "no"))
    unreal.log("  references camera enums: %s" % (", ".join(enum_refs) if enum_refs else "no"))
    if implemented and not interface_refs:
        unreal.log_warning("  ApplyCameraState is implemented but nothing references the camera interface;"
                           " the event may not be reaching the rig")
        problems += 1
    if enum_refs and not implemented:
        unreal.log_warning("  the graph still uses the camera enums while ApplyCameraState is unimplemented,"
                           " so the OLD owner is still driving - this is the two-owner case to resolve")
        problems += 1

unreal.log("")
unreal.log("CAMERA WIRING CHECK COMPLETE: %d thing(s) to look at" % problems)
