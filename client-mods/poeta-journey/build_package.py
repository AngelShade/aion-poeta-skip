"""Prepare Poeta Journey for a clean matching client; never install client files."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

from codec import read_pak, encode_pak, binary_xml
from patch_game_dll import build_dll, ORIGINAL_SHA256
from patch_plugin_key import patch_plugin_key
from PIL import Image


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-path', type=Path, required=True,
                        help='Aion game root containing bin64, Data, L10N and Plugin; not bin64 itself')
    parser.add_argument('--java', type=Path, required=True, help='Full path to JDK 25 bin/java.exe')
    parser.add_argument('--output', type=Path, required=True, help='New staging folder outside the client')
    args = parser.parse_args()
    root, output = args.client_path.resolve(), args.output.resolve()
    required = ['bin64/game.dll', 'bin64/crysystem.dll', 'bin64/Awesomium.dll',
                'bin32/bin32.pak', 'bin32/bin32.pak.sig', 'Data/func_pet/func_pet.pak',
                'Data/func_pet/func_pet.pak.sig', 'Plugin/RelicCalc/RelicCalc.pak',
                'Plugin/RelicCalc/RelicCalc.pak.sig', 'Data/Items/Items.pak',
                'L10N/enu/data/data.pak', 'Data/ui/ui.pak', 'Pub.key']
    missing = [name for name in required if not (root / name).is_file()]
    if missing:
        raise ValueError('Choose the Aion GAME ROOT containing bin64, Data, L10N and Plugin.\n'
                         'For D:\\Games\\Aion 4.8 NA\\bin64\\game.dll, enter D:\\Games\\Aion 4.8 NA.\n'
                         'Missing files:\n  ' + '\n  '.join(missing))
    if digest(root / 'bin64/game.dll') != ORIGINAL_SHA256:
        raise ValueError('Use the supported clean English 4.8 NA 64-bit client. '
                         'This standalone builder refuses an already customized Game.dll.')
    if output == root or root in output.parents or output in root.parents or output.exists():
        raise ValueError('Choose a NEW output folder outside the client; do not use its parent folder.')
    if not args.java.is_file():
        raise ValueError('--java must name your JDK 25 bin/java.exe file.')
    media_output = output.parent / (output.name + '-server-media')
    if media_output.exists(): raise ValueError('Use a new output name; generated server media already exists.')
    for name in ('loading_lf1.dds','loading_lc1.dds','loading_df1.dds','loading_dc1.dds'):
        if not (root/'Textures/loading'/name).is_file(): raise ValueError('Original loading artwork is missing: '+name)
    mod = Path(__file__).resolve().parent
    settings = json.loads((mod / 'menus.json').read_text(encoding='utf-8-sig'))
    if settings != {'label': 'Choose Your Journey', 'icon': 'v5_start_menu_relic_up',
                    'url': 'http://127.0.0.1:8091/journey'}:
        raise ValueError('Use the supplied loopback settings. Remote hosting needs coordinated native/server changes.')
    with read_pak(root / 'Data/ui/ui.pak') as archive:
        payload = archive.read('UI_Preload.xml')
        tree = binary_xml(payload) if payload[0] == 128 else ET.fromstring(payload)
        if not any(skin.get('name') == settings['icon'] for skin in tree.iter('Skin')):
            raise ValueError('The client is missing the native Poeta Journey menu skin.')
    source = root / 'Plugin/RelicCalc/RelicCalc.pak'
    with read_pak(source) as archive:
        if len(set(archive.namelist())) != len(archive.namelist()):
            raise ValueError('Duplicate addon archive entries are unsupported.')
        content = {name: archive.read(name) for name in archive.namelist()}
    anchor = b'\tRegisterMenu(GetAionStr("STR_RELICCALC_TITLE"), lastCommand, "v5_start_menu_relic_up");'
    if content['RelicCalc.lua'].count(anchor) != 1 or 'PrivateMenus.lua' in content:
        raise ValueError('Use the original unmodified RelicCalc addon.')
    content['RelicCalc.lua'] = content['RelicCalc.lua'].replace(anchor, b'\tPrivateMenus_Register();\r\n' + anchor)
    content['Journey.xml'] = (mod / 'Journey.xml').read_text(encoding='utf-8-sig').replace('UTF-8', 'UTF-16').replace('\n', '\r\n').encode('utf-16')
    ET.fromstring(content['Journey.xml'])
    content['PrivateMenus.lua'] = (mod / 'PrivateMenus.lua').read_text().replace('\n','\r\n').encode()
    toc = content['RelicCalc.toc'].decode().replace('\r', '').splitlines()
    content['RelicCalc.toc'] = ('\r\n'.join(toc + ['Journey.xml', 'PrivateMenus.lua']) + '\r\n').encode()
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, payload in content.items():
            archive.writestr(name, payload)
    addon = output / 'Plugin/RelicCalc/RelicCalc.pak'
    addon.parent.mkdir(parents=True)
    addon.write_bytes(encode_pak(buffer.getvalue()))
    with read_pak(addon) as archive:
        assert archive.testzip() is None and archive.namelist() == list(content)
        for name, payload in content.items():
            assert archive.read(name) == payload, name
    subprocess.run([str(args.java), str(mod / 'SignClientPackages.java'), str(root), str(output)], check=True)
    dll = build_dll(root / 'bin64/game.dll', [], [settings['url']])
    (output / 'bin64').mkdir(exist_ok=True)
    (output / 'bin64/crysystem.dll').write_bytes(patch_plugin_key((root / 'bin64/crysystem.dll').read_bytes()))
    sys.path.insert(0, str(mod.parent / 'native-icon-bridge'))
    from build_bridge import build
    from patch_client import patch_dll
    native = build(root, output)
    native['originalArchiveSha256'] = digest(root / 'Data/Items/Items.pak')
    (output / 'bin64/game.dll').write_bytes(patch_dll(dll))
    files = [{'path': p.relative_to(output).as_posix(),
              'original': digest(root / p.relative_to(output)) if (root / p.relative_to(output)).exists() else None,
              'staged': digest(p)} for p in sorted(output.rglob('*')) if p.is_file()]
    manifest = {'clientRoot': str(root), 'files': files, 'legacyAddon': [], 'retiredFiles': [],
                'inventorySlots': 0, 'signatureIsolation': 'archive-v2', 'nativeIcons': native,
                'poetaJourney': True}
    manifest['preservedFiles'] = [dict(path=p, sha256=digest(root/p)) for p in ['Data/Items/Items.pak','Data/ui/game/game.pak','L10N/enu/data/data.pak']]
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    media_output.mkdir()
    for source,name in [('loading_lf1.dds','poeta.jpg'),('loading_lc1.dds','sanctum.jpg'),('loading_df1.dds','ishalgen.jpg'),('loading_dc1.dds','pandaemonium.jpg')]:
        with Image.open(root/'Textures/loading'/source) as img:
            img.convert('RGB').save(media_output/name,quality=92)
    print('Copy generated artwork to active server config/journey/media:',media_output)
    print(f'Prepared {len(files)} verified Poeta Journey files in {output}. Client unchanged.')


if __name__ == '__main__':
    main()
