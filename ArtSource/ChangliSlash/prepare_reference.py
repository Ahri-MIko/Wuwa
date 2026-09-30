"""Prepare editable first-attack art from exported evidence, without touching UE assets.

Original PNG/geometry bytes are copied unchanged. Shader composition, normalized-age
curve sampling, lifespan and burst settings below are explicit reconstruction choices.
"""
from __future__ import annotations

import hashlib
import argparse
import json
import math
from pathlib import Path
import shutil
import struct
import zlib

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
DIAGNOSTICS = PROJECT / 'Saved/Diagnostics/SlashFx/OriginalImport'
EXPORTS = Path('C:/GamePakExtractor/Output/Exports')
SAMPLES = 24
WHITE = 'T_DefaultColorWhite_D'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def f(value):
    value = float(value)
    if not math.isfinite(value):
        raise ValueError(f'Nonfinite shader value {value}')
    text = format(value, '.9g')
    return text if '.' in text or 'e' in text else text + '.0'


def vec(values):
    return f'float{len(values)}(' + ','.join(f(v) for v in values) + ')'


def resize_samples(rows):
    if not rows:
        raise ValueError('Cannot resample an empty curve')
    result = []
    for i in range(SAMPLES):
        p = i / (SAMPLES - 1) * (len(rows) - 1)
        low = math.floor(p)
        high = min(low + 1, len(rows) - 1)
        result.append([a + (b - a) * (p - low) for a, b in zip(rows[low], rows[high])])
    return result


def curve_hlsl(name, rows):
    rows = resize_samples(rows)
    width = len(rows[0])
    data = ','.join(vec(row) if width > 1 else f(row[0]) for row in rows)
    typ = f'float{width}' if width > 1 else 'float'
    return f'const {typ} {name}Lut[{SAMPLES}] = {{{data}}};\n{typ} {name} = lerp({name}Lut[ci], {name}Lut[ci+1], cf);\n'


def known(material, category, name, default):
    return material['knownEffectiveOverrides'][category].get(name, {}).get('value', default)


def curve(layer, pattern, fallback):
    match = next((c for c in layer['curves'] if pattern in c['semanticBinding'] and c['samples']), None)
    return match['samples'] if match else [fallback]


def uv_hlsl(material, prefix):
    uv = known(material, 'vectors', prefix + '_UV', [1, 1, 0, 0])
    off = known(material, 'vectors', prefix + '_UV_Offset', [0, 0, 0, 0])
    # This wiring is authored reconstruction. Raw vectors remain in Evidence.
    scale = [v if abs(v) > 1e-5 else 1 for v in uv[:2]]
    angle = known(material, 'scalars', prefix + '_Rotation', 0)
    if prefix == 'Dissolve':
        angle = known(material, 'scalars', 'DIssolve_Rotation', angle)
    radians = float(angle) * math.tau
    name = prefix.lower() + 'Uv'
    code = f'float2 {name} = (UV-0.5)*{vec(scale)};\n'
    code += f'{name} = float2({name}.x*{f(math.cos(radians))}-{name}.y*{f(math.sin(radians))}, {name}.x*{f(math.sin(radians))}+{name}.y*{f(math.cos(radians))})+0.5;\n'
    code += f'{name} += {vec(off[:2])} + {vec(uv[2:])}*a*0.15;\n'
    return code


def shader(material, layer, sprite):
    alpha_weight = known(material, 'vectors', 'Base_AlphaSwitch', [0, 0, 0, 1])
    mask_weight = known(material, 'vectors', 'Mask_AlphaSwitch', [1, 0, 0, 0])
    dissolve_weight = known(material, 'vectors', 'Dissolve_ChannelSwitch', [1, 0, 0, 0])
    alpha_mul = known(material, 'scalars', 'Base_Alpha_Multiply', 1)
    noise_strength = known(material, 'scalars', 'Noise_Strength', 0)
    emissive_weight = known(material, 'vectors', 'Base_EmissiveSwitch', [0, 0, 0, 1])
    channel_emissive_blend = known(material, 'scalars', 'Base_bUseChannelAsEmissive', 0)
    emissive_add = known(material, 'scalars', 'Base_Emissive_Add', 0)
    color_rows = curve(layer, '.Color.ColorCurve', [1, 1, 1, 1])
    alpha_rows = curve(layer, '.Scale Alpha.FloatCurve', [1])
    s = '// Reconstructed shader; original texture bytes and curve values, authored wiring.\n'
    s += f'float a=saturate(Age);\nfloat cp=a*{f(SAMPLES-1)};\nint ci=min((int)floor(cp),{SAMPLES-2});\nfloat cf=cp-(float)ci;\n'
    s += curve_hlsl('curveColor', color_rows) + curve_hlsl('curveAlpha', alpha_rows)
    s += ''.join(uv_hlsl(material, p) for p in ('Base', 'Second', 'Noise', 'Mask', 'Dissolve'))
    s += 'float2 noise=(Texture2DSample(TexNoise,TexNoiseSampler,noiseUv).rg-0.5);\n'
    s += f'baseUv += noise*{f(noise_strength)}*0.12;\n'
    s += 'float4 base=Texture2DSample(TexBase,TexBaseSampler,baseUv);\n'
    s += 'float4 second=Texture2DSample(TexSecond,TexSecondSampler,secondUv);\n'
    s += 'float4 mask=Texture2DSample(TexMask,TexMaskSampler,maskUv);\n'
    s += 'float4 dissolve=Texture2DSample(TexDissolve,TexDissolveSampler,dissolveUv+noise*0.04);\n'
    s += f'float silhouette=saturate(dot(base,{vec(alpha_weight)})*{f(alpha_mul)});\n'
    s += f'float maskValue=saturate(dot(mask,{vec(mask_weight)}));\n'
    s += f'float dissolveValue=saturate(dot(dissolve,{vec(dissolve_weight)}));\n'
    s += 'float fadeIn=smoothstep(0.0,0.04,a);\nfloat fadeOut=1.0-smoothstep(0.55,1.0,a);\n'
    s += 'float dissolveCut=smoothstep(0.60,1.0,a);\nfloat breakup=1.0-smoothstep(dissolveValue+0.08,dissolveValue+0.33,dissolveCut);\n'
    s += 'float alpha=saturate(silhouette*maskValue*saturate(VertexAlpha)*max(curveColor.a,0.0)*max(curveAlpha,0.0)*fadeIn*fadeOut*breakup);\n'
    # Actual MI values drive the reconstruction instead of a generic luminance mix.
    # Do not clamp weighted emissive to 1: weights intentionally exceed 1.
    # Sprite blend=1 discards packed magenta RGB entirely; Liang blend=.3 retains
    # a partial source RGB contribution. Missing scalar uses authored fallback 0.
    s += f'float emissiveChannel=max(dot(base,{vec(emissive_weight)}),0.0);\n'
    s += f'float3 sourceColor=lerp(max(base.rgb,0.0),float3(emissiveChannel,emissiveChannel,emissiveChannel),saturate({f(channel_emissive_blend)}));\n'
    s += f'// Base_Emissive_Add={f(emissive_add)} retained as evidence; not applied because original master composition is unavailable.\n'
    s += 'float3 color=sourceColor*max(curveColor.rgb,0.0)*lerp(float3(1.0,1.0,1.0),max(second.rgb,0.0),0.28)*max(Intensity,0.0);\n'
    s += 'return float4(color,alpha);\n'
    return s


def mesh_rotation_wpo(layer, lifetime):
    """Integrate an actual rate LUT; axis/input/units are explicit reconstruction."""
    source = next((c for c in layer['curves']
                   if '.Rotation Rate.FloatCurve' in c['semanticBinding'] and c['samples']), None)
    if not source:
        return '', None
    rates = [row[0] for row in source['samples']]
    if len(rates) < 2:
        return '', None
    # Integral over normalized age, converted to elapsed seconds by lifetime.
    # Rate interpreted as turns/sec is our choice, not recovered compiled units.
    cumulative = [[0.0]]
    step = 1.0 / (len(rates) - 1)
    for left, right in zip(rates, rates[1:]):
        cumulative.append([cumulative[-1][0] + (left + right) * .5 * step])
    code = '// Reconstructed local +Z rotation from original Rotation Rate LUT.\n'
    code += '// Assumptions: input=normalizedAge; rate units=turns/second; pivot=mesh origin.\n'
    code += f'float a=saturate(Age);\nfloat cp=a*{f(SAMPLES-1)};\nint ci=min((int)floor(cp),{SAMPLES-2});\nfloat cf=cp-(float)ci;\n'
    code += curve_hlsl('integratedRate', cumulative)
    code += f'float theta=integratedRate*{f(lifetime * math.tau)};\n'
    code += 'float sn=sin(theta);\nfloat cs=cos(theta);\n'
    code += 'float3 turned=float3(P.x*cs-P.y*sn,P.x*sn+P.y*cs,P.z);\nreturn turned-P;\n'
    evidence = {
        'sourceBinding': source['semanticBinding'], 'sourceBindingLine': source['bindingSourceLine'],
        'rateFirst': rates[0], 'rateLast': rates[-1],
        'integratedNormalizedRate': cumulative[-1][0],
        'reconstructedEndDegrees': cumulative[-1][0] * lifetime * 360,
        'axis': 'local +Z', 'units': 'turns/second reconstruction',
        'input': 'normalizedAge reconstruction', 'pivot': 'mesh local origin',
        'integration': 'trapezoid integration of original samples, then 24-sample interpolation',
    }
    return code, evidence


def make_white(path):
    # Exact generated 2x2 RGBA-white placeholder, not altered source imagery.
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress((b'\x00' + b'\xff' * 8) * 2))
    png += chunk(b'IEND', b'')
    path.write_bytes(png)


def evidence_file(name, fallback):
    persistent = HERE / 'Evidence' / name
    if persistent.exists():
        return persistent
    if fallback.exists():
        shutil.copy2(fallback, persistent)
        return persistent
    raise FileNotFoundError(f'Missing both persistent evidence {persistent} and source {fallback}')


def verified_copy(source, destination, expected_sha, offline):
    if not offline and source.exists():
        source_sha = sha(source)
        if expected_sha and source_sha != expected_sha:
            raise ValueError(f'Source changed from recorded evidence: {source}')
        if not destination.exists() or sha(destination) != source_sha:
            shutil.copy2(source, destination)
        assert sha(destination) == source_sha
        return source_sha
    if destination.exists() and expected_sha and sha(destination) == expected_sha:
        return expected_sha
    raise FileNotFoundError(f'No available source or verified persistent copy: {source} -> {destination}')


def main(offline=False):
    for directory in ('Textures', 'Geometry', 'Evidence'):
        (HERE / directory).mkdir(parents=True, exist_ok=True)
    parameters = read(evidence_file('LayerParameters.json', DIAGNOSTICS / 'LayerParameters.json'))
    texture_manifest = read(evidence_file('TextureManifest.json', DIAGNOSTICS / 'TextureManifest.json'))
    attack_file = evidence_file('AttackFxReference.json', PROJECT / 'Docs/References/SlashFx/AttackFxReference.json')
    attack = next(a for a in read(attack_file)['attacks'] if a['projectAttack'] == 'Attack01')
    for file in ('LayerParameters-Readme.md', 'extract_layer_parameters.py'):
        if not (HERE / 'Evidence' / file).exists() and (DIAGNOSTICS / file).exists():
            shutil.copy2(DIAGNOSTICS / file, HERE / 'Evidence' / file)
    hashes_file = HERE / 'Evidence/PreparedSourceHashes.json'
    hashes = read(hashes_file) if hashes_file.exists() else {'schemaVersion': 1, 'textures': {}, 'geometry': {}}
    textures = []
    for t in texture_manifest:
        dest = HERE / 'Textures' / (t['name'] + '.png')
        source = Path(t['pngPath'])
        substitute = False
        if t['name'] == WHITE and not (not offline and source.exists()):
            make_white(dest)
            substitute = True
        else:
            verified_copy(source, dest, t.get('sha256') or hashes['textures'].get(t['name'], {}).get('sha256'), offline)
        metadata = EXPORTS / (t['packagePath'] + '.json')
        explicit_srgb = hashes['textures'].get(t['name'], {}).get('explicitOriginalSrgb')
        if not offline and metadata.exists():
            data = read(metadata)
            objects = data if isinstance(data, list) else [data]
            source_obj = next((o for o in objects if o.get('Type') == 'Texture2D'), {})
            explicit_srgb = source_obj.get('Properties', {}).get('SRGB')
        # Original values win; otherwise ramps/color art use sRGB, packed masks stay linear.
        srgb = explicit_srgb if explicit_srgb is not None else '/Color/' in t['packagePath']
        textures.append({'name': t['name'], 'file': 'Textures/' + dest.name, 'srgb': bool(srgb),
                         'source': str(source), 'sha256': sha(dest), 'substitute': substitute,
                         'srgbSource': 'explicit original JSON' if explicit_srgb is not None else 'authored reconstruction choice'})
        hashes['textures'][t['name']] = {'sha256': sha(dest), 'explicitOriginalSrgb': explicit_srgb, 'substitute': substitute}
    materials = {m['leaf']: m for m in parameters['materials']}
    meshes = set()
    systems = []
    suffix_systems = {}
    lifetimes = {'Daoguang_BG': .42, 'Daoguang_Liang': .38, 'Fether_B': .54, 'Fether_R': .58}
    intensities = {'Daoguang_BG': .85, 'Daoguang_Liang': 1.25, 'Fether_B': 1.4, 'Fether_R': 1.25}
    for system in parameters['systems']:
        suffix = system['asset'].split('NS_Fx_Changli_R1a01_', 1)[1]
        name = 'NS_Reference_Attack01_' + suffix
        suffix_systems[suffix] = name
        layers = []
        for layer in system['layers']:
            if not layer['enabled']:
                continue
            for renderer in layer.get('renderers', []):
                if renderer is None:
                    continue
                rp = renderer['properties']
                sprite = renderer['type'] == 'NiagaraSpriteRendererProperties'
                matref = rp['Material'] if sprite else rp['OverrideMaterials'][0]['ExplicitMat']
                matpath = matref['ObjectPath'].rsplit('.', 1)[0].replace('Client/Content/', '/Game/')
                material = materials[matpath]
                mesh = '' if sprite else rp['ParticleMesh']['ObjectPath'].rsplit('.', 1)[0].split('/')[-1]
                if mesh:
                    meshes.add(mesh)
                bindings = {key: known(material, 'textures', key, '/Game/' + WHITE).split('/')[-1] for key in ('Base', 'Second', 'Noise', 'Mask', 'Dissolve')}
                pivot = rp.get('PivotOffset', {})
                definition = {
                    'id': 'Reference_Attack01_' + suffix + '_' + layer['handle'], 'name': layer['handle'],
                    'sprite': sprite, 'mesh': mesh,
                    'lifetime': .50 if sprite else lifetimes[layer['handle']], 'count': 5 if sprite else 1,
                    'sortOrder': rp.get('SortOrderHint', 0), 'pivot': [pivot.get(k, 0) for k in ('X', 'Y', 'Z')],
                    'textures': {'Tex' + key: value for key, value in bindings.items()},
                    'intensity': .8 if sprite else intensities[layer['handle']],
                    'shader': shader(material, layer, sprite), 'wpo': '',
                    'sourceMaterial': matpath, 'sourceSystem': system['asset'],
                    'sourceEmitter': layer['instance'],
                    'reconstruction': 'Burst count/lifetime, curve input=normalizedAge, shader wiring, intensity, fade and dissolve are authored reconstruction. Original LUT and texture/channel parameter evidence retained.',
                }
                if sprite:
                    definition.update(spriteSize=[-18, 60], spawnRadius=85, speedMin=280, speedMax=420)
                else:
                    definition['wpo'], rotation = mesh_rotation_wpo(layer, definition['lifetime'])
                    if rotation:
                        definition['rotationReconstruction'] = rotation
                    geometry_file = HERE / 'Geometry' / (mesh + '.json')
                    hashes['geometry'][mesh] = verified_copy(DIAGNOSTICS / 'Geometry' / geometry_file.name, geometry_file, hashes['geometry'].get(mesh), offline)
                    geometry = read(geometry_file)
                    positions = geometry['positions']
                    definition['sourceGeometryBounds'] = {
                        'min': [min(p[axis] for p in positions) for axis in range(3)],
                        'max': [max(p[axis] for p in positions) for axis in range(3)],
                        'source': 'copied original geometry positions, no rescale',
                    }
                    definition['particleScaleEvidence'] = 'Renderer exports ScaleBinding but no decoded particle initialization scale; identity scale is reconstruction. Source notify scale remains in placement. Color Scale Alpha is never geometry scale.'
                layers.append(definition)
        systems.append({'name': name, 'source': system['asset'], 'layers': layers})
    events = [e for e in attack['sourceEvents'] if e['included']]
    first_time = min(e['authoredSeconds'] for e in events)
    placements = []
    for event in events:
        suffix = event['effectAsset'].split('DA_Fx_Group_R1a01_01_', 1)[1].split('.', 1)[0]
        placement = {
            'system': suffix_systems[suffix], 'location': [event['location'][k] for k in ('X', 'Y', 'Z')],
            'rotation': [event['rotation'][k] for k in ('Pitch', 'Yaw', 'Roll')],
            'scale': [event['scale'][k] for k in ('X', 'Y', 'Z')] if event['scale'] else [1, 1, 1],
            'delay': (event['authoredSeconds'] - first_time) * attack['timelineScale'],
            'sourceNotify': event['notifyObject'], 'sourceSeconds': event['authoredSeconds'],
            'sourceSocket': event['socketName'],
            'scaleSource': 'explicit source' if event['scale'] else 'identity reconstruction for omitted source field',
        }
        if suffix == 'N_Dg1':
            source_system = next(s for s in parameters['systems'] if s['asset'].endswith('_N_Dg1'))
            raw_keys = source_system['dataAssetExplicitProperties']['Rotation']['Curve[2]']['EditorCurveData']['Keys']
            scale = attack['timelineScale']
            placement['localYawKeys'] = [
                {'time': k['Time'] * scale, 'value': k['Value'],
                 'arriveTangent': k['ArriveTangent'] / scale,
                 'leaveTangent': k['LeaveTangent'] / scale}
                for k in raw_keys
            ]
            placement['localYawSource'] = 'Original DA Rotation.Curve[2] with timeline-scaled seconds and reciprocal-scaled slopes; mapping axis to local yaw is authored reconstruction.'
        placements.append(placement)
    recipe = {
        'schemaVersion': 1, 'description': 'First attack real source geometry/textures with reconstructed playable Niagara/materials; not original shader bytecode.',
        'textures': textures, 'meshes': sorted(meshes), 'systems': systems, 'placements': placements,
        'notifyAuthoredSeconds': first_time * attack['timelineScale'],
        'reconstructionChoices': [
            'Missing master Material graphs: HLSL topology is reconstructed, while raw MI parameters remain in Evidence/LayerParameters.json.',
            'All LUTs resampled to 24 points and sampled on particle normalizedAge, not a recovered original curve input.',
            'Use dot(textureRGBA, exported channel vector); exact original material combining operation unknown.',
            'Emissive uses exported Base_EmissiveSwitch dot, then lerp(baseRGB, channel.xxx, Base_bUseChannelAsEmissive). Values are original; dot/lerp topology is reconstructed. Missing blend uses authored 0.',
            'Base_Emissive_Add is recorded in shader comments/evidence but intentionally not applied: its original order and whether it affects intensity or source color remain unknown. Do not silently turn it into an exposure gain or constant RGB offset.',
            'UV RG=centered tiling, BA=small age pan, Rotation=turns, zero tiling=1: authored mapping, not recovered original master.',
            'Particle count/lifetime/intensity, fade-in .04 and fade-out .55..1, breakup smoothing, ramp blend .28 are authored.',
            'N_Dg1 whole-system Rotation.Curve[2] maps to local yaw as an explicit reconstruction choice; original times scaled by timelineScale, tangents divided by it. No duplicate yaw in shader.',
            'Only visible mesh layers with exported Rotation Rate curves get WPO rotation (Liang and Fether_R). Original rate LUT is integrated assuming normalizedAge input and turns/sec units, rotating around local +Z and mesh origin; units, axis, pivot are reconstruction. No geometry scaling from Color Scale Alpha.',
            'Sprite count=5 per emitter, size=(-18,60), spawn radius=85, speed=280..420, intensity=.8, lifetime=.50 are deliberate tuning values, not original constants.',
            'No runtime attachment inferred from missing source Attached field. Existing Root socket placement supplies world transform.',
            'Original placement scale omitted fields use explicit identity reconstruction.',
            'Generated 2x2 white placeholder replaces missing T_DefaultColorWhite_D only; 17 source PNGs copied byte-for-byte.',
        ],
    }
    (HERE / 'Attack01.json').write_text(json.dumps(recipe, ensure_ascii=False, indent=2), encoding='utf-8')
    hashes_file.write_text(json.dumps(hashes, ensure_ascii=False, indent=2), encoding='utf-8')
    copied = [t for t in textures if not t['substitute']]
    assert len(copied) == 17 and len(textures) == 18
    assert len(meshes) == 4 and len(systems) == 3 and len(placements) == 4
    assert [len(s['layers']) for s in systems] == [4, 2, 2]
    print(json.dumps({'recipe': str(HERE/'Attack01.json'), 'originalPngsCopied': len(copied), 'whiteSubstitutes': 1, 'meshesCopied': len(meshes), 'systems': len(systems), 'layers': sum(len(s['layers']) for s in systems), 'placements': len(placements)}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--offline', action='store_true', help='Rebuild solely from persistent ArtSource evidence and hash-verified geometry/texture copies.')
    main(offline=parser.parse_args().offline)
