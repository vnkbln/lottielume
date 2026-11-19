"""Preview/remove an explicit allowlist of reproducible local caches.

Never touches installed plugins, release checkpoints, user projects or originals.
"""
import argparse
import json
import os
import shutil
import zipfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CACHES = ('centerline-preview', 'centerline-expansion-stills', 'centerline-v04-study',
          'pack-study', 'skater-slow-study')
PACKAGES = ('package', 'package-0.3', 'package-0.4', 'package-0.6', 'package-0.7',
            'package-0.7.1', 'package-0.7.2')


def checked(name):
    path = ROOT / 'output' / name
    assert name in CACHES + PACKAGES
    assert path.resolve() == path.absolute() and path.parent.resolve() == ROOT / 'output'
    assert not path.is_symlink() and not path.is_junction()
    for base, dirs, files in os.walk(path, followlinks=False):
        for entry in dirs + files:
            item = Path(base) / entry
            assert not item.is_symlink() and not item.is_junction(), item
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    targets = []
    for name in CACHES + PACKAGES:
        path = checked(name)
        if path.is_dir():
            files = [p for p in path.rglob('*') if p.is_file()]
            targets.append((name, path, files, sum(p.stat().st_size for p in files)))
    for name, _, files, size in targets:
        print(f'{name}: {len(files)} files, {size / 1024**2:.1f} MiB')
    if not args.apply:
        print('Preview only. Use --apply to archive diagnostics and remove these targets.')
        return
    archive_dir = ROOT / 'output/archive'
    archive_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    archive_path = archive_dir / f'cleanup-{stamp}.zip'
    with zipfile.ZipFile(archive_path, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, _, files, _ in targets:
            for file in files:
                # Preserve old packages completely; retain diagnostic reports,
                # not reproducible rendered frames/movies in frame-cache trees.
                if name in PACKAGES or file.suffix.lower() in ('.json', '.txt', '.md', '.csv', '.log', '.py'):
                    archive.write(file, file.relative_to(ROOT / 'output').as_posix())
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
    for name, path, _, _ in targets:
        assert checked(name) == path
        shutil.rmtree(path)
    report = {'archive': archive_path.name,
              'removed': [{'path': str(path), 'bytes': size} for _, path, _, size in targets],
              'reclaimed_bytes': sum(t[3] for t in targets) - archive_path.stat().st_size}
    (archive_dir / f'cleanup-{stamp}.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f"Reclaimed {report['reclaimed_bytes'] / 1024**3:.2f} GiB; archive: {archive_path}")


if __name__ == '__main__':
    main()
