#!/usr/bin/env python3
"""Compile and exercise the actual production math. This is not an Unreal build."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", default=shutil.which("g++") or shutil.which("clang++"))
args = parser.parse_args()
if not args.compiler:
    parser.error("Install g++ or clang++, or supply --compiler")
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="velkorran-axiom-") as temporary:
    executable = Path(temporary) / "axiom-math-tests"
    subprocess.run([
        args.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-fsanitize=undefined", "-fno-sanitize-recover=all",
        "-I", str(root / "Source/ProjectVelkorran/Private"),
        str(root / "Scripts/Tests/AxiomPulseMathTests.cpp"), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
