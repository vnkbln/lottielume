"""Create reviewed local release artifacts from a clean commit; never publish."""
import argparse
import hashlib
import json
import struct
import subprocess
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def zip_files(target, files):
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for source, name in sorted(files, key=lambda item: item[1]):
            archive.write(source, name)
    with zipfile.ZipFile(target) as archive:
        assert archive.testzip() is None, target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', default='build')
    args = parser.parse_args()
    status = subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=ROOT, text=True)
    if status.strip():
        raise SystemExit('Commit reviewed changes before packaging; source tree must be clean.')
    meta = json.loads((ROOT / 'release.json').read_text(encoding='utf-8'))
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    build = (ROOT / args.build_dir).resolve()
    cmake_cache = (build / 'CMakeCache.txt').read_text(encoding='utf-8')
    cmake = next(line.split('=', 1)[1] for line in cmake_cache.splitlines()
                 if line.startswith('CMAKE_COMMAND:INTERNAL='))
    ctest = Path(cmake).with_name('ctest.exe')
    listing = subprocess.check_output([str(ctest), '--test-dir', str(build), '-C', 'Release', '-N'], text=True)
    assert 'Total Tests: 14' in listing, listing
    run(ctest, '--test-dir', build, '-C', 'Release', '--output-on-failure')
    for script, extra in [('scripts/embed-demo.py', ['--check']),
                          ('tests/thumbnails.py', ['--build-dir', str(build)]), ('scripts/validate-pack.py', [])]:
        run(sys.executable, script, *extra)
    dlls = [build / 'Release' / name for name in ('LottiePlayer.dll', 'LottieEmitter.dll')]
    demo = (ROOT / 'assets/demo/LottieLogo.json').read_bytes()
    for dll in dlls:
        data = dll.read_bytes()
        assert data[:2] == b'MZ' and demo in data, dll
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        assert data[pe:pe+4] == b'PE\0\0' and struct.unpack_from('<H', data, pe+4)[0] == 0x8664
    output = ROOT / 'output/release' / meta['version']
    output.mkdir(parents=True, exist_ok=True)
    plugin = output / f"Lottie-for-Resolume-{meta['version']}-windows-x64.zip"
    files = [(p, p.name) for p in dlls]
    files.append((ROOT / 'docs/INSTALL.md', 'README.md'))
    for name in ('LICENSE', 'LICENSE-ASSETS', 'ATTRIBUTION.md', 'CHANGELOG.md'):
        files.append((ROOT / name, name))
    files.extend((p, p.relative_to(ROOT).as_posix()) for p in (ROOT / 'third_party').rglob('*') if p.is_file())
    zip_files(plugin, files)
    pack_root = ROOT / 'assets/packs/Centerline_v01'
    pack = output / f"Centerline-130-v{meta['pack_version']}.zip"
    zip_files(pack, [(p, 'Centerline/' + p.relative_to(pack_root).as_posix())
                     for p in pack_root.rglob('*') if p.is_file()])
    source = output / f"Lottie-for-Resolume-{meta['version']}-source.zip"
    run('git', 'archive', '--format=zip', f'--output={source}', 'HEAD')
    with zipfile.ZipFile(source) as archive:
        assert archive.testzip() is None
        forbidden = ('AE/', 'Lottie_Icons/', 'output/', 'deps/', 'build/', '.tools/')
        assert not any(n.startswith(forbidden) for n in archive.namelist())
    manifest = {
        **meta, 'commit': commit, 'created_utc': datetime.now(timezone.utc).isoformat(),
        'validation': {'ctest_release_passed': 14, 'embedded_demo': 'passed',
                       'thumbnails': 'passed', 'centerline': '130 passed',
                       'native_host_release_acceptance': 'pending'},
        'plugins': [{'file': p.name, 'sha256': sha(p), 'bytes': p.stat().st_size}
                    for p in dlls],
        'archives': [{'file': p.name, 'sha256': sha(p), 'bytes': p.stat().st_size}
                     for p in (plugin, pack, source)],
        'demo_sha256': sha(ROOT / 'assets/demo/LottieLogo.json'),
        'thorvg_patch_sha256': sha(ROOT / 'patches/thorvg-fill-effect-slots.patch'),
    }
    manifest_path = output / 'release-manifest.json'
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    sums = [f'{sha(p)}  {p.name}' for p in (plugin, pack, source, manifest_path)]
    (output / 'SHA256SUMS.txt').write_text('\n'.join(sums) + '\n', encoding='utf-8')
    (output / 'release-notes.md').write_text(
        '# Lottie for Resolume 1.0.0-beta.1\n\n'
        'First Windows x64 beta by vnkbln: GPU Player and Emitter, integrated folder '
        'playlists, vector transforms, four-color palette and embedded LottieLogo. '
        'Centerline is a separate pack of 130 animations.\n\n'
        'Install the two DLLs in a Resolume FFGL plugin folder. Player Position '
        'must be switched to Timeline or BPM Sync manually. Requires OpenGL 4.1. '
        'Development host: Arena 7.22. JSON only; .lottie and macOS are not included.\n\n'
        'Code: MIT. Artwork: CC BY 4.0, credit vnkbln. See bundled notices. '
        'Back up presets/compositions from private builds.\n', encoding='utf-8')
    print(f'Prepared local release: {output}\nCommit: {commit}\nNothing published.')


if __name__ == '__main__':
    main()
