"""Read cooked exports into an evidence manifest; never modifies source exports/assets."""
import json
import re
from pathlib import Path

ROOT = Path('C:/GamePakExtractor/Output/Exports')
OUT = Path(__file__).parent
NIAGARA = 'Client/Content/Aki/Effect/Niagara/NI_R2T1ChangliMd10011/'
LEAVES = [
    'MI_MeshParticle/MI_Trans_Daoguang_140109_MP',
    'MI_MeshParticle/MI_Trans_Daoguang_140100_MP',
    'MI_MeshParticle/MI_Trans_Daoguang_140062_MP',
    'MI_MeshParticle/MI_Trans_Daoguang_140055_MP4',
    'MI_Sprite/MI_Trans_Wenli_140031_S',
    'MI_Sprite/MI_Trans_Wenli_140031_S1',
]

def package(ref):
    return re.sub(r'\.\d+$', '', ref)

def game_path(pkg):
    return pkg.replace('Client/Content/', '/Game/').replace('Engine/Content/', '/Engine/')

def load(pkg):
    path = ROOT / (package(pkg) + '.json')
    if not path.exists():
        return path, [], ''
    text = path.read_text(encoding='utf-8-sig')
    objs = json.loads(text)
    return path, objs if isinstance(objs, list) else [objs], text

def line(text, needle, start=0):
    offset = text.find(needle, start)
    return text.count('\n', 0, offset) + 1 if offset >= 0 else None

def values(props, field, source, text):
    result = {}
    for p in props.get(field, []):
        name = p.get('ParameterInfo', {}).get('Name')
        if not name:
            continue
        value = p.get('ParameterValue', p.get('Value'))
        if isinstance(value, dict) and 'R' in value:
            value = [value[k] for k in ('R', 'G', 'B', 'A')]
        elif isinstance(value, dict) and 'ObjectPath' in value:
            value = game_path(package(value['ObjectPath']))
        result[name] = {'value': value, 'source': source, 'line': line(text, '"Name": "' + name + '"')}
        if 'bOverride' in p:
            result[name]['bOverride'] = p['bOverride']
    return result

materials = []
for leaf in LEAVES:
    pkg = 'Client/Content/Aki/Effect/Materials/' + leaf
    chain = []
    visited = set()
    while pkg and pkg not in visited:
        visited.add(pkg)
        path, objs, txt = load(pkg)
        obj = next((o for o in objs if o.get('Type') in ('MaterialInstanceConstant', 'Material')), None)
        if obj is None:
            chain.append({'asset': game_path(pkg), 'source': str(path), 'exists': False})
            break
        p = obj.get('Properties', {})
        chain.append({
            'asset': game_path(pkg), 'source': str(path), 'exists': True, 'type': obj['Type'],
            'expressionObjectCount': sum(o.get('Type', '').startswith('MaterialExpression') for o in objs),
            'parent': game_path(package(p['Parent']['ObjectPath'])) if p.get('Parent') else None,
            'scalars': values(p, 'ScalarParameterValues', str(path), txt),
            'vectors': values(p, 'VectorParameterValues', str(path), txt),
            'textures': values(p, 'TextureParameterValues', str(path), txt),
            'staticSwitches': values(p.get('StaticParameters', {}), 'StaticSwitchParameters', str(path), txt),
            'basePropertyOverrides': p.get('BasePropertyOverrides', {}),
        })
        pkg = package(p.get('Parent', {}).get('ObjectPath', ''))
    merged = {k: {} for k in ('scalars', 'vectors', 'textures', 'staticSwitches')}
    for entry in reversed(chain):
        for k in merged:
            merged[k].update({name: value for name, value in entry.get(k, {}).items() if value.get('bOverride', True)})
    texture_availability = {}
    for name, entry in merged['textures'].items():
        localpkg = entry['value'].replace('/Game/', 'Client/Content/').replace('/Engine/', 'Engine/Content/')
        base = ROOT / localpkg
        texture_availability[name] = [str(f) for f in base.parent.glob(base.name + '.*')]
    materials.append({
        'leaf': game_path('Client/Content/Aki/Effect/Materials/' + leaf),
        'chainLeafToRoot': chain, 'knownEffectiveOverrides': merged,
        'allMasterDefaultsKnown': bool(chain and chain[-1].get('type') == 'Material'),
        'textureAvailability': texture_availability,
        'formulaStatus': 'Unknown: no master Material expression graph exported. Parameter names and numeric values are evidence, not proof of channel/UV/formula semantics.',
    })

systems = []
for suffix in ('N_Dg', 'N_Dg1', 'N1_Dg'):
    pkg = NIAGARA + 'NS_Fx_Changli_R1a01_' + suffix
    path, objs, txt = load(pkg)
    system = next(o for o in objs if o['Type'] == 'NiagaraSystem')
    def resolve(ref):
        return objs[int(ref['ObjectPath'].rsplit('.', 1)[1])] if ref else None
    layers = []
    for handle in system['Properties']['EmitterHandles']:
        emitter = resolve(handle.get('Instance'))
        record = {'handle': handle['Name'], 'enabled': handle.get('bIsEnabled'), 'instance': emitter['Name'] if emitter else None}
        if emitter:
            ep = emitter.get('Properties', {})
            record['explicitEmitterProperties'] = {k: v for k, v in ep.items() if k not in ('UpdateScriptProps', 'SpawnScriptProps', 'RendererProperties', 'GPUComputeScript')}
            renderers = []
            for renderer_ref in ep.get('RendererProperties', []):
                renderer = resolve(renderer_ref)
                if renderer:
                    rp = renderer.get('Properties', {})
                    renderers.append({'type': renderer['Type'], 'properties': rp, 'sourceLine': line(txt, '"ObjectPath": "' + rp.get('ParticleMesh', rp.get('Material', {})).get('ObjectPath', '__missing__') + '"')})
                else:
                    renderers.append(None)
            record['renderers'] = renderers
            curves = []
            script_modules = []
            seen = set()
            for script in objs:
                if script.get('Type') != 'NiagaraScript' or not script.get('Outer', {}).get('ObjectName', '').endswith(':' + emitter['Name'] + "'"):
                    continue
                script_modules.append({'script': script['Name'], 'compiledStatScopeNames': [scope.get('FriendlyName') for scope in script.get('Properties', {}).get('CachedScriptVM', {}).get('StatScopes', [])]})
                for binding in script.get('Properties', {}).get('CachedDefaultDataInterfaces', []):
                    di = resolve(binding.get('DataInterface'))
                    if di is None or di.get('Type') not in ('NiagaraDataInterfaceCurve', 'NiagaraDataInterfaceColorCurve', 'NiagaraDataInterfaceVectorCurve'):
                        continue
                    key = (binding.get('Name'), binding['DataInterface']['ObjectPath'])
                    if key in seen:
                        continue
                    seen.add(key)
                    cp = di.get('Properties', {})
                    width = {'NiagaraDataInterfaceCurve': 1, 'NiagaraDataInterfaceVectorCurve': 3, 'NiagaraDataInterfaceColorCurve': 4}[di['Type']]
                    lut = cp.get('ShaderLUT', [])
                    samples = [lut[i:i+width] for i in range(0, len(lut), width)]
                    extrema = [{'min': min(row[i] for row in samples), 'max': max(row[i] for row in samples)} for i in range(width)] if samples else []
                    semantic = binding['Name']
                    curves.append({
                        'semanticBinding': semantic, 'type': di['Type'], 'dataInterfaceObjectPath': binding['DataInterface']['ObjectPath'],
                        'dataInterfaceOuter': di.get('Outer', {}).get('ObjectName'),
                        'bindingSourceLine': line(txt, '"Name": "' + semantic + '"'),
                        'exportedDomain': {k: cp[k] for k in ('LUTMinTime', 'LUTMaxTime', 'LUTInvTimeRange', 'LUTNumSamplesMinusOne') if k in cp},
                        'sampleCount': len(samples), 'components': width,
                        'first': samples[0] if samples else None, 'last': samples[-1] if samples else None,
                        'componentRanges': extrema, 'samples': samples,
                        'samplingInputKnown': False,
                        'samplingWarning': 'LUT domain is exported; input could be emitter time or particle age/index. Missing fields are not claimed defaults; do not assume normalized age or seconds solely from binding name.',
                    })
            record['curves'] = curves
            record['compiledScriptScopes'] = script_modules
        layers.append(record)
    da_path, da_objs, _ = load('Client/Content/Aki/Effect/DataAsset/Niagara/R2T1ChangliMd10011/DA_Fx_R1a01_01_' + suffix)
    systems.append({'asset': game_path(pkg), 'source': str(path), 'dataAssetSource': str(da_path),
                    'dataAssetExplicitProperties': da_objs[0].get('Properties', {}) if da_objs else {}, 'layers': layers})

manifest = {
    'schemaVersion': 1, 'sourceExportRoot': str(ROOT),
    'scope': 'First attack actual three NS systems and six active renderer leaf materials. Facts and reconstruction choices are separated.',
    'materials': materials, 'systems': systems,
    'reconstructionChoicesNotOriginalFacts': [
        'Material graph absent: base/second/mask/noise/dissolve ordering and parameter-channel math must be reconstructed, not declared exact.',
        'Uniform LUT samples are exact exported data; curve sampling inputs and compiled script arithmetic are not recovered.',
        'Particle spawn count, burst timing and final lifetime require authored reconstruction unless decoded and independently verified.',
        'Do not use the zero-renderer BG/Liang simulations in N1_Dg as additional visible meshes.',
        'BasePropertyOverrides contains engine defaults too: only bOverride flags establish MI overrides.',
        'Do not substitute all Base_AlphaSwitch with texture alpha; exported channel vectors differ per layer.',
        'Scale Alpha curve belongs to color/alpha processing; it is not evidence for changing mesh spatial scale. Mesh size/orientation initialization is not decoded.',
    ],
}
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'LayerParameters.json').write_text(json.dumps(manifest, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
print(json.dumps({'output': str(OUT/'LayerParameters.json'), 'materials': len(materials), 'systems': len(systems), 'curves': sum(len(l.get('curves', [])) for s in systems for l in s['layers']), 'bytes': (OUT/'LayerParameters.json').stat().st_size}, ensure_ascii=False))
