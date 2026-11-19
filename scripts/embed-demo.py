"""Keep the DLL demo payload synchronized with its public JSON source."""
from pathlib import Path
import argparse
import json

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
source = (root / 'assets/demo/LottieLogo.json').read_bytes()
document = json.loads(source)
assert (document['w'], document['h']) == (1000, 1000)
target = root / 'src/LottieLogo.json.inc'
expected = '\n'.join(','.join(str(x) for x in source[i:i+32])+','
                     for i in range(0, len(source), 32))
if args.check:
    if target.read_text() != expected:
        raise SystemExit('Embedded demo is stale; run scripts/embed-demo.py')
else:
    target.write_text(expected, encoding='utf-8')
print('Embedded square LottieLogo matches its source')
