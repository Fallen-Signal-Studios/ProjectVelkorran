#!/usr/bin/env python3
"""Measure hostile attack-goal acquisition in cold-start runs.

WHAT THIS MEASURES, AND WHY IT IS NOT "DID IT STALL"
----------------------------------------------------
An earlier version of this analysis classified a run by its LAST snapshot: zero goals
while still seeing a hostile player was called a stall. That measure is invalid. In
every run sampled so far, 100% of goal drops happen because the player is already
dead - the NPC killed them, dropped the attack goal and correctly returned to spawn.
A terminal snapshot taken after that reads exactly like a stall and is not one.

What actually varies, and what the startup ordering race actually costs, is HOW LONG
the NPC takes to acquire its attack goal. So this reports acquisition latency per
controller, plus the state at each goal drop so a drop can never again be mistaken
for a failure.

Usage:
  python Scripts/Classify-AIStartupRuns.py <diagnostics directory> [<baseline directory>]
"""
import argparse
import json
import statistics
import sys
from pathlib import Path


def controllers(capture):
    per = {}
    for row in capture.get("events", []):
        if row.get("event") == "snapshot":
            per.setdefault(row["context"], []).append(row)
    return per


def gameplay_captures(report):
    return [c for c in report.get("captures", []) if "/Temp/" not in c.get("world", "")]


def analyse(directory):
    runs = sorted(directory.glob("run-*/trace.json"))
    if not runs:
        sys.exit(f"No run-*/trace.json under {directory}")

    per_run = []
    latencies = []
    drops_total = 0
    drops_after_death = 0
    never_acquired = 0

    for path in runs:
        report = json.loads(path.read_text(encoding="utf-8"))
        publication = None
        run_latencies = []
        for capture in gameplay_captures(report):
            for row in capture.get("events", []):
                if publication is None and row.get("event") in (
                        "faction_membership_publication", "player_faction_publication"):
                    publication = row["game_seconds"]
            for snapshots in controllers(capture).values():
                acquired = next((s for s in snapshots if s["data"].get("goal_count", 0) > 0), None)
                if acquired is None:
                    never_acquired += 1
                else:
                    run_latencies.append(acquired["game_seconds"])
                counts = [s["data"].get("goal_count", 0) for s in snapshots]
                for index in range(1, len(snapshots)):
                    if counts[index] == 0 and counts[index - 1] > 0:
                        drops_total += 1
                        players = snapshots[index]["data"].get("players") or [{}]
                        if any(p.get("alive") is False for p in players):
                            drops_after_death += 1
                        break
        latencies.extend(run_latencies)
        per_run.append((path.parent.name[-2:], publication, run_latencies))

    print(f"  {'run':>4}  {'publication':>11}  {'acquisition (game seconds, per controller)':<44}")
    for run, publication, values in per_run:
        shown = ", ".join(f"{v:.3f}" for v in sorted(values)) or "none acquired"
        pub = f"{publication:.3f}" if publication is not None else "-"
        print(f"  {run:>4}  {pub:>11}  {shown:<44}")

    print(f"\n  controllers measured        {len(latencies)}")
    print(f"  never acquired a goal       {never_acquired}")
    if latencies:
        print(f"  acquisition min/median/max  {min(latencies):.4f} / "
              f"{statistics.median(latencies):.4f} / {max(latencies):.4f}")
    print(f"  goal drops                  {drops_total}")
    print(f"  ...with player already dead {drops_after_death}"
          f" ({100.0 * drops_after_death / max(drops_total, 1):.0f}% - correct disengagement)")
    return latencies


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("directory", type=Path)
    parser.add_argument("baseline", type=Path, nargs="?")
    args = parser.parse_args()

    print("== measured ==")
    measured = analyse(args.directory)
    if args.baseline:
        print("\n== baseline ==")
        base = analyse(args.baseline)
        if measured and base:
            print(f"\n  median acquisition: baseline {statistics.median(base):.4f}s -> "
                  f"measured {statistics.median(measured):.4f}s "
                  f"({statistics.median(base) - statistics.median(measured):+.4f}s)")


if __name__ == "__main__":
    main()
