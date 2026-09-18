"""Read-only: what M12's beats tell the player, and what they say about failing.

Nothing is opened for edit and nothing is saved. Written after a play report that the Fire and Frost
phase failed repeatedly with no objective marker and no explanation.
"""
import unreal

MISSION = "/Game/Aurelion/Data/DA_M12_FireAndFrost"

mission = unreal.EditorAssetLibrary.load_asset(MISSION)
if not mission:
    unreal.log_error("MISSING %s" % MISSION)
    raise SystemExit(1)

beats = mission.get_editor_property("beats") or []
unreal.log("Mission %s has %d beats" % (mission.get_editor_property("mission_id"), len(beats)))
for index, beat in enumerate(beats):
    def get(name, default=""):
        try:
            return beat.get_editor_property(name)
        except Exception:
            return default
    beat_id = get("beat_id")
    objective = get("objective_text")
    reason = get("failure_reason_id")
    rule = get("failure_rule_text")
    encounter = get("required_encounter_id")
    unreal.log("")
    unreal.log("[%d] %s" % (index, beat_id))
    unreal.log("    objective      : %s" % (str(objective) if objective else "<EMPTY - no objective shown>"))
    unreal.log("    optional       : %s" % get("b_optional"))
    unreal.log("    encounter      : %s" % (encounter if encounter else "<none>"))
    unreal.log("    failure reason : %s" % (reason if reason and str(reason) != "None" else "<none>"))
    rule_text = str(rule) if rule else ""
    unreal.log("    failure rule   : %s" % (rule_text if rule_text else "<EMPTY - nothing tells the player how they can lose>"))

unreal.log("")
unreal.log("M12 BEAT INSPECTION COMPLETE")
