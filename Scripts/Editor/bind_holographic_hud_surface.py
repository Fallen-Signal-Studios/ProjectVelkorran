"""Point the player controller's frontend at the authored holographic HUD widget (handoff task 1).

This is a default-value set on a C++ default subobject, not Blueprint graph authoring: the frontend
is created in ASovPlayerController's constructor as "NativeFrontend", and the only thing changing is
its HolographicHUDSurfaceClass. Nothing in the event graph is touched.

The surface class is verified to derive from USovHolographicHUDSurface before anything is assigned,
and the asset is read back after saving, because a set that silently did not take looks identical to
one that did until the HUD fails to appear in play.
"""
import unreal

CONTROLLER = "/Game/Framework/BP_SovPlayerController"
SURFACE = "/Game/Aurelion/UI/HUD/WBP_SovHolographicHUD"


def fail(message):
    unreal.log_error("HUD BINDING FAILED: %s" % message)
    raise SystemExit(1)


def resolve_surface_class():
    widget = unreal.EditorAssetLibrary.load_asset(SURFACE)
    if not widget:
        fail("the HUD widget is missing at %s" % SURFACE)
    generated = widget.generated_class()
    if not generated:
        fail("%s has no generated class; it may need compiling" % SURFACE)
    # Python exposes neither is_child_of nor parent traversal on a class object, but it does map the
    # hierarchy onto its own types, so the default object answers the question directly.
    if not isinstance(unreal.get_default_object(generated), unreal.SovHolographicHUDSurface):
        parent = widget.get_editor_property("parent_class")
        fail("%s is parented to %s, not USovHolographicHUDSurface. Reparent it first."
             % (SURFACE, parent.get_name() if parent else "nothing"))
    unreal.log("Surface class resolved: %s" % generated.get_name())
    return generated


def frontend_of(controller_path):
    blueprint = unreal.EditorAssetLibrary.load_asset(controller_path)
    if not blueprint:
        fail("the player controller is missing at %s" % controller_path)
    generated = blueprint.generated_class()
    if not generated:
        fail("%s has no generated class; it may need compiling" % controller_path)
    cdo = unreal.get_default_object(generated)
    try:
        frontend = cdo.get_editor_property("Frontend")
    except Exception as exc:
        fail("the frontend component could not be reached: %s" % exc)
    if not frontend:
        fail("the controller has no frontend component")
    return blueprint, frontend


def describe(frontend):
    try:
        return str(frontend.get_editor_property("HolographicHUDSurfaceClass"))
    except Exception as exc:
        return "<unreadable: %s>" % exc


surface_class = resolve_surface_class()
blueprint, frontend = frontend_of(CONTROLLER)
unreal.log("Before: HolographicHUDSurfaceClass = %s" % describe(frontend))

try:
    frontend.set_editor_property("HolographicHUDSurfaceClass", surface_class)
except Exception as exc:
    fail("the surface class could not be assigned: %s" % exc)

if not unreal.EditorAssetLibrary.save_asset(CONTROLLER, only_if_is_dirty=False):
    fail("the controller could not be saved")

# Read it back from disk rather than trusting the in-memory object we just set.
unreal.EditorAssetLibrary.load_asset(CONTROLLER)
_, reloaded = frontend_of(CONTROLLER)
after = describe(reloaded)
unreal.log("After:  HolographicHUDSurfaceClass = %s" % after)
if "WBP_SovHolographicHUD" not in after:
    fail("the assignment did not survive the save; the controller still reports %s" % after)

unreal.log("HUD BINDING COMPLETE")
