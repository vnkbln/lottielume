"""Apply the pinned Fill slot patch, accepting only a clean tree or this exact diff."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
repo = root / "deps" / "thorvg"
patch = root / "patches" / "thorvg-fill-effect-slots.patch"
def git(*args):
    return subprocess.check_output(["git", "-C", str(repo), *args])
def normalized(data):
    return data.replace(b"\r\n", b"\n").strip()
changes = git("diff", "HEAD", "--binary")
if git("ls-files", "--others", "--exclude-standard").strip():
    raise SystemExit("Unexpected untracked ThorVG files; refusing to apply Fill patch")
if changes:
    if normalized(changes) != normalized(patch.read_bytes()):
        raise SystemExit("Unexpected ThorVG changes; refusing to apply Fill patch")
else:
    subprocess.run(["git", "-C", str(repo), "apply", "--check", str(patch)], check=True)
    subprocess.run(["git", "-C", str(repo), "apply", str(patch)], check=True)
print("ThorVG Fill effect slot patch verified")
