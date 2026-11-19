"""Focused 9:16 Fill review and visibility measurements for the v0.4 revisions."""
import json
from pathlib import Path
from PIL import Image, ImageDraw
import preview_centerline as p

BG_NAMES={
    'Orbital_Lanes','Drifting_Rings','Slow_Precession','Ring_Archipelago',
    'Vertical_Orbits','Twin_Lenses','Arc_Constellation','Lunar_Lanes',
    'Tension_Lines','Wave_Filaments','Sine_Horizon','Ribbon_Lanes','Horizon_Rails','Peripheral_Rays',
    'Breathing_Cells','Orbital_Dust','Parallax_Gates','Tilted_Grid','Zigzag_Drifters'}
SHAPE_NAMES={'Hex_Braid','Breathing_Lens','Ladder_Block','Rect_Wave'}


def main():
    bg=[(i,x) for i,x in enumerate(p.ITEMS) if x['role']=='BG' and Path(x['file']).stem in BG_NAMES]
    baseline=next((i,x) for i,x in enumerate(p.ITEMS) if Path(x['file']).stem=='Quiet_Depth')
    bg=[baseline]+bg
    shapes=[(i,x) for i,x in enumerate(p.ITEMS) if x['role']=='Shapes' and Path(x['file']).stem in SHAPE_NAMES]
    assert len(bg)==20 and len(shapes)==4
    folder=p.CACHE/'revision-portrait';folder.mkdir(exist_ok=True)
    shape_folder=p.CACHE/'revision-shapes';shape_folder.mkdir(exist_ok=True)
    visibility=[[] for _ in bg]
    for frame in range(8*p.FPS):
        board=Image.new('RGB',(1100,1240),'#090c15');draw=ImageDraw.Draw(board)
        draw.text((14,10),'CENTERLINE v0.4 / BG / 9:16 FILL / Quiet Depth = reference',font=p.FONT,fill='#e7edf8')
        for n,(i,item) in enumerate(bg):
            source=p.load(p.CACHE/f'asset-{i:02d}/{frame:03d}.png')
            portrait=p.fit_fill(source,(135,240))
            hist=portrait.getchannel('A').histogram()
            visibility[n].append(sum(hist[9:]))
            x,y=(n%5)*220,40+(n//5)*300
            board.paste(p.composite_on_black(portrait),(x+42,y))
            draw.text((x+10,y+246),Path(item['file']).stem.replace('_',' '),font=p.FONT,fill='#dce7fb')
            draw.text((x+10,y+269),'reference' if n==0 else 'reworked / 100% peak',font=p.SMALL,fill='#8292ad')
        board.save(folder/f'{frame:03d}.png')
        if frame==66:board.save(p.PREVIEW/'Revision_v04_Portrait.png')
    for frame in range(4*p.FPS):
        board=Image.new('RGB',(1080,320),'#090c15');draw=ImageDraw.Draw(board)
        for n,(i,item) in enumerate(shapes):
            im=p.load(p.CACHE/f'asset-{i:02d}/{frame:03d}.png')
            board.paste(p.composite_on_black(im),(n*270+7,8))
            draw.text((n*270+12,279),Path(item['file']).stem.replace('_',' '),font=p.FONT,fill='#e7edf8')
            draw.text((n*270+12,302),'reworked / uniform proportions',font=p.SMALL,fill='#8292ad')
        board.save(shape_folder/f'{frame:03d}.png')
    report=[]
    for n,(_,item) in enumerate(bg):
        visible=sum(v>0 for v in visibility[n])/len(visibility[n])
        longest=run=0
        for value in visibility[n]*2:
            run=run+1 if value==0 else 0
            longest=max(longest,run)
        longest=min(longest,len(visibility[n]))/p.FPS
        report.append({'file':item['file'],'reference':n==0,'portrait_visible_fraction':round(visible,4),
                       'longest_blank_interval_seconds':round(longest,4),
                       'mean_ink_area_percent':round(sum(visibility[n])/len(visibility[n])/(135*240)*100,4)})
        assert visible>=.95,(item['file'],visible,longest)
    (p.PREVIEW/'Revision_v04_QA.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    p.movie(folder/'%03d.png',p.PREVIEW/'Revision_v04_Portrait.mp4')
    p.movie(shape_folder/'%03d.png',p.PREVIEW/'Revision_v04_Shapes.mp4')
    print('19 revised BG visible in 9:16 Fill; portrait and Shape review movies saved',flush=True)


if __name__=='__main__':main()
