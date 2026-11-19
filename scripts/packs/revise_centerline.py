"""v0.4 art direction: sparse continuous depth flow, no visible global return."""
import math
from create_centerline import MANIFEST, animated, static, layer, path, polygon, rectangle, ellipse, geometry, trim, emit
from complete_centerline import cycle, spin, at
from extend_centerline import point


def stream(name, form, n, *, count=6, color=2, width=2.4, origin=(0,0),
           angle=None, bend=18, radius=(12,790), sizes=(30,200), turn=80,
           phase_step=None, vertical=False, lane=0, depth_power=1.5):
    """An independent object travels once then wraps only while invisible.

    q is local lifetime, not a single whole-scene fade. Sampled with linear
    temporal interpolation so each tiny segment doesn't ease to a stop.
    """
    phase=(n*(phase_step if phase_step is not None else 8/count))%8
    base=n*137.507764 if angle is None else angle
    positions=[];scales=[];rotations=[];opacities=[]
    samples={k/16:((k/16+phase)%8)/8 for k in range(129)}
    reset=(8-phase) if phase else 8
    samples[reset-.0001]=1
    samples[reset]=0
    for t,q in sorted(samples.items()):
        distance=radius[0]+(radius[1]-radius[0])*q**depth_power
        a=base+bend*q
        if vertical:
            pos=[500+origin[0]+lane+26*math.sin(q*math.pi+n),-180+1360*q,0]
        else:
            pos=point(distance,a);pos[0]+=origin[0];pos[1]+=origin[1]
        size=sizes[0]+(sizes[1]-sizes[0])*q
        fade=min(1,q/.10,(1-q)/.16)
        positions.append((t,pos));scales.append((t,[size,size,100]))
        rotations.append((t,base+turn*q))
        opacities.append((t,max(0,fade)*100))
    return layer(name,form,8,color,width,opacity=animated(opacities,linear=True),
                 position=animated(positions,linear=True),size=animated(scales,linear=True),
                 rotation=animated(rotations,linear=True))


def circles():
    ls=[]
    for n in range(6):
        ls.append(stream(f'Travelling lane {n}',ellipse(48+n%2*12),n,count=6,
                         origin=(-28,25),bend=48,sizes=(40,195),turn=135))
        ls[-1]['shapes'].append(trim(0,28+n%3*7))
    emit('BG','Circles','Orbital_Lanes',8,ls,
         'Небольшие дуговые фрагменты непрерывно расходятся по кривым в глубину; статичных больших орбит больше нет.',[2])
    ls=[]
    for n in range(6):
        ls.append(stream(f'Outbound ring {n}',ellipse(19+n%3*5),n,count=6,
                         origin=(20,-15),bend=-12,sizes=(45,220),turn=0,phase_step=1.29))
    emit('BG','Circles','Drifting_Rings',8,ls,
         'Редкие малые кольца идут от внутренней области к краям с разным возрастом, без возвратного дрейфа.',[2])
    ls=[]
    for n in range(4):
        ls.append(stream(f'Precessing depth arc {n}',ellipse(100),n,count=4,
                         origin=((n%2-.5)*35,(n//2-.5)*35),radius=(0,0),
                         angle=n*90+15,bend=0,sizes=(12,820),turn=170,width=1.25))
        ls[-1]['shapes'].append(trim(0,28))
    emit('BG','Circles','Slow_Precession',8,ls,
         'Четыре разорванные дуги растут из глубины и проворачиваются с независимой фазой; постоянного крупного знака нет.',[2])
    ls=[]
    for n in range(4):
        ls.append(stream(f'Passing ring cluster {n}',ellipse(25),n,count=4,
                         origin=(15,10),bend=32,sizes=(22,180),turn=-120,phase_step=1.87))
        for x,y in ((42,0),(21,36)):
            circle=ellipse(25);circle['p']=static([x,y])
            ls[-1]['shapes'].insert(1,circle)
        ls[-1]['shapes'].append(trim(0,76))
    emit('BG','Circles','Ring_Archipelago',8,ls,
         'Небольшие связки из трёх колец выходят из глубины и разлетаются, без статичной круговой расстановки.',[2])
    ls=[]
    for n in range(5):
        ls.append(stream(f'Vertical curl {n}',ellipse(36+n%2*9),n,count=5,
                         vertical=True,lane=(n%3-1)*130,sizes=(50,145),turn=150))
        ls[-1]['shapes'].append(trim(0,45))
    emit('BG','Circles','Vertical_Orbits',8,ls,
         'Пять разорванных круговых следов проходят сверху вниз внутри центральных дорожек, видимых в вертикальном Fill.',[2])
    ls=[]
    for n in range(4):
        ls.append(stream(f'Twin stream {n}',ellipse(52),n,count=4,
                         origin=(0,-10),angle=35+n*90,bend=(-1 if n%2 else 1)*40,
                         sizes=(30,190),turn=(-1 if n%2 else 1)*165))
        ls[-1]['shapes'].append(trim(0,42))
    emit('BG','Circles','Twin_Lenses',8,ls,
         'Две пары круговых фрагментов расходятся по независимым кривым; неподвижных линз и растяжения по осям нет.',[2])
    ls=[]
    for n in range(7):
        ls.append(stream(f'Passing arc {n}',ellipse(20+n%3*6),n,count=7,
                         origin=(-12,32),bend=32,sizes=(45,230),turn=120,phase_step=1.13))
        ls[-1]['shapes'].append(trim(0,24+n%2*14))
    emit('BG','Circles','Arc_Constellation',8,ls,
         'Разреженная дуговая пыль расходится без закреплённых мест и общего обратного движения.',[2])
    ls=[]
    for n in range(5):
        ls.append(stream(f'Lunar fragment {n}',ellipse(80),n,count=5,
                         origin=(18,24),angle=n*72-90,bend=-32,sizes=(18,175),turn=-155,width=2.1))
        ls[-1]['shapes'].append(trim(0,12+n%3*5))
    emit('BG','Circles','Lunar_Lanes',8,ls,
         'Короткие лунные серпы пролетают к краям с разной фазой, не образуя постоянных дорожек.',[2])


def lines():
    # Long, nearly screen-wide wave bands are intentionally removed. Each motif
    # is a small travelling fragment with its own phase and finite lifetime.
    ls=[]
    for n in range(6):
        ls.append(stream(f'Short tension fragment {n}',path([[-24,4],[0,-4],[24,3]]),n,
                         color=3,width=2.5,bend=12,sizes=(45,165),turn=45,origin=(20,-20)))
    emit('BG','Lines','Tension_Lines',8,ls,
         'Вместо шести натянутых полос — редкие короткие ломаные следы, непрерывно уходящие из глубины.',[3])
    ls=[]
    for n in range(5):
        verts=[[-34,-3],[-17,5],[0,0],[17,-5],[34,3]]
        ls.append(stream(f'Wisp {n}',path(verts),n,count=5,color=3,width=2.3,
                         bend=35,sizes=(35,170),turn=90,origin=(-30,15)))
    emit('BG','Lines','Wave_Filaments',8,ls,
         'Пять коротких волнистых нитей пролетают по отдельным траекториям; горизонтальных полос на весь экран нет.',[3])
    ls=[]
    for n in range(5):
        ls.append(stream(f'Horizon fleck {n}',path([[-27,6],[-9,-3],[9,-6],[27,1]]),n,count=5,
                         color=3,width=2.4,angle=n*137.5+35,bend=-22,sizes=(40,175),turn=35,
                         origin=(25,0)))
    emit('BG','Lines','Sine_Horizon',8,ls,
         'Независимые короткие дугообразные следы заменяют три явно читаемые синусоидальные полосы.',[3])
    ls=[]
    for n in range(4):
        for side in (-1,1):
            form=path([[-26,side*5],[0,side*8],[26,side*5]])
            ls.append(stream(f'Ribbon fleck {n}-{side}',form,n,count=4,color=3,width=2,
                             origin=(-10,20),angle=n*90+25,bend=38,sizes=(30,150),turn=80))
    emit('BG','Lines','Ribbon_Lanes',8,ls,
         'Четыре небольших парных следа расходятся из глубины; сплошные длинные ленты убраны.',[3])
    ls=[]
    for n in range(6):
        l=stream(f'Depth rail pair {n}',path([[-36,0],[-7,0]]),n,count=6,
                 color=3,width=2.4,angle=n*60-80,bend=15,sizes=(28,240),turn=20,origin=(0,18))
        l['shapes'].insert(1,path([[8,0],[37,0]]))
        ls.append(l)
    emit('BG','Lines','Horizon_Rails',8,ls,
         'Короткие разорванные пары рельсов приближаются и расходятся по разным направлениям, без статичной веерной схемы.',[3])
    ls=[]
    for n in range(7):
        form=path([[-14,0],[14,0]]) if n%2 else path([[-12,4],[0,-4],[12,4]])
        ls.append(stream(f'Peripheral passing mark {n}',form,n,count=7,color=3,width=2.8,
                         origin=(-24,-14),bend=44,sizes=(40,210),turn=-35,phase_step=1.09))
    emit('BG','Lines','Peripheral_Rays',8,ls,
         'Вместо качающихся лучей по окружности — нерегулярные короткие штрихи с непрерывным разлётом.',[3])


def geometry_fields():
    ls=[]
    for n in range(6):
        ls.append(stream(f'Approaching cell {n}',polygon(6,24,-30),n,count=6,
                         origin=(0,18),angle=n*60-90,bend=12,sizes=(30,185),turn=45))
    emit('BG','Geometry','Breathing_Cells',8,ls,
         'Малые соты появляются во внутренней области и расходятся наружу: центральные фазы видны и при вертикальном кропе.',[2])
    ls=[]
    for n in range(9):
        ls.append(stream(f'Outgoing dust {n}',geometry(('circle','square','triangle')[n%3],13+n%2*4),n,
                         count=9,color=2 if n%4 else 4,width=2.4,origin=(18,-12),
                         bend=20,sizes=(35,200),turn=100,phase_step=.87))
    emit('BG','Geometry','Orbital_Dust',8,ls,
         'Геометрическая пыль непрерывно разлетается из внутренней области, без флипфлопа и коллективного возврата.',[2,4])
    ls=[]
    for n in range(5):
        ls.append(stream(f'Approaching gate {n}',rectangle(34,2),n,count=5,
                         origin=(-12,12),angle=n*72+20,bend=-12,sizes=(20,265),turn=35,width=2.4))
        ls[-1]['shapes'].append(trim(0,78))
    emit('BG','Geometry','Parallax_Gates',8,ls,
         'Незамкнутые рамки приближаются и покидают кадр; вместо качания ширины — только равномерный масштаб и движение в глубину.',[2])
    ls=[]
    for n in range(7):
        ls.append(stream(f'Free tile {n}',rectangle(17,1),n,count=7,
                         origin=(12,8),bend=-26,sizes=(30,210),turn=95,phase_step=1.10))
    emit('BG','Geometry','Tilted_Grid',8,ls,
         'Разреженные плитки расходятся с плавным вращением. Закреплённая почти неподвижная сетка удалена.',[2])
    ls=[]
    for n in range(7):
        ls.append(stream(f'Outgoing angular glyph {n}',path([[-13,7],[0,-8],[13,7]]),n,count=7,
                         color=2,width=2.6,origin=(-15,20),bend=30,sizes=(40,220),turn=110,phase_step=1.07))
    emit('BG','Geometry','Zigzag_Drifters',8,ls,
         'Угловые знаки с разных глубин движутся наружу по плавным кривым, без движения по ломаной туда-обратно.',[2])


def hero_shapes():
    ls=[]
    for n,r in enumerate((172,112)):
        ls.append(layer(f'Interlocked hex {n}',polygon(6,r,-30),4,2 if not n else 3,3.5,
                        position=cycle(lambda q,n=n:[500+18*math.cos(math.tau*q+n*math.pi),
                                                           500+18*math.sin(math.tau*q+n*math.pi),0]),
                        rotation=spin(0,60 if not n else -60)))
    for n in range(6):
        ls.append(layer(f'Braid tooth {n}',path([[-10,0],[10,0]]),4,2,3,
                        position=cycle(lambda q,n=n:point(218,n*60+60*q)),
                        rotation=spin(n*60+90,n*60+150)))
    emit('Shapes','Hexagons','Hex_Braid',4,ls,
         'Два шестигранника сцепляются встречным вращением и малым круговым смещением; шесть зубцов ведут внешний ритм. Растяжения по осям нет.',[2,3])
    ls=[]
    for n in range(2):
        ls.append(layer(f'Interlaced circular arc {n}',ellipse(138),4,3,3.6,
                        position=cycle(lambda q,n=n:[500+52*math.cos(math.tau*q+n*math.pi),
                                                           500+32*math.sin(math.tau*q+n*math.pi),0]),
                        rotation=spin(n*180,n*180+(360 if n else -360)),draw=trim(0,68)))
    ls.append(layer('Inner tracer',ellipse(70),4,4,2.3,rotation=spin(0,360),draw=trim(0,22)))
    emit('Shapes','Circles','Breathing_Lens',4,ls,
         'Круговые дуги переплетаются и встречным вращением меняют просветы; все окружности сохраняют форму.',[3,4])
    ls=[]
    for n in range(4):
        ls.append(layer(f'Rotating corner assembly {n}',path([[-48,0],[0,0],[0,48]]),4,1,3.8,
                        position=cycle(lambda q,n=n:point(155+18*math.sin(math.tau*q),45+n*90+q*90)),
                        rotation=spin(n*90,n*90+90)))
    ls.append(layer('Square escapement',rectangle(56,2),4,4,2.7,rotation=spin(45,-135)))
    for n in range(4):
        ls.append(layer(f'Guide tooth {n}',path([[-9,0],[9,0]]),4,1,3,
                        position=cycle(lambda q,n=n:point(235,n*90+q*90)),rotation=spin(n*90+90,n*90+180)))
    emit('Shapes','Squares','Ladder_Block',4,ls,
         'Вместо перекладин — механическая сборка из четырёх углов и встречного центрального квадрата, с непрерывным четвертьоборотом.',[1,4])
    ls=[]
    for n,r in enumerate((150,103,54)):
        ls.append(layer(f'Counter rotating frame {n}',rectangle(r,2),4,1 if n!=1 else 4,3.2,
                        rotation=spin(n*18,n*18+(90 if n%2 else -90))))
    for n in range(4):
        ls.append(layer(f'Orbital corner marker {n}',rectangle(5,1),4,1,2.5,
                        position=cycle(lambda q,n=n:point(241,45+n*90+90*q)),rotation=spin(45,135)))
    emit('Shapes','Squares','Rect_Wave',4,ls,
         'Сетку заменяют три встречновращающиеся квадратные рамки и четыре внешних маркера. Равномерные размеры, без деформации осей.',[1,4])


def populate():
    MANIFEST.clear()
    circles();lines();geometry_fields();hero_shapes()
    assert len(MANIFEST)==23
    return {item['file']:item for item in MANIFEST}
