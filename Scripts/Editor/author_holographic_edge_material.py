"""Author the holographic HUD's torn plasma edge material.

The edging was built from jittered polylines. That is deterministic and cheap, but a line cannot
bleed: the fray read as scribble rather than plasma. This builds a UI-domain material that generates
the fray procedurally instead, so the edge can fall off into glow the way the reference art does.

The shape comes from displacing a boundary, not from dimming a gradient. An earlier version computed
opacity as band * noise, which only darkens: the edge stays exactly where the gradient put it and
the band renders as a smooth wash however much noise is mixed in. Here the noise is signed and added
to the band, then cut hard - saturate((band + noise - threshold) * steepness) - so the boundary
itself becomes ragged.

Procedural noise rather than a noise texture, deliberately. Automation here may only load content
whose *transitive* dependencies are all version controlled, and an earlier violation of that rule
caused asynchronous load failures that the test framework attributed to whichever unrelated test was
running. A generated node graph references nothing outside itself, so this asset stays safe to track
and safe to load.

One asset serves every case. EdgeColour and GlowColour are set per protagonist at runtime through
dynamic instances, and FlipV mirrors the gradient so the same material draws the bottom runs as well
as the top ones.

Idempotent: re-running rebuilds the graph in place, so the path and anything referencing it survive.
"""
import json
import os
from pathlib import Path
import unreal

PACKAGE_PATH = '/Game/Aurelion/UI'
ASSET_NAME = 'M_AurelionHolographicEdge'
ASSET_PATH = PACKAGE_PATH + '/' + ASSET_NAME

EDGE_COLOUR = 'EdgeColour'
GLOW_COLOUR = 'GlowColour'
EDGE_INTENSITY = 'EdgeIntensity'
FLIP_V = 'FlipV'

# Where the cut falls across the band, and how sharply. A softer steepness reads as haze rather than
# a torn edge; a harder one reads as a cut-paper silhouette, which is what 7 produced - hard enough
# that the mask was effectively binary and the edge had no falloff at all. The threshold sets how
# much of the band survives: against signed noise of +/-.45, .42 left most of it standing.
THRESHOLD = .62
STEEPNESS = 3.2
# How sharply each run fades in along its own length. A run is drawn as a rectangle and the noise
# frays across the band, never along it, so without this taper the band stops dead at its inner end
# and leaves a straight vertical cut across the screen.
TAPER = 4.5


def expression(material, cls, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)
    assert node, 'Could not create ' + str(cls)
    return node


def connect(source, source_output, target, target_input):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(
        source, source_output, target, target_input), \
        'Could not connect %s -> %s.%s' % (source.get_name(), target.get_name(), target_input or '<first>')


def to_property(source, source_output, prop):
    assert unreal.MaterialEditingLibrary.connect_material_property(source, source_output, prop), \
        'Could not connect %s to %s' % (source.get_name(), str(prop))


def constant(material, value, x, y):
    node = expression(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property('r', value)
    return node


def build():
    library = unreal.EditorAssetLibrary
    if library.does_asset_exist(ASSET_PATH):
        library.delete_asset(ASSET_PATH)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME, PACKAGE_PATH, unreal.Material, unreal.MaterialFactoryNew())
    assert material, 'Material was not created'

    # A Slate brush needs the UI domain. Left at Surface, the emissive and opacity outputs are not
    # the ones Slate reads, and the brush draws wrong or not at all.
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)

    # The band gradient reads unscaled V, so this coordinate stays at 1:1.
    plain = expression(material, unreal.MaterialExpressionTextureCoordinate, -1080, 120)

    across = expression(material, unreal.MaterialExpressionComponentMask, -880, 160)
    across.set_editor_property('r', False)
    across.set_editor_property('g', True)
    across.set_editor_property('b', False)
    across.set_editor_property('a', False)
    connect(plain, '', across, '')

    inverted = expression(material, unreal.MaterialExpressionOneMinus, -700, 120)
    connect(across, '', inverted, '')

    # Which side of the band is the screen edge. 0 keeps the glow at the top, 1 mirrors it to the
    # bottom, so one material draws all four runs.
    flip = expression(material, unreal.MaterialExpressionScalarParameter, -880, 320)
    flip.set_editor_property('parameter_name', FLIP_V)
    flip.set_editor_property('default_value', 0.)

    band = expression(material, unreal.MaterialExpressionLinearInterpolate, -520, 180)
    connect(inverted, '', band, 'A')
    connect(across, '', band, 'B')
    connect(flip, '', band, 'Alpha')

    # A separate, stretched coordinate for the fray. The bands are far wider than they are tall, so
    # square noise cells would be coarse along the run and almost per-pixel across it.
    grain = expression(material, unreal.MaterialExpressionTextureCoordinate, -1080, -260)
    grain.set_editor_property('u_tiling', 5.)
    grain.set_editor_property('v_tiling', 1.6)

    # Noise takes a float3 position. Texture coordinates are a float2, and HLSL will not widen one
    # to resolve the overload: feeding it directly compiled to "no matching function for call to
    # MaterialExpressionNoise", whereupon Slate substituted the default material and the edging drew
    # nothing at all. The material still loaded and still reported its parameters, so only the
    # rendered frame revealed it.
    widened = expression(material, unreal.MaterialExpressionAppendVector, -880, -260)
    connect(grain, '', widened, 'A')
    connect(constant(material, 0., -1080, -140), '', widened, 'B')

    noise = expression(material, unreal.MaterialExpressionNoise, -700, -260)
    # Scale multiplies the incoming position, which the coordinate above has already tiled, so this
    # stays at 1 and the cell shape is controlled in one place.
    noise.set_editor_property('scale', 1.)
    noise.set_editor_property('quality', 3)
    noise.set_editor_property('turbulence', True)
    noise.set_editor_property('levels', 3)
    # Signed, so it displaces the boundary both ways rather than only eroding it. Kept modest: a
    # wider swing pushes torn islands far enough inboard that the edging reads as blotches rather
    # than as a frayed edge.
    noise.set_editor_property('output_min', -.38)
    noise.set_editor_property('output_max', .38)
    noise.set_editor_property('level_scale', 2.2)
    # Tiling stays off. It was enabled to stop the edge crawling between frames, which was the wrong
    # reason: the fray holds still because the position is static UVs, not because it tiles. What
    # tiling actually did was repeat over RepeatSize, which defaults to 512 - so a 0..1 coordinate
    # sampled a fraction of one tile and the noise came out very nearly constant, drawing the band
    # as a flat wash with no fray in it at all.
    noise.set_editor_property('tiling', False)
    # Addressed as the first input rather than by name. Noise overrides GetInputName and returns
    # Position under a world-position label, so no literal 'Position' can match; every other node
    # here resolves by its field name, which is the default behaviour.
    connect(widened, '', noise, '')

    # The torn boundary: displace, then cut.
    perturbed = expression(material, unreal.MaterialExpressionAdd, -340, -60)
    connect(band, '', perturbed, 'A')
    connect(noise, '', perturbed, 'B')

    shifted = expression(material, unreal.MaterialExpressionSubtract, -160, -60)
    connect(perturbed, '', shifted, 'A')
    connect(constant(material, THRESHOLD, -340, 60), '', shifted, 'B')

    steep = expression(material, unreal.MaterialExpressionMultiply, 20, -60)
    connect(shifted, '', steep, 'A')
    connect(constant(material, STEEPNESS, -160, 60), '', steep, 'B')

    opacity = expression(material, unreal.MaterialExpressionClamp, 200, -60)
    connect(steep, '', opacity, '')

    # Fade each run in along its own length, at both ends. The caller extends the outer ends past
    # the screen so only the inner ones are ever seen fading; symmetry keeps one material serving
    # the left-hand and right-hand runs without a second mirroring parameter.
    along = expression(material, unreal.MaterialExpressionComponentMask, -880, 420)
    along.set_editor_property('r', True)
    along.set_editor_property('g', False)
    along.set_editor_property('b', False)
    along.set_editor_property('a', False)
    connect(plain, '', along, '')

    opening = expression(material, unreal.MaterialExpressionMultiply, -700, 400)
    connect(along, '', opening, 'A')
    connect(constant(material, TAPER, -880, 520), '', opening, 'B')
    opening_cut = expression(material, unreal.MaterialExpressionClamp, -540, 400)
    connect(opening, '', opening_cut, '')

    closing_start = expression(material, unreal.MaterialExpressionOneMinus, -700, 540)
    connect(along, '', closing_start, '')
    closing = expression(material, unreal.MaterialExpressionMultiply, -540, 540)
    connect(closing_start, '', closing, 'A')
    connect(constant(material, TAPER, -700, 660), '', closing, 'B')
    closing_cut = expression(material, unreal.MaterialExpressionClamp, -380, 540)
    connect(closing, '', closing_cut, '')

    window = expression(material, unreal.MaterialExpressionMultiply, -200, 460)
    connect(opening_cut, '', window, 'A')
    connect(closing_cut, '', window, 'B')

    masked = expression(material, unreal.MaterialExpressionMultiply, 380, 120)
    connect(opacity, '', masked, 'A')
    connect(window, '', masked, 'B')

    # Glow further out, bright filament where the edge survives the cut.
    glow = expression(material, unreal.MaterialExpressionVectorParameter, -340, -520)
    glow.set_editor_property('parameter_name', GLOW_COLOUR)
    glow.set_editor_property('default_value', unreal.LinearColor(.95, .2, .05, 1.))

    edge = expression(material, unreal.MaterialExpressionVectorParameter, -340, -400)
    edge.set_editor_property('parameter_name', EDGE_COLOUR)
    edge.set_editor_property('default_value', unreal.LinearColor(1., .62, .22, 1.))

    tint = expression(material, unreal.MaterialExpressionLinearInterpolate, 20, -440)
    connect(glow, '', tint, 'A')
    connect(edge, '', tint, 'B')
    # Ramped on the smooth band, not on the cut mask. Driving the colour from the mask tied it to a
    # value that saturates immediately after the threshold, so every surviving pixel came out at
    # full edge colour and the glow never appeared: torn shapes in one flat tone rather than a
    # filament bleeding outward.
    connect(band, '', tint, 'Alpha')

    intensity = expression(material, unreal.MaterialExpressionScalarParameter, 20, -280)
    intensity.set_editor_property('parameter_name', EDGE_INTENSITY)
    # Kept at unity. Amber (1, .62, .22) multiplied by 1.6 clips to (1, .99, .35), which is yellow -
    # the multiply was quietly destroying the hue it was supposed to brighten.
    intensity.set_editor_property('default_value', 1.)

    emissive = expression(material, unreal.MaterialExpressionMultiply, 260, -400)
    connect(tint, '', emissive, 'A')
    connect(intensity, '', emissive, 'B')

    to_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_property(masked, '', unreal.MaterialProperty.MP_OPACITY)

    unreal.MaterialEditingLibrary.recompile_material(material)
    assert library.save_asset(ASSET_PATH), 'Material did not save'

    parameters = {str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)}
    vectors = {str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(material)}
    # Verified against the saved asset rather than assumed: the runtime sets these by name, and a
    # renamed or missing parameter would fail silently as an untinted edge.
    assert {FLIP_V, EDGE_INTENSITY} <= parameters, parameters
    assert {EDGE_COLOUR, GLOW_COLOUR} <= vectors, vectors
    assert material.get_editor_property('material_domain') == unreal.MaterialDomain.MD_UI

    # Note what this does *not* check: that the shader compiles for Slate. recompile_material
    # reports nothing, and the authoring editor never requests a Slate permutation, so a graph that
    # cannot compile still saves and still reports its parameters here. The capture run carries that
    # check instead, failing on a default-material substitution in the editor log.
    return dict(asset=ASSET_PATH, domain='MD_UI', blend='BLEND_TRANSLUCENT',
                threshold=THRESHOLD, steepness=STEEPNESS,
                scalar_parameters=sorted(parameters), vector_parameters=sorted(vectors))


def run(output_directory=None):
    report = dict(status='running', scope='Holographic HUD torn edge material')
    try:
        report.update(build())
        report['status'] = 'passed'
    except Exception as error:
        report.update(status='failed', error=str(error))
        raise
    finally:
        out = output_directory or os.environ.get('SOV_AURELION_RUN_DIRECTORY')
        if out:
            path = Path(out)
            path.mkdir(parents=True, exist_ok=True)
            (path / 'edge-material.json').write_text(json.dumps(report, indent=2, default=str), encoding='utf8')
        unreal.log('AURELION_EDGE_MATERIAL ' + report['status'] + ' ' + ASSET_PATH)
    return report


if __name__ == '__main__':
    run()
