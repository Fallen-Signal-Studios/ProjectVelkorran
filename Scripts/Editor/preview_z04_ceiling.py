from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());code=(root/'Scripts/Editor/preview_z03_ceiling.py').read_text()
code=code.replace('Z03CeilingKit','Z04CeilingKit').replace('PERSIST_Z03_CEILING','PERSIST_Z04_CEILING').replace('check_z03_ceiling','check_z04_ceiling').replace('z03-ceiling-fit.json','z04-ceiling-fit.json')
code=code.replace("result=runpy.run_path", "runpy.run_path(str(root/'Scripts/Editor/fit_z04_coffer_lights.py'))['fit_z04_coffer_lights'](actors)\nresult=runpy.run_path")
code=code.replace("('z03-approach',(6800,-19300,165),(0,90),90),('z03-middle',(6800,-17200,165),(15,90),90),('z03-coffer',(7000,-18000,380),(65,90),80)","('entry',(7000,-12600,165),(5,90),90),('middle',(6900,-11500,180),(10,90),90),('balcony',(9000,-11000,465),(15,180),90),('coffer',(7000,-11000,500),(65,90),85),('frontage',(7000,-13900,200),(12,90),95)")
exec(compile(code,'z04_ceiling_fit','exec'),globals())
