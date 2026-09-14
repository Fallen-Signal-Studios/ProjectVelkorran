"""Cold-session control: render 64 warmup frames before the four-frame sample."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/compare_z08_capture_warmup.py').read_text()
prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('entry-cold-64',64,(0,18900,-1035),(5,90),90),('entry-following-4',4,(0,18900,-1035),(5,90),90),('detail-first-64',64,(-2330,19100,-1040),(-8,165),65),('detail-following-4',4,(-2330,19100,-1040),(-8,165),65)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'capture_reverse','exec'),globals())
