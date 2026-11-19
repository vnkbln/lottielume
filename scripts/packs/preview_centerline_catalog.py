"""Folder-by-folder native-render previews for the expanded Centerline pack."""
from concurrent.futures import ThreadPoolExecutor
from collections import OrderedDict
from pathlib import Path
import hashlib
import json
import subprocess
import preview_centerline as p
from PIL import Image, ImageDraw


def groups():
    result=OrderedDict()
    for i,item in enumerate(p.ITEMS):
        result.setdefault(str(Path(item['file']).parent).replace('\\','/'),[]).append(i)
    return result


COLS=5
CELL_W=250
CELL_H=280
HEADER=36
WIDTH=COLS*CELL_W
HEIGHT=HEADER+2*CELL_H


def row(group,indices,frame):
    image=Image.new('RGB',(WIDTH,HEIGHT),'#090c15')
    draw=ImageDraw.Draw(image)
    draw.text((14,9),group.replace('/',' / '),font=p.FONT,fill='#dce7fb')
    for number,i in enumerate(indices):
        col,r=number%COLS,number//COLS
        x,y=col*CELL_W,HEADER+r*CELL_H
        item=p.ITEMS[i];count=round(item['duration']*p.FPS)
        thumb=p.load(p.CACHE/f'asset-{i:02d}/{frame%count:03d}.png')
        thumb.thumbnail((224,224))
        image.paste(p.composite_on_black(thumb),(x+13,y+2))
        draw.text((x+12,y+232),Path(item['file']).stem.replace('_',' '),font=p.FONT,fill='#e7edf8')
        draw.text((x+12,y+255),f"{item['duration']}s  /  1000 x 1000",font=p.SMALL,fill='#8292ad')
    return image


def main():
    p.PREVIEW.mkdir(exist_ok=True)
    mapping=groups()
    assert len(mapping)==13 and all(len(v)==10 for v in mapping.values())
    fingerprints={str(i):hashlib.sha256((p.PACK/x['file']).read_bytes()).hexdigest() for i,x in enumerate(p.ITEMS)}
    index=p.CACHE/'catalog-cache.json'
    cached=json.loads(index.read_text()) if index.exists() else {}
    jobs=[]
    for i,item in enumerate(p.ITEMS):
        count=round(item['duration']*p.FPS)
        for f in range(count):
            if cached.get(str(i))!=fingerprints[str(i)] or not (p.CACHE/f'asset-{i:02d}/{f:03d}.png').exists():
                jobs.append((i,f))
    with ThreadPoolExecutor(max_workers=4) as pool:
        for count,_ in enumerate(pool.map(p.sample_job,jobs),1):
            if count%500==0:print(f'Native frames {count}/{len(jobs)}',flush=True)
    index.write_text(json.dumps(fingerprints))
    print(f'Rendered {len(jobs)} frames for {len(p.ITEMS)} assets',flush=True)
    p.inspect()
    print('Native render / palette / timing / transparent endpoint checks passed',flush=True)
    (p.PREVIEW/'Folders').mkdir(exist_ok=True)
    contacts={role:Image.new('RGB',(WIDTH,HEIGHT*sum(g.startswith(role+'/') for g in mapping)),'#090c15')
              for role in ('Shapes','One_Shot','BG')}
    role_rows={role:0 for role in contacts}
    full=Image.new('RGB',(WIDTH,HEIGHT*len(mapping)),'#090c15')
    concat=[];gif=[]
    for group_number,(group,indices) in enumerate(mapping.items()):
        count=round(p.ITEMS[indices[0]]['duration']*p.FPS)
        cache=p.CACHE/'groups'/group.replace('/','_')
        cache.mkdir(parents=True,exist_ok=True)
        for f in range(count):row(group,indices,f).save(cache/f'{f:03d}.png')
        name=group.replace('/','_')+'.mp4'
        destination=p.PREVIEW/'Folders'/name
        p.movie(cache/'%03d.png',destination)
        concat.append(destination)
        for f in range(0,48,2):
            gif.append(row(group,indices,f%count).resize((875,418),Image.Resampling.LANCZOS))
        # Sample each item at a readable point, not at One_Shot's blank first frame.
        contact=row(group,indices,0)
        for number,i in enumerate(indices):
            col,r=number%COLS,number//COLS
            item=p.ITEMS[i]
            f=round((.72 if item['role']=='One_Shot' else item['duration']*.35)*p.FPS)
            frame_image=row(group,indices,f)
            x,y=col*CELL_W,HEADER+r*CELL_H
            contact.paste(frame_image.crop((x,y,x+CELL_W,y+CELL_H)),(x,y))
            snapshot=p.rendered(item,f/p.FPS,600,p.PREVIEW/(Path(item['file']).stem+'.png'))
            p.load(snapshot).save(snapshot,optimize=True)
        full.paste(contact,(0,group_number*HEIGHT))
        role=group.split('/')[0]
        contacts[role].paste(contact,(0,role_rows[role]*HEIGHT));role_rows[role]+=1
        print(f'Preview complete: {group}',flush=True)
    full.save(p.PREVIEW/'Contact_Sheet.png')
    for role,image in contacts.items():image.save(p.PREVIEW/f'{role}_Contact.png')
    listing=p.CACHE/'concat.txt'
    listing.write_text(''.join("file '"+str(f.resolve()).replace('\\','/')+"'\n" for f in concat))
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','concat','-safe','0',
                    '-i',str(listing),'-c','copy','-movflags','+faststart',str(p.PREVIEW/'Pack_Preview.mp4')],check=True)
    gif[0].save(p.PREVIEW/'Pack_Preview.gif',save_all=True,append_images=gif[1:],duration=83,loop=0)
    # Keep the three-layer/aspect study on the same original source files.
    hires=[(i,f) for i in (0,8,10) for f in range(round(p.ITEMS[i]['duration']*p.FPS))
           if cached.get(str(i))!=fingerprints[str(i)] or not (p.CACHE/f'hi-{i:02d}/{f:03d}.png').exists()]
    def hi(job):
        i,f=job;return p.rendered(p.ITEMS[i],f/p.FPS,1000,p.CACHE/f'hi-{i:02d}/{f:03d}.png')
    with ThreadPoolExecutor(max_workers=4) as pool:
        for _ in pool.map(hi,hires):pass
    (p.CACHE/'setup').mkdir(exist_ok=True)
    for f in range(8*p.FPS):p.setup_frame(f).save(p.CACHE/f'setup/{f:03d}.png')
    p.movie(p.CACHE/'setup/%03d.png',p.PREVIEW/'Layer_Setup.mp4')
    p.setup_frame(round(.92*p.FPS)).save(p.PREVIEW/'Aspect_Setup.png')
    print(f'{len(p.ITEMS)}-file catalogue and folder movies completed',flush=True)


if __name__=='__main__':main()
