"""Read engine API metadata for a native-driven receiver material."""
from pathlib import Path
import os,json,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
(out/'status-api.txt').write_text(unreal.MaterialExpressionScalarParameter.__doc__+'\n'+unreal.MaterialEditingLibrary.connect_material_expressions.__doc__)
