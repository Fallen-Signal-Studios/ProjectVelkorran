"""Fresh Blender FBX round trip for the two faction shuttle source meshes."""
import bpy
import json
from pathlib import Path

root = Path(__file__).resolve().parent
rows = []
for faction in ("Dominion", "Reformation"):
    name = f"SM_Aurelion_{faction}_Shuttle"
    spec = json.loads((root / f"{name}.json").read_text(encoding="utf8"))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root / f"{name}.fbx"))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    assert len(meshes) == 1, (name, len(meshes))
    obj = meshes[0]
    bpy.context.view_layer.update()
    obj.data.calc_loop_triangles()
    dims = [float(v) for v in obj.dimensions]
    assert all(abs(a - b) < .02 for a, b in zip(dims, spec["dimensions_metres"])), (name, dims)
    slots = {mat.name for mat in obj.data.materials if mat}
    assert slots == set(spec["materials"]), (name, slots)
    assert len(obj.data.uv_layers) >= 2, (name, list(obj.data.uv_layers))
    triangles = len(obj.data.loop_triangles)
    assert triangles >= 30000 and triangles == spec["triangles"], (name, triangles)
    rows.append(dict(asset=name, dimensions_metres=dims, triangles=triangles,
                     material_slots=sorted(slots), uv_channels=len(obj.data.uv_layers)))

(root / "departure-shuttle-verification.json").write_text(
    json.dumps(dict(status="PASS", fresh_fbx_import=True, shuttles=rows), indent=2),
    encoding="utf8")
print("DEPARTURE_SHUTTLE_FBX_ROUNDTRIP_PASS")
