#!/usr/bin/env python3
"""Compile and run production-used portable C++ policies; this does not build Unreal."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=shutil.which("g++") or shutil.which("clang++"))
    args = parser.parse_args()
    if not args.compiler:
        parser.error("Install g++ or clang++, or supply --compiler")
    root = Path(__file__).resolve().parents[1]
    sources = sorted((root / "Scripts/Tests").glob("*.cpp")) + sorted((root / "Tests/Portable").glob("*.cpp"))
    if not sources:
        raise SystemExit("No portable test sources found")
    with tempfile.TemporaryDirectory(prefix="velkorran-native-") as temporary:
        for index, source in enumerate(sources):
            executable = Path(temporary) / f"policy-{index}"
            command = [args.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                       "-fsanitize=undefined", "-fno-sanitize-recover=all",
                       "-I", str(root / "Source/ProjectVelkorran/Private"),
                       "-I", str(root / "Source/ProjectVelkorran/Public"),
                       "-I", str(root / "Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public"),
                       str(source), "-o", str(executable)]
            print(f"Compile and run: {source.relative_to(root)}", flush=True)
            subprocess.run(command, check=True)
            subprocess.run([str(executable)], check=True)
    print(f"PASS: {len(sources)} portable suites. Unreal build/automation not run.")


if __name__ == "__main__":
    main()
