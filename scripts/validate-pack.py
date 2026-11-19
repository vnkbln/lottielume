"""Validate the committed Centerline structure without generated frame caches."""
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / 'assets/packs/Centerline_v01'


def validate():
    manifest = json.loads((PACK / 'manifest.json').read_text(encoding='utf-8'))
    entries = manifest['files']
    expected = {'Shapes': (40, 4), 'One_Shot': (60, 2), 'BG': (30, 8)}
    actual = {p.relative_to(PACK).as_posix() for role in expected
              for p in (PACK / role).rglob('*.json')}
    listed = [e['file'] for e in entries]
    assert len(listed) == len(set(listed)) == 130, 'Duplicate/missing entries'
    assert set(listed) == actual, 'Manifest differs from animation files'
    folders = Counter(str(Path(p).parent) for p in actual)
    assert len(folders) == 13 and set(folders.values()) == {10}, folders
    roles = Counter(e['role'] for e in entries)
    for role, (count, duration) in expected.items():
        assert roles[role] == count, roles
        for entry in (e for e in entries if e['role'] == role):
            data = json.loads((PACK / entry['file']).read_text(encoding='utf-8'))
            assert data['w'] == data['h'] == 1000, entry['file']
            assert data['fr'] == 60, entry['file']
            assert abs((data['op'] - data['ip']) / data['fr'] - duration) < 1e-6
            assert entry['duration'] == duration
            assert not data.get('assets'), entry['file']
            for layer in data['layers']:
                assert not layer.get('masksProperties') and not layer.get('ef'), entry['file']
    print('Centerline: 130 JSON, 13 folders x 10, canvas/timing/manifest OK')


if __name__ == '__main__':
    validate()
