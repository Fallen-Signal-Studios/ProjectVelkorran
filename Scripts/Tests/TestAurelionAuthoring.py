"""Host validation of the Aurelion authoring input boundary; never executes Unreal."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("aurelion_setup", ROOT / "Scripts/Editor/setup_aurelion_slice.py")
SETUP = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SETUP)


def valid_config():
    config = json.loads((ROOT / "Scripts/Editor/Manifests/AurelionPreparation.example.json").read_text(encoding="utf-8"))
    config["player_start_actor"] = "PlayerStart_0"
    for index, row in enumerate(config["participants"]):
        row["actor_name"] = "SovNPC_" + str(index)
        row["npc_definition"] = "/Game/Tests/ND_Participant" + str(index)
    config["placement"]["arrival_terminal"]["location"] = [200, 0, 0]
    config["placement"]["start_volume"]["location"] = [800, 0, 0]
    config["placement"]["start_volume"]["extent"] = [100, 150, 150]
    config["placement"]["secure_terminal"]["location"] = [1800, 0, 0]
    return config


class AurelionAuthoringInputTests(unittest.TestCase):
    def test_example_is_read_only_inventory_usable_but_cannot_author_fake_content(self):
        example = json.loads((ROOT / "Scripts/Editor/Manifests/AurelionPreparation.example.json").read_text())
        self.assertEqual(SETUP.validate_config(example, inventory_only=True), [])
        self.assertTrue(SETUP.validate_config(example))

    def test_valid_explicit_roster_and_positions_accepted_without_engine_claim(self):
        config = valid_config()
        before = copy.deepcopy(config)
        self.assertEqual(SETUP.validate_config(config), [])
        self.assertEqual(config, before)

    def test_empty_roster_and_all_survivor_roster_cannot_complete_encounter(self):
        for rows in ([], valid_config()["participants"][1:]):
            config = valid_config()
            config["participants"] = rows
            errors = SETUP.validate_config(config)
            self.assertTrue(any("actual enemy" in error for error in errors))

    def test_roster_requires_both_survivor_backgrounds_and_unambiguous_roles(self):
        for role, background in (("enemy", "hostile"), ("protected_survivor", "dominion"), ("required", "reformation")):
            config = valid_config()
            config["participants"][2].update(role=role, background=background)
            self.assertTrue(SETUP.validate_config(config))

    def test_stable_names_are_case_insensitively_unique(self):
        for key in ("participant_id", "actor_name"):
            config = valid_config()
            config["participants"][1][key] = config["participants"][0][key].swapcase()
            self.assertTrue(any("duplicated" in error for error in SETUP.validate_config(config)))

    def test_missing_or_misspelled_config_keys_fail_closed(self):
        for location in ("root", "sources", "participant", "placement"):
            config = valid_config()
            target = {"root": config, "sources": config["sources"], "participant": config["participants"][0], "placement": config["placement"]["start_volume"]}[location]
            target["misspelled_setting"] = True
            self.assertTrue(SETUP.validate_config(config), location)

    def test_destination_aliases_traversal_and_object_paths_rejected_as_sources(self):
        paths = [SETUP.OUTPUTS["map"], SETUP.OUTPUTS["mission"].swapcase().replace("/gAME/", "/Game/"),
                 "/Game/Maps/../Source", "/Game/Maps/L_Test.L_Test", "/Narrative/Maps/L_Test", "/Game/Maps/Bad Name"]
        for path in paths:
            config = valid_config()
            config["sources"]["map"] = path
            self.assertTrue(SETUP.validate_config(config, inventory_only=True), path)

    def test_non_finite_invalid_or_missing_placement_values_rejected(self):
        for bad in (None, [0, 0], [0, 0, float("nan")], [0, 0, float("inf")], [0, 0, True], [0, 0, "100"]):
            config = valid_config()
            config["placement"]["arrival_terminal"]["location"] = bad
            self.assertTrue(SETUP.validate_config(config), repr(bad))
        for bad in ([0, 1, 1], [-1, 1, 1]):
            config = valid_config()
            config["placement"]["start_volume"]["extent"] = bad
            self.assertTrue(SETUP.validate_config(config))

    def test_rotated_trigger_rejected_until_separation_check_supports_it(self):
        config = valid_config()
        config["placement"]["start_volume"]["rotation"] = [0, 45, 0]
        self.assertTrue(SETUP.validate_config(config))

    def test_missing_participant_definitions_and_non_identifier_names_rejected(self):
        for key, bad in (("npc_definition", None), ("actor_name", "A descriptive actor label"), ("participant_id", "")):
            config = valid_config()
            config["participants"][0][key] = bad
            self.assertTrue(SETUP.validate_config(config))

    def test_digest_stable_for_json_key_order_and_changes_with_bindings(self):
        config = valid_config()
        reordered = dict(reversed(list(config.items())))
        self.assertEqual(SETUP.config_digest(config), SETUP.config_digest(reordered))
        changed = copy.deepcopy(config)
        changed["participants"][0]["actor_name"] = "AnotherNPC"
        self.assertNotEqual(SETUP.config_digest(config), SETUP.config_digest(changed))

    def test_bad_json_types_fail_cleanly(self):
        for bad in (None, [], False, 1, "config"):
            self.assertTrue(SETUP.validate_config(bad))
        for value in (True, 1.0, "1", None):
            config = valid_config()
            config["schema_version"] = value
            self.assertTrue(SETUP.validate_config(config))


if __name__ == "__main__":
    unittest.main()
