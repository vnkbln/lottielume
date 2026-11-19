"""Render and inspect Centerline through the project's actual ThorVG CPU renderer.

Pillow is used only for preview sheets/compositing. Lottie geometry is never rasterized
into the deliverable animations. FFmpeg writes a compact local review movie.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json
import math
import subprocess
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / "assets/packs/Centerline_v01"
PREVIEW = PACK / "Preview"
CACHE = ROOT / "output/centerline-preview"
RENDER = ROOT / "build/Release/lottie-render.exe"
FPS = 24
ITEMS = json.loads((PACK / "manifest.json").read_text(encoding="utf-8"))["files"]
FONT = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 15)
SMALL = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 12)


def rendered(item, time, size, filename):
    filename.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(RENDER), str(PACK / item["file"]), str(filename),
                             str(time), str(size), str(size)], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(item["file"] + ": " + result.stderr)
    return filename


def load(path):
    with Image.open(path) as im:
        return im.convert("RGBA")


def sample_job(job):
    i, frame = job
    item = ITEMS[i]
    return rendered(item, frame / FPS, 256, CACHE / f"asset-{i:02d}/{frame:03d}.png")


def composite_on_black(image):
    bg = Image.new("RGBA", image.size, "#090c15")
    return Image.alpha_composite(bg, image).convert("RGB")


def inspect():
    report = []
    for i, item in enumerate(ITEMS):
        doc = json.loads((PACK / item["file"]).read_text())
        assert (doc["w"], doc["h"], doc["fr"]) == (1000, 1000, 60)
        assert (doc["op"] - doc["ip"]) / doc["fr"] == item["duration"]
        assert not doc["assets"]
        allowed = {tuple(int(h[k:k+2],16)/255 for k in (0,2,4)) for h in
                   ("f318e8","9c32fe","277ff4","00f3e9","ffffff","000000")}
        for layer in doc["layers"]:
            assert layer["ty"] == 4 and not layer.get("masksProperties") and not layer.get("ef")
            for prop in layer["ks"].values():
                if prop.get("a"):
                    times = [key["t"] for key in prop["k"]]
                    assert times == sorted(set(times)), (item["file"], times)
            for shape in layer['shapes']:
                if shape['ty'] in ('st','fl'):
                    assert shape['c']['a'] == 0 and tuple(shape['c']['k'][:3]) in allowed
        sample_frames = [0, int(item["duration"] * FPS * .25),
                         int(item["duration"] * FPS * .5), int(item["duration"] * FPS * .75)]
        coverages, equivalent = [], []
        for frame in sample_frames:
            alpha = load(CACHE / f"asset-{i:02d}/{frame:03d}.png").getchannel("A")
            hist = alpha.histogram()
            coverages.append(round(sum(hist[9:]) / 256**2 * 100, 3))
            equivalent.append(round(sum(n * v / 255 for n, v in enumerate(hist)) / 256**2 * 100, 3))
        first = load(CACHE / f"asset-{i:02d}/000.png")
        safe_box = None
        if item['role']=='Shapes':
            box=[256,256,0,0]
            for frame in range(round(item['duration']*FPS)):
                alpha=load(CACHE/f'asset-{i:02d}/{frame:03d}.png').getchannel('A')
                b=alpha.point(lambda a:255 if a>4 else 0).getbbox()
                if b:box=[min(box[0],b[0]),min(box[1],b[1]),max(box[2],b[2]),max(box[3],b[3])]
            assert box[0]>=55 and box[1]>=55 and box[2]<=201 and box[3]<=201, (item['file'],box)
            safe_box=[round(v*1000/256,2) for v in box]
        if item['role']=='BG':
            for l in doc['layers']:
                o=l['ks']['o']
                peak=max(k['s'][0] for k in o['k']) if o['a'] else o['k']
                assert abs(peak-100)<1e-7, 'BG must have full peak opacity'
        endfile = rendered(item, item["duration"] - 1/60, 256, CACHE / f"end-{i:02d}.png")
        end = load(endfile)
        if item["role"] == "One_Shot":
            assert not first.getchannel("A").getbbox(), "One_Shot must start transparent"
            assert not end.getchannel("A").getbbox(), "One_Shot must end transparent"
        if item["role"] != "One_Shot":
            # The final rendered frame is 1/60s before the repeated start.
            # Record the difference; it is not an exact same-time comparison.
            # Ignore undefined RGB under transparent pixels: compare visible black composites.
            aa, bb = composite_on_black(first), composite_on_black(end)
            delta = sum(abs(a-b) for a,b in zip(aa.tobytes(),bb.tobytes())) / (256*256*3)
        else:
            delta = 0
        report.append({**item, "bytes": (PACK/item["file"]).stat().st_size,
                       "sampled_safe_zone_bounds_in_source_units": safe_box,
                       "sampled_ink_area_percent": coverages,
                       "sampled_equivalent_alpha_percent": equivalent,
                       "loop_adjacent_frame_mean_channel_difference": round(delta, 5)})
    (PREVIEW / "QA.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def sheet(frame, tile=256, caption=43):
    board = Image.new("RGB", (tile * 4, (tile + caption) * 4), "#090c15")
    draw = ImageDraw.Draw(board)
    for i, item in enumerate(ITEMS):
        count = round(item["duration"] * FPS)
        im = load(CACHE / f"asset-{i:02d}/{frame % count:03d}.png")
        x, y = i % 4 * tile, i // 4 * (tile + caption)
        board.paste(composite_on_black(im), (x, y))
        name = Path(item["file"]).stem.replace("_", " ")
        draw.text((x+12, y+tile+1), name, font=FONT, fill="#e7edf8")
        draw.text((x+12, y+tile+23), f"{item['role']}   {item['duration']}s", font=SMALL, fill="#8c9bb5")
    return board


def fit_fill(im, size):
    ratio = max(size[0]/im.width, size[1]/im.height)
    im = im.resize((round(im.width*ratio),round(im.height*ratio)), Image.Resampling.LANCZOS)
    x, y = (im.width-size[0])//2, (im.height-size[1])//2
    return im.crop((x,y,x+size[0],y+size[1]))


def setup_frame(frame):
    # Three layer roles, shown simultaneously through two different screen crops.
    t = frame/FPS
    combined = load(CACHE/f"hi-10/{frame:03d}.png")
    shots = Image.new("RGBA", combined.size)
    for n in range(3):
        age = t % .8 + n*.8
        if age >= 2 or t-age < -.001:
            continue
        shot = load(CACHE/f"hi-08/{min(2*FPS-1,round(age*FPS)):03d}.png")
        shot.putalpha(shot.getchannel("A").point(lambda x: round(x*.55)))
        shots = Image.alpha_composite(shots,shot)
    combined = Image.alpha_composite(combined, shots)
    combined = Image.alpha_composite(combined, load(CACHE/f"hi-00/{frame%(4*FPS):03d}.png"))
    board = Image.new("RGB",(1280,680),"#090c15")
    draw=ImageDraw.Draw(board)
    for rect in [(16,82,912,513),(962,82,288,512)]:
        x,y,w,h=rect
        board.paste(composite_on_black(fit_fill(combined,(w,h))),(x,y))
        draw.rectangle((x,y,x+w,y+h),outline="#27334a",width=1)
    draw.text((18,16),"CENTERLINE / 3 LAYERS / SAME SOURCE ANIMATIONS",font=FONT,fill="#e7edf8")
    draw.text((18,51),"16:9 / Fill crop",font=FONT,fill="#8c9bb5")
    draw.text((964,51),"9:16 / Fill crop",font=FONT,fill="#8c9bb5")
    draw.text((18,624),"Halo  +  Radial Streaks  +  Quiet Depth",font=FONT,fill="#e7edf8")
    draw.text((18,651),"Square vector sources 1000 x 1000 / preview uses 24 fps",font=SMALL,fill="#8c9bb5")
    return board


def movie(pattern, destination):
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-framerate", str(FPS),
                    "-i", str(pattern), "-c:v", "libx264", "-crf", "19", "-pix_fmt", "yuv420p",
                    "-movflags", "+faststart", str(destination)], check=True)


if __name__ == "__main__":
    if len(ITEMS)>13:
        from preview_centerline_catalog import main
        main()
        raise SystemExit(0)
    PREVIEW.mkdir(exist_ok=True)
    jobs = [(i, f) for i, item in enumerate(ITEMS) for f in range(round(item["duration"]*FPS))]
    with ThreadPoolExecutor(max_workers=4) as pool:
        for _ in pool.map(sample_job, jobs):
            pass
    print(f"Rendered {len(jobs)} native ThorVG frames", flush=True)
    hires = [(i,f) for i in (0,8,10) for f in range(round(ITEMS[i]['duration']*FPS))]
    def hi_job(job):
        i,f=job
        return rendered(ITEMS[i],f/FPS,1000,CACHE/f"hi-{i:02d}/{f:03d}.png")
    with ThreadPoolExecutor(max_workers=4) as pool:
        for _ in pool.map(hi_job,hires):
            pass
    inspect()
    gif = []
    (CACHE/"sheet").mkdir(exist_ok=True)
    (CACHE/"setup").mkdir(exist_ok=True)
    for frame in range(8*FPS):
        image=sheet(frame)
        image.save(CACHE/f"sheet/{frame:03d}.png")
        setup_frame(frame).save(CACHE/f"setup/{frame:03d}.png")
        if frame % 2 == 0:
            gif.append(image.resize((768,897),Image.Resampling.LANCZOS))
    gif[0].save(PREVIEW/"Pack_Preview.gif",save_all=True,append_images=gif[1:],duration=83,loop=0)
    movie(CACHE/"sheet/%03d.png",PREVIEW/"Pack_Preview.mp4")
    movie(CACHE/"setup/%03d.png",PREVIEW/"Layer_Setup.mp4")
    setup_frame(round(.92*FPS)).save(PREVIEW/"Aspect_Setup.png")
    # Midpoints differ by role, so one-shots stay readable in a static overview.
    board=Image.new("RGB",(1024,1196),"#090c15")
    draw=ImageDraw.Draw(board)
    for i,item in enumerate(ITEMS):
        f=round((.72 if item["role"]=="One_Shot" else item["duration"]*.35)*FPS)
        im=load(CACHE/f"asset-{i:02d}/{f:03d}.png")
        snapshot=rendered(item,f/FPS,600,PREVIEW/(Path(item['file']).stem+'.png'))
        load(snapshot).save(snapshot,optimize=True)
        x,y=i%4*256,i//4*299
        board.paste(composite_on_black(im),(x,y))
        draw.text((x+12,y+257),Path(item['file']).stem.replace('_',' '),font=FONT,fill="#e7edf8")
        draw.text((x+12,y+280),f"{item['role']}   {item['duration']}s",font=SMALL,fill="#8c9bb5")
    board.save(PREVIEW/"Contact_Sheet.png")
    print("Preview movies, contact sheet and native-render QA saved",flush=True)
