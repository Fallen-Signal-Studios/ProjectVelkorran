from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/Editor/audit_z03_lighting.py').read_text().replace('4500<p.x<9500 and -21500<p.y<-13500','2500<p.x<11500 and -14000<p.y<-8000').replace('z03-lights.json','z04-lights.json')
exec(compile(code,'z04_light_inventory','exec'),globals())
