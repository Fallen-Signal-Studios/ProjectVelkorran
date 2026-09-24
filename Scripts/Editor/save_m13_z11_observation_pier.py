"""Save only the previously reviewed Z11 pier preview on the same map/source hashes."""
import os
import runpy
from pathlib import Path

root = Path(__file__).resolve().parents[2]
review = root/'Saved/Validation/Aurelion/Z11PierPreview-20260924-054202-d87bda1c/z11-observation-pier-fit.json'
assert review.is_file()
os.environ['SOV_Z11_PIER_SAVE'] = '1'
os.environ['SOV_Z11_PIER_REVIEW'] = str(review)
runpy.run_path(str(root/'Scripts/Editor/fit_m13_z11_observation_pier.py'))
