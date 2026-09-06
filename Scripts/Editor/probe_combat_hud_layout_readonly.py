"""Read live HUD geometry after wielding in PIE. Does not author or save assets.

Run through the existing editor session after at least one rendered frame. The
cached geometry describes that frame; it is evidence, not a layout driver.
"""
import json
import os
from pathlib import Path
import unreal

OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "WorkPCSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUT = OUTPUT_DIR / "combat-hud-layout.json"
TARGETS = ("SovCombatVitalsWidget", "WBP_DefaultGameplayHUD", "WBP_WeaponInfo",
           "WBP_PlayerInfo_HUD", "WBP_CrosshairContainer", "WBP_Crosshair_Firearm")


def xy(value):
    return [float(value.x), float(value.y)]


def encode(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "export_text"):
        return value.export_text()
    return str(value)


def safe_call(obj, method, *args):
    try:
        return encode(getattr(obj, method)(*args))
    except Exception as exc:
        return {"unavailable": str(exc)}


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
report = {"method": "read-only cached live widget geometry", "widgets": [], "errors": []}
try:
    if not world:
        raise RuntimeError("Run during actual PIE after a weapon is wielded.")
    report["viewport_pixels"] = xy(unreal.WidgetLayoutLibrary.get_viewport_size(world))
    report["viewport_scale"] = unreal.WidgetLayoutLibrary.get_viewport_scale(world)
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False)
    roots = [w for w in widgets if any(token in w.get_class().get_name() for token in TARGETS)]
    root_paths = [w.get_path_name() for w in roots]
    seen = {w.get_path_name(): w for w in roots}
    # Native WidgetTree and Blueprint subwidgets are UObjects outered to the
    # owning UserWidget, so this also reaches unnamed native vitals children.
    for widget in unreal.ObjectIterator(unreal.Widget):
        path = widget.get_path_name()
        if any(path.startswith(root + ".") or path.startswith(root + ":") for root in root_paths):
            seen[path] = widget
    for path, widget in sorted(seen.items()):
        row = {"path": path, "name": widget.get_name(), "class": widget.get_class().get_path_name(),
               "outer": encode(widget.get_outer()), "parent": safe_call(widget, "get_parent"),
               "visibility": safe_call(widget, "get_visibility"),
               "desired_size": safe_call(widget, "get_desired_size"),
               "render_transform": encode(widget.get_editor_property("RenderTransform")),
               "render_opacity": safe_call(widget, "get_render_opacity")}
        try:
            geometry = widget.get_paint_space_geometry()
            size = unreal.SlateLibrary.get_local_size(geometry)
            row["geometry_source"] = "paint space"
            if size.x <= 0 or size.y <= 0:
                geometry = widget.get_cached_geometry()
                size = unreal.SlateLibrary.get_local_size(geometry)
                row["geometry_source"] = "cached tick space fallback"
            if size.x <= 0 or size.y <= 0:
                row["local_size"] = xy(size)
                raise RuntimeError("No nonzero paint/tick geometry; do not infer viewport position from zero geometry")
            start = unreal.SlateLibrary.local_to_absolute(geometry, unreal.Vector2D(0, 0))
            end = unreal.SlateLibrary.local_to_absolute(geometry, size)
            start_pixel, start_viewport = unreal.SlateLibrary.absolute_to_viewport(world, start)
            end_pixel, end_viewport = unreal.SlateLibrary.absolute_to_viewport(world, end)
            row.update({"local_size": xy(size), "absolute_start": xy(start), "absolute_end": xy(end),
                        "viewport_start": xy(start_viewport), "viewport_end": xy(end_viewport),
                        "pixel_start": xy(start_pixel), "pixel_end": xy(end_pixel)})
            if "Crosshair" in row["class"]:
                row["pixel_center_minus_viewport_center"] = [
                    (start_pixel.x + end_pixel.x - report["viewport_pixels"][0]) / 2,
                    (start_pixel.y + end_pixel.y - report["viewport_pixels"][1]) / 2]
        except Exception as exc:
            row["geometry_error"] = str(exc)
        try:
            slot = widget.get_editor_property("slot")
            row["slot"] = {"class": slot.get_class().get_path_name()} if slot else None
            if slot:
                for getter in ("get_anchors", "get_offsets", "get_alignment", "get_auto_size",
                               "get_padding", "get_horizontal_alignment", "get_vertical_alignment"):
                    if hasattr(slot, getter):
                        row["slot"][getter.removeprefix("get_")] = safe_call(slot, getter)
                for prop in ("Padding", "Size", "HorizontalAlignment", "VerticalAlignment"):
                    try:
                        row["slot"][prop] = encode(slot.get_editor_property(prop))
                    except Exception:
                        pass
        except Exception as exc:
            row["slot_error"] = str(exc)
        if isinstance(widget, unreal.TextBlock):
            row["text"] = str(widget.get_text())
        report["widgets"].append(row)
    report["status"] = "captured"
except Exception as exc:
    report["status"] = "failed"
    report["errors"].append(str(exc))
finally:
    OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("VELKORRAN_COMBAT_HUD_LAYOUT " + str(OUT))
