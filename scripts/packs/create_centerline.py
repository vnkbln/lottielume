"""Author the first Centerline pack. Geometry/keyframes are editable Lottie, no effects.

Run from any directory with Python 3; source JSONs are written deterministically.
"""
from pathlib import Path
import json
import math

ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / "assets/packs/Centerline_v01"
COLORS = ["f318e8", "9c32fe", "277ff4", "00f3e9", "ffffff", "000000"]
MANIFEST = []
_catalog_path = PACK / "manifest.json"
_english_descriptions = {
    item["file"]: item["description"]
    for item in (json.loads(_catalog_path.read_text(encoding="utf-8"))["files"]
                 if _catalog_path.exists() else [])
    if not any('\u0400' <= ch <= '\u04ff' for ch in item.get("description", ""))
}


def rgb(index):
    h = COLORS[index]
    return [int(h[i:i+2], 16) / 255 for i in (0, 2, 4)] + [1]


def static(v):
    return {"a": 0, "k": v}


def animated(points, linear=False):
    # Collapse coincident boundary keys (e.g. an undelayed first wave).
    points = sorted(dict(points).items())
    keys = []
    for n, (time, value) in enumerate(points):
        value = value if isinstance(value, list) else [value]
        key = {"t": round(time * 60, 5), "s": value}
        if n + 1 < len(points):
            key.update({"o": {"x": [0.0 if linear else .33], "y": [0]},
                        "i": {"x": [1.0 if linear else .67], "y": [1]}})
        keys.append(key)
    return {"a": 1, "k": keys}


def scale(points):
    return animated([(t, [v, v, 100]) for t, v in points])


def ellipse(radius):
    return {"ty": "el", "nm": "Circle", "d": 1,
            "p": static([0, 0]), "s": static([radius * 2, radius * 2])}


def polygon(sides, radius, phase=-90):
    pts = [[round(radius * math.cos(math.radians(phase + k * 360 / sides)), 4),
            round(radius * math.sin(math.radians(phase + k * 360 / sides)), 4)]
           for k in range(sides)]
    return path(pts, True)


def rectangle(half, radius=0):
    return {"ty": "rc", "nm": "Frame", "d": 1, "p": static([0, 0]),
            "s": static([half * 2, half * 2]), "r": static(radius)}


def path(vertices, closed=False):
    return {"ty": "sh", "nm": "Path", "ks": static({
        "i": [[0, 0] for _ in vertices], "o": [[0, 0] for _ in vertices],
        "v": vertices, "c": closed})}


def geometry(kind, radius):
    if kind == "circle":
        return ellipse(radius)
    if kind == "square":
        return rectangle(radius / math.sqrt(2), 2)
    if kind == "triangle":
        return polygon(3, radius)
    if kind == "hex":
        return polygon(6, radius, -30)
    if kind == "cross":
        r = radius
        # One continuous open contour, intentionally no filled center.
        return path([[-r, 0], [r, 0], [0, 0], [0, -r], [0, r]])
    raise ValueError(kind)


def stroke(color, width):
    return {"ty": "st", "nm": "Palette stroke", "c": static(rgb(color)),
            "o": static(100), "w": static(width), "lc": 2, "lj": 2, "ml": 4}


def trim(start=0, end=100, offset=0):
    return {"ty": "tm", "nm": "Draw contour", "s": static(start),
            "e": end if isinstance(end, dict) else static(end),
            "o": static(offset), "m": 1}


def layer(name, form, duration, color=3, width=4, opacity=100,
          position=None, size=None, rotation=None, draw=None, fill=False):
    shapes = [form]
    shapes.append({"ty": "fl", "nm": "Small accent", "c": static(rgb(color)),
                   "o": static(100), "r": 1} if fill else stroke(color, width))
    if draw is not None:
        shapes.append(draw)
    return {"ddd": 0, "ind": 0, "ty": 4, "nm": name, "sr": 1,
            "ks": {"o": opacity if isinstance(opacity, dict) else static(opacity),
                   "r": rotation or static(0),
                   "p": position or static([500, 500, 0]), "a": static([0, 0, 0]),
                   "s": size or static([100, 100, 100])},
            "ao": 0, "shapes": shapes, "ip": 0, "op": duration * 60, "st": 0, "bm": 0}


def orbit(radius, phase, duration=4, swing=22):
    pts = []
    for t, delta in [(0, 0), (duration / 2, swing), (duration, 0)]:
        a = math.radians(phase + delta)
        pts.append((t, [500 + radius * math.cos(a), 500 + radius * math.sin(a), 0]))
    return animated(pts)


def emit(category, family, name, duration, layers, description, colors):
    for index, item in enumerate(layers, 1):
        item["ind"] = index
    document = {"v": "5.12.2", "fr": 60, "ip": 0, "op": duration * 60,
                "w": 1000, "h": 1000, "nm": name, "ddd": 0,
                "assets": [], "layers": layers, "markers": []}
    relative = f"{category}/{family}/{name}.json"
    file = PACK / relative
    file.parent.mkdir(parents=True, exist_ok=True)
    file.write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
    MANIFEST.append({"file": relative, "role": category, "duration": duration,
                     "layers": len(layers), "colors": ["#" + COLORS[i] for i in colors],
                     "description": _english_descriptions.get(relative, name.replace("_", " "))})


def central_shapes():
    d = 4
    # Halo: changing gaps, orbiting dashes and a stable central silhouette.
    layers = [layer("Core", ellipse(144), d, 3, 4.5,
                    size=scale([(0, 92), (2, 108), (4, 92)])),
              layer("Counter arc", ellipse(202), d, 3, 3,
                    rotation=animated([(0, -35), (2, 25), (4, -35)]),
                    draw=trim(0, 64, 30)),
              layer("White arc", ellipse(178), d, 4, 2.5, 75,
                    rotation=animated([(0, 70), (2, -20), (4, 70)]),
                    draw=trim(0, 18))]
    for a in (0, 90, 180, 270):
        layers.append(layer(f"Orbit marker {a}", path([[-10, 0], [10, 0]]), d, 3, 5,
                            position=orbit(240, a), rotation=static(a + 90)))
    emit("Shapes", "Circles", "Halo", d, layers,
         "Цельный круг, встречные дуги и четыре орбитальных штриха. Для мягкого скретча.", [3, 4])

    layers = [layer("Primary frame", rectangle(151, 5), d, 1, 4.5,
                    rotation=animated([(0, -9), (2, 9), (4, -9)]),
                    size=scale([(0, 100), (2, 91), (4, 100)])),
              layer("Inset corners", rectangle(117, 3), d, 4, 2.5, 80,
                    rotation=animated([(0, 9), (2, -9), (4, 9)]),
                    draw=trim(0, 72, 42))]
    for a in (45, 135, 225, 315):
        layers.append(layer(f"Satellite {a}", rectangle(8, 1), d, 1, 3,
                            position=orbit(250, a, swing=-14), rotation=static(45)))
    emit("Shapes", "Squares", "Floating_Frame", d, layers,
         "Два слегка расходящихся квадратных контура и четыре небольших спутника.", [1, 4])

    layers = [layer("Triangle", polygon(3, 224), d, 0, 4.5,
                    size=scale([(0, 94), (2, 104), (4, 94)])),
              layer("Inner draw", polygon(3, 157), d, 0, 2.5, 70,
                    rotation=animated([(0, 0), (2, 18), (4, 0)]),
                    draw=trim(0, animated([(0, 28), (2, 76), (4, 28)]))),
              layer("Small white center", ellipse(5), d, 4, fill=True)]
    for a in (-90, 30, 150):
        layers.append(layer(f"Vertex signal {a}", path([[-8, 0], [8, 0]]), d, 4, 3, 75,
                            position=orbit(252, a, swing=10), rotation=static(a + 90)))
    emit("Shapes", "Triangles", "Prism_Signal", d, layers,
         "Треугольный знак с внутренним рисующимся контуром; три нейтральных акцента.", [0, 4])

    layers = [layer("Hex shell", polygon(6, 215, -30), d, 2, 4,
                    rotation=animated([(0, 0), (2, 12), (4, 0)])),
              layer("Hex inner", polygon(6, 146, -30), d, 3, 2.5, 78,
                    size=scale([(0, 100), (2, 84), (4, 100)]),
                    rotation=animated([(0, 12), (2, 0), (4, 12)]))]
    for a in range(0, 360, 60):
        layers.append(layer(f"Spoke {a}", path([[176, 0], [192, 0]]), d, 2, 3, 65,
                            rotation=static(a), size=scale([(0, 100), (2, 108), (4, 100)])))
    emit("Shapes", "Hexagons", "Hex_Relay", d, layers,
         "Шестигранная оболочка, спокойное встречное движение и шесть коротких рёбер.", [2, 3])


def echo_shot(family, name, kind, color, count, twist, description):
    layers = []
    for n in range(count):
        delay = .12 * n
        end = min(1.98, delay + 1.25)
        layers.append(layer(f"Wave {n+1}", geometry(kind, 100), 2, color, 1.8 if n else 2.4,
                            opacity=animated([(0, 0), (delay, 0), (delay + .10, 85 - n * 6),
                                              (end - .32, 50 - n * 4), (end, 0), (2, 0)]),
                            size=scale([(0, 8), (delay, 8), (delay + .30, 120), (end, 820), (2, 820)]),
                            rotation=animated([(0, 0), (delay, 0), (end, twist), (2, twist)])))
    emit("One_Shot", family, name, 2, layers, description, [color])


def fly_shot(family, name, kind, color, count, description):
    layers = []
    for n in range(count):
        a = math.radians(-90 + n * 360 / count)
        delay = (n % 3) * .045
        # Acceleration creates flight towards the camera; no central full-frame flash.
        times = [(0, 14), (delay + .08, 28), (.48 + delay, 152),
                 (1.05 + delay, 430), (1.72 + delay, 850), (2, 950)]
        pos = animated([(t, [500 + r * math.cos(a), 500 + r * math.sin(a), 0])
                        for t, r in times])
        form = path([[0, -24], [0, 24]]) if kind == "ray" else geometry(kind, 16)
        layers.append(layer(f"Flight {n+1}", form, 2, color, 3.4,
                            position=pos, rotation=static(n * 360 / count + (0 if kind != "ray" else 0)),
                            size=scale([(0, 18), (.5, 80), (1.5, 220), (2, 260)]),
                            opacity=animated([(0, 0), (.10 + delay, 90), (.85 + delay, 80),
                                              (1.70 + delay, 0), (2, 0)])))
    emit("One_Shot", family, name, 2, layers, description, [color])


def shots():
    echo_shot("Circles", "Ring_Wake", "circle", 3, 5, 0,
              "Пять тонких круговых волн ускоряются от центра и растворяются за краями.")
    echo_shot("Squares", "Square_Tunnel", "square", 1, 5, 18,
              "Пять квадратных рамок дают короткий пролёт через геометрический тоннель.")
    echo_shot("Hexagons", "Hex_Echo", "hex", 2, 4, -16,
              "Четыре разреженных шестигранных эха с небольшим доворотом.")
    fly_shot("Triangles", "Prism_Flight", "triangle", 0, 6,
             "Шесть небольших контурных треугольников разлетаются от центра к зрителю.")
    fly_shot("Lines", "Radial_Streaks", "ray", 3, 8,
             "Восемь коротких радиальных штрихов; подходит для частых ударов без сплошной заливки.")
    fly_shot("Crosses", "Cross_Flight", "cross", 2, 6,
             "Шесть маленьких крестов приближаются и уходят в края.")


def backgrounds():
    # Five wrapped phases make a continuous sparse tunnel. All shapes are faded
    # to zero at the wrap; first/last JSON states exactly match.
    layers = []
    for n in range(5):
        phase = n * 8 / 5
        samples = []
        for step in range(81):
            t = step / 10
            q = ((t + phase) % 8) / 8
            radius = 32 + 680 * q * q
            alpha = 100 * math.sin(math.pi * q) ** 2
            samples.append((t, radius, alpha))
        layers.append(layer(f"Depth ring {n+1}", ellipse(100), 8, 2, 2.4,
                            size=scale([(t, r) for t, r, _ in samples]),
                            opacity=animated([(t, a) for t, _, a in samples])))
    emit("BG", "Circles", "Quiet_Depth", 8, layers,
         "Пять очень тихих контурных колец: непрерывное движение из глубины, без яркого центра.", [2])

    layers = []
    for n in range(12):
        angle = n * 137.507764
        r = 180 + (n % 4) * 94
        a = math.radians(angle)
        x, y = 500 + r * math.cos(a), 500 + r * math.sin(a)
        kind = ["circle", "square", "triangle"][n % 3]
        layers.append(layer(f"Drifter {n+1}", geometry(kind, 12 + n % 3 * 4), 8,
                            2 if n % 3 else 4, 2.8, 100,
                            position=animated([(0, [x, y, 0]),
                                               (4, [x + math.cos(a + .7) * 30, y - 35, 0]),
                                               (8, [x, y, 0])]),
                            rotation=animated([(0, n * 15), (4, n * 15 + 25), (8, n * 15)])))
    emit("BG", "Geometry", "Orbital_Dust", 8, layers,
         "Двенадцать разреженных маленьких контуров медленно дрейфуют; центр остаётся свободным.", [2, 4])

    layers = []
    for n in range(6):
        a = n * 60
        layers.append(layer(f"Far meridian {n+1}", path([[0, -300], [0, -640]]), 8, 3, 2.2,
                            opacity=100,
                            rotation=animated([(0, a), (4, a + 8), (8, a)]),
                            size=scale([(0, 95), (4, 105), (8, 95)])))
    layers.append(layer("Broken distant ring", ellipse(340), 8, 3, 2, 100,
                        rotation=animated([(0, 0), (4, 22), (8, 0)]), draw=trim(0, 62)))
    emit("BG", "Lines", "Quiet_Meridians", 8, layers,
         "Тихие внешние лучи и неполное кольцо оставляют пространство центральному объекту.", [3])


if __name__ == "__main__":
    central_shapes()
    shots()
    backgrounds()
    from extend_centerline import populate
    MANIFEST.extend(populate())
    from complete_centerline import populate as complete
    MANIFEST.extend(complete())
    from revise_centerline import populate as revise
    revisions=revise()
    MANIFEST[:]=[revisions.get(item['file'],item) for item in MANIFEST]
    (PACK / "manifest.json").write_text(json.dumps({"name": "Centerline", "version": "0.4",
        "canvas": [1000, 1000], "fps": 60, "files": MANIFEST}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Created {len(MANIFEST)} Lottie animations in {PACK}")
