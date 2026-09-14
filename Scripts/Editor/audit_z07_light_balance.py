"""Read-only potential light influence on the capture gallery."""
from pathlib import Path
import unreal
source=Path(unreal.Paths.project_dir())/'Scripts/Editor/audit_z06_light_balance.py'
code=source.read_text().replace('Z06','Z07').replace('z06','z07')
code=code.replace('(-1700,6000,-600),(1700,11600,100)','(-1700,13900,-900),(1700,16100,-300)')
exec(compile(code,'z07_light_survey','exec'),globals())
