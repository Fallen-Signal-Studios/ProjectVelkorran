"""One inert, read-only owned HUD sample. No timers, callbacks, input or asset loads.

Call sample() after an ordinary handoff/weapon change. Returns JSON primitives.
The caller may retry `not_ready` for at most one second after normal readiness;
a persistent mismatch is a failed handoff qualification. `failed` is structural.
Text observations are widget-bound text, not a screenshot or pixel-paint proof.
"""
import json
import re
import unreal

HUD_CLASS = "/Game/Aurelion/UI/WBP_AurelionGameplayHUD.WBP_AurelionGameplayHUD_C"
WEAPON_CLASS = "/Game/Aurelion/UI/WBP_WeaponInfo.WBP_WeaponInfo_C"
WORLDS = {"/Game/Aurelion/Maps/L_Aurelion_M12", "/Game/Aurelion/Maps/L_Aurelion_M13"}
TEXT_NAMES = ("TextBlock_WeaponName", "TextBlock_AmmoInClip", "TextBlock_SpareAmmo",
              "TextBlock_AmmoInClip_Alt", "TextBlock_SpareAmmo_Alt")


def _path(obj):
    return obj.get_path_name() if obj is not None else None


def _valid(obj):
    return obj is not None and unreal.SystemLibrary.is_valid(obj)


def _within(path, parent):
    return bool(parent and (path.startswith(parent + ".") or path.startswith(parent + ":")))


def _inventory(pawn):
    return {"mainhand": _path(pawn.get_weapon(True)),
            "offhand": _path(pawn.get_weapon(False)),
            "wielded": sorted(_path(item) for item in pawn.get_wielded_weapons())}


def _weapon(item):
    if item is None:
        return None
    assert _valid(item), "Native getter returned an invalid wielded weapon"
    ammo_class = item.get_editor_property("required_ammo")
    # Same class-cast branch and flags as the original GetWeaponDisplayName graph.
    show_ammo = bool(ammo_class and unreal.MathLibrary.class_is_child_of(ammo_class, unreal.EquippableItem))
    return {"path": _path(item), "required_ammo": _path(ammo_class),
            "ammo_in_clip": int(item.get_ammo_in_clip()), "spare_ammo": int(item.get_spare_ammo()),
            "display_name": str(item.get_weapon_display_name(False, show_ammo)),
            "single_weapon_display_name": str(item.get_weapon_display_name(True, show_ammo))}


def _check(row, name, observed, expected):
    matches = observed == expected
    row["checks"][name] = {"observed": observed, "expected": expected, "matches": matches}
    if not matches:
        row["not_ready_reasons"].append(name)


def _capture(row):
    # All Unreal references live in this stack only. Nothing is stored on a module or callback.
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if not _valid(world):
        row["not_ready_reasons"].append("No current PIE world")
        return
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Requires existing PIE"
    package = re.sub(r"UEDPIE_\d+_", "", world.get_path_name().split(".")[0])
    assert package in WORLDS, "Refusing unrelated world: " + package
    row.update(world=_path(world), package=package, world_seconds=float(unreal.GameplayStatics.get_time_seconds(world)))
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if not _valid(pc) or not _valid(pawn):
        row["not_ready_reasons"].append("Current player is not installed")
        return
    assert isinstance(pc, unreal.SovPlayerController) and isinstance(pawn, unreal.NarrativeCharacter)
    row.update(controller=_path(pc), pawn=_path(pawn), transition=str(pc.get_campaign_transition_state()))
    asc = pawn.get_narrative_ability_system_component()
    if (pc.get_campaign_transition_state() != unreal.SovCampaignTransitionState.IDLE
            or not pawn.is_character_ready() or not pawn.is_alive() or pc.get_narrative_character() != pawn
            or not _valid(asc) or asc.get_avatar_owner() != pawn):
        row["not_ready_reasons"].append("Current living pawn/ASC/controller is not ready and Idle")
        return
    initial_inventory = _inventory(pawn)
    main = _weapon(pawn.get_weapon(True))
    off = _weapon(pawn.get_weapon(False))
    row.update(native_inventory=initial_inventory, native_mainhand=main, native_offhand=off)
    hud = pc.get_narrative_gameplay_hud()
    if not _valid(hud):
        row["not_ready_reasons"].append("Gameplay HUD is not installed")
        return
    assert _path(hud.get_class()) == HUD_CLASS, "Current gameplay HUD is not the exact Aurelion copy"
    assert hud.get_world() == world and hud.get_owning_player() == pc, "Gameplay HUD has another world/player"
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False)
    children = [widget for widget in widgets if _valid(widget) and widget.get_world() == world
                and _within(_path(widget), _path(hud)) and "WBP_WeaponInfo" in widget.get_class().get_name()]
    if not children:
        row["not_ready_reasons"].append("Weapon child is not constructed")
        return
    assert len(children) == 1, "Ambiguous weapon children beneath the current HUD"
    child = children[0]
    assert _path(child.get_class()) == WEAPON_CLASS, "Current HUD still contains an unowned weapon child"
    assert child.get_owning_player() == pc, "Weapon child has another owning player"
    row.update(hud=_path(hud), hud_class=_path(hud.get_class()), weapon_widget=_path(child),
               weapon_widget_class=_path(child.get_class()), visible=bool(hud.is_visible() and child.is_visible()))
    if not row["visible"]:
        row["not_ready_reasons"].append("HUD/weapon child is not visible")
    _check(row, "owner", _path(child.get_editor_property("Owner")), _path(pawn))
    _check(row, "mainhand", _path(child.get_editor_property("MainhandWeapon")), initial_inventory["mainhand"])
    _check(row, "offhand", _path(child.get_editor_property("OffhandWeapon")), initial_inventory["offhand"])
    _check(row, "wielded", sorted(_path(item) for item in child.get_editor_property("OurWeapons")), initial_inventory["wielded"])
    _check(row, "dual_wielding", bool(child.get_editor_property("DualWielding")), len(initial_inventory["wielded"]) > 1)
    vitals = [widget for widget in widgets if _valid(widget) and widget.get_world() == world
              and widget.get_owning_player() == pc and isinstance(widget, unreal.SovCombatVitalsWidget)]
    assert len(vitals) <= 1, "Ambiguous current-player native vitals widgets"
    child_text = {}
    shield_text = None
    for text in unreal.ObjectIterator(unreal.TextBlock):
        path = _path(text)
        if _within(path, _path(child)) and text.get_name() in TEXT_NAMES:
            name = text.get_name()
            assert name not in child_text, "Duplicate expected weapon text: " + name
            child_text[name] = {"path": path, "text": str(text.get_text()), "visibility": str(text.get_visibility())}
        if vitals and _within(path, _path(vitals[0])) and text.get_name() == "TextBlock_1":
            assert shield_text is None, "Ambiguous native Shield label"
            shield_text = {"path": path, "text": str(text.get_text()), "visibility": str(text.get_visibility())}
    row["bound_weapon_text"] = child_text
    expected_name = (main["single_weapon_display_name"] if main else "")
    if len(initial_inventory["wielded"]) > 1:
        expected_name = (main["display_name"] if main else "") + " | " + (off["display_name"] if off else "")
    if "TextBlock_WeaponName" in child_text:
        _check(row, "weapon_name_text", child_text["TextBlock_WeaponName"]["text"], expected_name)
    else:
        row["not_ready_reasons"].append("Weapon name TextBlock unavailable")
    for item, suffix in ((main, ""), (off, "_Alt")):
        if item is None or not item["required_ammo"]:
            continue  # The original graph has distinct non-ammunition/absent-item display branches.
        for label, key in (("TextBlock_AmmoInClip", "ammo_in_clip"), ("TextBlock_SpareAmmo", "spare_ammo")):
            name = label + suffix
            if name not in child_text:
                row["not_ready_reasons"].append(name + " unavailable")
                continue
            expected = str(unreal.TextLibrary.conv_int_to_text(item[key], False, True, 1, 324))
            _check(row, name, child_text[name]["text"], expected)
    shield = pawn.get_component_by_class(unreal.SovShieldComponent)
    row["shield"] = {"component": _path(shield), "bound_text": shield_text,
                     "qualification": "unavailable"}
    if _valid(shield) and shield.is_initialized():
        values = [float(shield.get_shield()), float(shield.get_max_shield())]
        row["shield"].update(current=values[0], maximum=values[1])
        surfaces = [widget for widget in unreal.ObjectIterator(unreal.SovHolographicHUDSurface)
                    if widget.get_world() == world and widget.is_in_viewport()
                    and widget.get_owning_player() == pc]
        assert len(surfaces) <= 1, "Ambiguous current-player holographic HUD surfaces"
        if surfaces:
            surface = surfaces[0]
            view = surface.get_holographic_hud_view()
            bar = surface.get_editor_property('ShieldBar')
            assert isinstance(bar, unreal.ProgressBar), "Holographic ShieldBar is missing"
            expected = max(0., min(1., values[0]/values[1])) if values[1] > 0 else 0.
            displayed = float(bar.get_editor_property('percent'))
            agrees = (view.valid and abs(view.shield.current-values[0]) < .01
                      and abs(view.shield.maximum-values[1]) < .01
                      and abs(view.shield.fraction-expected) < .001
                      and abs(displayed-expected) < .001
                      and bar.get_visibility() not in (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN))
            row['shield'].update(surface=_path(surface), bar=_path(bar), displayed_fraction=displayed,
                                 expected_fraction=expected,
                                 qualification='fraction_matches' if agrees else 'mismatch')
            if not agrees:
                row['not_ready_reasons'].append('Holographic ShieldBar differs from current pawn')
        elif shield_text:
            # Native widget formats floats with zero fractional digits. Accept only that rounded value.
            parts = shield_text["text"].rsplit("/", 1)
            numbers = [re.findall(r"\d[\d\s,.\u00a0\u202f]*", part) for part in parts]
            if len(numbers) == 2 and all(group for group in numbers):
                parsed = [int("".join(c for c in group[-1] if c.isdecimal())) for group in numbers]
                agrees = all(abs(number - value) <= 0.50001 for number, value in zip(parsed, values))
                row["shield"].update(qualification="rounded_value_matches" if agrees else "mismatch", displayed_values=parsed)
                if not agrees:
                    row["not_ready_reasons"].append("Native Shield bound text differs from current pawn")
            else:
                row["shield"]["qualification"] = "text_format_unavailable"
    # Outward pure Blueprint text getters must not conceal a context change during this sample.
    if (editor.get_game_world() != world or unreal.GameplayStatics.get_player_controller(world, 0) != pc
            or unreal.GameplayStatics.get_player_pawn(world, 0) != pawn or pc.get_narrative_character() != pawn
            or pc.get_narrative_gameplay_hud() != hud or _inventory(pawn) != initial_inventory
            or _weapon(pawn.get_weapon(True)) != main or _weapon(pawn.get_weapon(False)) != off
            or not pawn.is_character_ready() or asc.get_avatar_owner() != pawn):
        row["not_ready_reasons"].append("Live context or ammunition changed during sample")


def sample():
    """Return a single primitive-only observation; never retains the live world."""
    row = {"status": "failed", "read_only": True, "checks": {}, "not_ready_reasons": [],
           "errors": [], "callbacks_installed": False, "pixel_paint_qualified": False}
    try:
        _capture(row)
        row["status"] = "not_ready" if row["not_ready_reasons"] else "passed"
    except Exception as exc:
        row["errors"].append(str(exc))
    return json.loads(json.dumps(row, allow_nan=False))
