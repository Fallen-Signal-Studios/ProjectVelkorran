"""Save the successful transient cargo experiment as isolated review assets."""
from pathlib import Path
import unreal
SAVE_CHAOS_PROTOTYPE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/probe_aurelion_chaos_authoring.py').read_text(),
             'probe_aurelion_chaos_authoring','exec'),globals())
