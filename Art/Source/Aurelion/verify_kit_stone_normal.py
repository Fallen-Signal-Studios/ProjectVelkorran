"""Check the baked texture's dimensions, unit normals and periodic edge continuity."""
from pathlib import Path
from PIL import Image
import numpy as np,json
root=Path(__file__).resolve().parent/'StoneNormalKit';a=np.array(Image.open(root/'T_AurelionKit_StoneFine_N.png').convert('RGB'));v=a.astype(float)/127.5-1
assert a.shape==(1024,1024,3) and a[:,:,2].min()>240
assert np.abs(np.linalg.norm(v,axis=2)-1).max()<.02
edge=max(np.abs(a[:,0].astype(int)-a[:,-1].astype(int)).max(),np.abs(a[0].astype(int)-a[-1].astype(int)).max())
assert edge<=3,edge
(root/'texture-verification.json').write_text(json.dumps(dict(status='passed',rgb_min=a.min(axis=(0,1)).tolist(),rgb_max=a.max(axis=(0,1)).tolist(),maximum_edge_difference_8bit=int(edge),qualification='Image dimensions, tangent normal length and periodic seam only; engine material preview pending.'),indent=2))
print('STONE_NORMAL_TEXTURE_PASS')
