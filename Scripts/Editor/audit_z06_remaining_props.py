"""Current read-only inventory for retained breach-room cover and climb visuals."""
from pathlib import Path
import unreal
source=(Path(unreal.Paths.project_dir())/'Scripts/Editor/audit_z06_architecture.py').read_text()
source=source.split('editor.editor_set_game_view(True)',1)[0].replace('assert len(actors)==2509','assert len(actors)==2842')
exec(compile(source,'z06_remaining_prop_inventory','exec'),globals())
