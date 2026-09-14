"""Read-only text and editor-guide inventory near the Crucible."""
import unreal

def audit_z08_presentation(actors):
    rows=[]
    for a in actors:
        for c in a.get_components_by_class(unreal.SceneComponent):
            if not isinstance(c,(unreal.TextRenderComponent,unreal.BillboardComponent,unreal.ArrowComponent,unreal.SplineComponent,unreal.WidgetComponent)):continue
            p=c.get_world_location()
            if not (-5000<p.x<5000 and 18000<p.y<23500 and -1600<p.z<1600):continue
            properties={}
            for key in ('visible','hidden_in_game','is_editor_only','text','text_render_color','world_size','sprite','draw_debug','arrow_color'):
                try:properties[key]=str(c.get_editor_property(key))
                except Exception:pass
            rows.append(dict(actor=a.get_actor_label(),component=c.get_path_name(),class_name=c.get_class().get_name(),location=[p.x,p.y,p.z],properties=properties))
    return dict(scope='Candidates near the room; stopped-editor visibility does not establish live-game visibility or causality.',components=rows)
