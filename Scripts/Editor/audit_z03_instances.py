from pathlib import Path
import unreal
Z03_INVENTORY_ONLY=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/audit_z03_architecture.py').read_text(),'z03_complete_instances','exec'),globals())
