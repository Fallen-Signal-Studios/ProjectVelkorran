"""Save the reviewed broad 300-lumen ceiling fill and capture the saved result."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);Z03_WIDE_FILL=True
code=(root/'Scripts/Editor/preview_z03_ceiling_light.py').read_text().split("code=(root/'Scripts/Editor/review_eclipse_wall_scars.py')")[0]
exec(compile(code,'create_reviewed_z03_fill','exec'),globals())
for c in fills:c.set_intensity(300)
result=runpy.run_path(str(root/'Scripts/Editor/check_z03_ceiling_light.py'))['check_z03_ceiling_light'](actors)
shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(out/'z03-ceiling-light-fit.json').write_text(json.dumps(dict(status='saved',settings=result),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=capture.split('views=[',1)[0];suffix=capture.split('state=dict',1)[1]
views="views=[('approach',(6800,-19300,165),(0,90),90),('middle',(6800,-17200,165),(15,90),90)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z03_saved_fill_review','exec'),globals())
