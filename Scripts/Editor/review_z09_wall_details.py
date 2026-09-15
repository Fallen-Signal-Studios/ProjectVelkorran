"""Fresh saved mesh review with no reimports."""
from pathlib import Path
import unreal
SKIP_WALL_DETAIL_IMPORT=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/refine_z09_wall_details.py').read_text(),'z09_wall_detail_review','exec'),globals())
