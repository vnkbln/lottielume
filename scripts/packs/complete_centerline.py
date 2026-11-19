"""Seven additional, individually choreographed studies per Centerline theme."""
import math
from create_centerline import (MANIFEST, animated, static, scale, layer, path, polygon,
                               rectangle, ellipse, geometry, trim, emit)
from extend_centerline import morph, pulse, point


TAU=math.tau


def cycle(fn, duration=4, samples=32):
    return animated([(duration*k/samples,[round(x,5) for x in fn(k/samples)])
                     for k in range(samples+1)],linear=True)


def at(x,y):return static([500+x,500+y,0])


def spin(a,b,d=4):return animated([(0,a),(d,b)],linear=True)


def breathe(lo=80,hi=100,d=4):return scale([(0,lo),(d/2,hi),(d,lo)])


def dots(radius,count,color=3,d=4):
    return [layer(f"Dot {n}",ellipse(4),d,color,fill=True,
                  position=static(point(radius,n*360/count))) for n in range(count)]


def shapes_circles():
    names=['Split_Orbit','Concentric_Lock','Radar_Petals','Breathing_Lens',
           'Dot_Corona','Pendulum_Rings','Circle_Weave']
    desc=['Две большие полуокружности расходятся в стороны и встречаются вновь.',
          'Три кольца разного радиуса раскрывают разрывы с независимой фазой.',
          'Короткие радарные дуги проходят по нескольким радиусам с разной скоростью.',
          'Две окружности сходятся в линзу и расходятся без общего масштабирования.',
          'Двенадцать малых контурных точек бегут по пульсирующей короне.',
          'Три подвешенных кольца качаются на разной высоте.',
          'Четыре кольца переплетаются по плавным лиссажу-подобным траекториям.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for side in (-1,1):
                ls.append(layer(f'Split shell {side}',ellipse(175),4,3,4,
                    position=animated([(0,[500,500,0]),(2,[500+side*65,500,0]),(4,[500,500,0])]),
                    rotation=static(0 if side==1 else 180),draw=trim(0,48)))
            ls+=dots(240,4)
        elif mode==1:
            for n in range(3):
                ls.append(layer(f'Lock ring {n}',ellipse(85+n*68),4,3 if n!=1 else 4,3,
                    rotation=spin(n*35,n*35+(360 if n%2 else -360)),
                    draw=trim(0,animated([(0,35+n*12),(2,85-n*10),(4,35+n*12)]))))
        elif mode==2:
            for n in range(8):
                ls.append(layer(f'Radar petal {n}',ellipse(115+n%3*50),4,3,3.4,
                    rotation=spin(n*45,n*45+360*(1 if n%2 else -1)),draw=trim(0,8+n%3*4)))
        elif mode==3:
            for side in (-1,1):
                ls.append(layer(f'Lens circle {side}',ellipse(160),4,3,3.8,
                    position=animated([(0,[500+side*85,500,0]),(2,[500+side*25,500,0]),(4,[500+side*85,500,0])]),
                    size=animated([(0,[100,75,100]),(2,[85,100,100]),(4,[100,75,100])])) )
            ls.append(layer('Lens axis',path([[0,-195],[0,195]]),4,4,2,draw=trim(26,74)))
        elif mode==4:
            for n in range(12):
                a=n*TAU/12
                ls.append(layer(f'Corona bead {n}',ellipse(11),4,3,3,
                    position=cycle(lambda q,a=a:[500+(190+35*math.sin(TAU*q+a))*math.cos(a+TAU*q),
                                                     500+(190+35*math.sin(TAU*q+a))*math.sin(a+TAU*q),0])))
            ls.append(layer('Inner reference',ellipse(100),4,4,2,draw=trim(0,62)))
        elif mode==5:
            for n in range(3):
                ls.append(layer(f'Pendulum {n}',ellipse(65+n*5),4,3,3.2,
                    position=cycle(lambda q,n=n:[500+120*math.sin(TAU*q+n*.7),
                                                       500+(n-1)*105+14*math.cos(TAU*q+n*.7),0]),
                    draw=trim(0,82)))
        else:
            for n in range(4):
                ls.append(layer(f'Woven loop {n}',ellipse(90),4,3 if n%2 else 4,2.8,
                    position=cycle(lambda q,n=n:[500+115*math.sin(TAU*q+n*math.pi/2),
                                                       500+110*math.sin(TAU*q*2+n*math.pi/2),0])))
        emit('Shapes','Circles',name,4,ls,desc[mode],[3,4] if mode in (1,3,4,6) else [3])


def shapes_squares():
    names=['Folding_Box','Offset_Nest','Frame_Assembly','Diamond_Shutter',
           'Ladder_Block','Corner_Chase','Rect_Wave']
    desc=['Четыре стороны складываются к центру, меняя длину по одной оси.',
          'Три вложенные рамки сдвигаются по разным осям, сохраняя прямые углы.',
          'Восемь коротких сегментов поочерёдно собирают контур квадратной рамки.',
          'Четыре ромбические створки открывают и закрывают центральный просвет.',
          'Пять горизонтальных перекладин меняют ширину и смещаются ступенями.',
          'Четыре скобы непрерывно обходят квадрат по его сторонам.',
          'Девять прямоугольников передают вертикальную волну по сетке.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(4):
                ls.append(layer(f'Box side {n}',path([[-140,0],[140,0]]),4,1,3.5,
                    position=cycle(lambda q,n=n:point(120+55*math.cos(TAU*q),90+n*90)),
                    rotation=animated([(0,n*90),(2,n*90+65),(4,n*90)]),size=breathe(75,100)))
        elif mode==1:
            for n in range(3):
                ls.append(layer(f'Nested frame {n}',rectangle(75+n*45,2),4,1 if n!=1 else 4,3,
                    position=cycle(lambda q,n=n:[500+35*math.sin(TAU*q+n*TAU/3),
                                                       500+35*math.cos(TAU*q+n*TAU/3),0])))
        elif mode==2:
            for n in range(8):
                side=n//2;shift=(-70 if n%2 else 70)
                ls.append(layer(f'Assembly segment {n}',path([[-55,0],[55,0]]),4,1,3.7,
                    rotation=static(side*90),
                    position=cycle(lambda q,side=side,shift=shift,n=n:
                        [500+math.cos(side*math.pi/2)*shift-math.sin(side*math.pi/2)*(160+35*math.sin(TAU*q+n*.5)),
                         500+math.sin(side*math.pi/2)*shift+math.cos(side*math.pi/2)*(160+35*math.sin(TAU*q+n*.5)),0])))
        elif mode==3:
            for n in range(4):
                ls.append(layer(f'Diamond leaf {n}',rectangle(48,0),4,1,3.4,rotation=static(45),
                    position=cycle(lambda q,n=n:point(95+65*(1-math.cos(TAU*q))/2,n*90))))
            ls.append(layer('Center diamond',rectangle(27),4,4,2,rotation=static(45)))
        elif mode==4:
            for n in range(5):
                ls.append(layer(f'Ladder rung {n}',path([[-130,0],[130,0]]),4,1,3.8,
                    position=cycle(lambda q,n=n:[500+25*math.sin(TAU*q+n*.7),500+(n-2)*65,0]),
                    size=cycle(lambda q,n=n:[75+25*math.cos(TAU*q+n*.65),100,100])))
        elif mode==5:
            for n in range(4):
                pts=[]
                perimeter=[[-175,-175],[175,-175],[175,175],[-175,175],[-175,-175]]
                for k in range(5):
                    x,y=perimeter[(k+n)%4];pts.append((k,[500+x,500+y,0]))
                ls.append(layer(f'Chasing bracket {n}',path([[-25,0],[0,0],[0,25]]),4,1,4,
                    position=animated(pts,linear=True),rotation=static(n*90)))
        else:
            for n in range(9):
                x,y=(n%3-1)*115,(n//3-1)*100
                ls.append(layer(f'Wave rectangle {n}',rectangle(34,2),4,1,2.8,position=at(x,y),
                    size=cycle(lambda q,n=n:[100,55+45*(1+math.sin(TAU*q+n*.55))/2,100])))
        emit('Shapes','Squares',name,4,ls,desc[mode],[1,4] if mode in (1,3) else [1])


def shapes_triangles():
    names=['Delta_Stack','Triad_Orbit','Arrow_Gate','Triangle_Weave','Delta_Draw','Prism_Scissor','Three_Petals']
    desc=['Три треугольника ходят по вертикали в противофазе, образуя расслоенный знак.',
          'Три небольших треугольника обмениваются местами вокруг центральной оси.',
          'Два шеврона сходятся и расходятся, открывая направленный проход.',
          'Три контура растягиваются поперёк своих осей и переплетаются.',
          'Контур большого треугольника рисуется навстречу внутреннему контуру.',
          'Три пары рёбер раскрываются как ножницы вокруг вершин.',
          'Три треугольных лепестка сходятся в центральный знак и раскрываются по радиусам.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(3):
                ls.append(layer(f'Delta layer {n}',polygon(3,145),4,0,3,
                    position=cycle(lambda q,n=n:[500,500+(n-1)*40+35*math.sin(TAU*q+n*2),0])))
        elif mode==1:
            for n in range(3):
                ls.append(layer(f'Orbit delta {n}',polygon(3,52),4,0,3.4,
                    position=cycle(lambda q,n=n:point(155,n*120+q*360)),rotation=spin(n*120,n*120-360)))
            ls.append(layer('Triad center',polygon(3,32),4,4,2.3))
        elif mode==2:
            for n in range(2):
                side=-1 if n else 1
                ls.append(layer(f'Arrow gate {n}',path([[-90,65],[0,-80],[90,65]]),4,0,4,
                    rotation=static(n*180),position=cycle(lambda q,side=side:[500,500+side*(55+70*(1-math.cos(TAU*q))/2),0])))
        elif mode==3:
            for n in range(3):
                ls.append(layer(f'Woven delta {n}',polygon(3,185),4,0,2.7,rotation=static(n*120),
                    size=cycle(lambda q,n=n:[65+30*math.sin(TAU*q+n*TAU/3),85,100])))
        elif mode==4:
            for n,r in enumerate((225,138)):
                ls.append(layer(f'Drawn delta {n}',polygon(3,r),4,0 if not n else 4,3.6,
                    rotation=static(n*180),draw=trim(0,animated([(0,18 if not n else 85),(2,85 if not n else 18),(4,18 if not n else 85)]))))
        elif mode==5:
            for n in range(3):
                for side in (-1,1):
                    ls.append(layer(f'Scissor {n}-{side}',path([[0,0],[95,0]]),4,0,3.5,
                        position=static(point(135,n*120-90)),
                        rotation=animated([(0,n*120+side*20+90),(2,n*120+side*65+90),(4,n*120+side*20+90)])))
        else:
            for n in range(3):
                ls.append(layer(f'Delta petal {n}',polygon(3,75),4,0,3.2,
                    position=cycle(lambda q,n=n:point(75+70*(1-math.cos(TAU*q))/2,n*120-90)),rotation=static(n*120)))
        emit('Shapes','Triangles',name,4,ls,desc[mode],[0,4] if mode in (1,4) else [0])


def shapes_hexagons():
    names=['Hex_Iris','Cell_Orbit','Facet_Pulse','Hex_Lattice','Segmented_Crown','Hex_Braid','Honeycomb_Draw']
    desc=['Шесть угловых створок сходятся к центру и раскрываются, как шестигранная диафрагма.',
          'Три малые ячейки проходят по общей орбите вокруг крупной центральной соты.',
          'Короткие рёбра передают масштабный импульс по очереди вокруг оболочки.',
          'Семь ячеек медленно раздвигаются и снова сцепляются в компактную соту.',
          'Шесть двойных граней поворачиваются в разные стороны вокруг центрального просвета.',
          'Два сжатых шестигранника проходят друг сквозь друга по встречным траекториям.',
          'Центральная ячейка и шесть внешних углов рисуются с волной задержки.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(6):
                ls.append(layer(f'Iris facet {n}',path([[-38,20],[0,0],[38,20]]),4,2,3.6,
                    position=cycle(lambda q,n=n:point(120+65*(1-math.cos(TAU*q))/2,n*60-90)),rotation=static(n*60)))
        elif mode==1:
            ls.append(layer('Core cell',polygon(6,115,-30),4,2,3))
            for n in range(3):
                ls.append(layer(f'Orbit cell {n}',polygon(6,30,-30),4,3,2.8,
                    position=cycle(lambda q,n=n:point(205,n*120+360*q)),rotation=spin(0,-360)))
        elif mode==2:
            for n in range(6):
                ls.append(layer(f'Pulse facet {n}',path([[-64,0],[64,0]]),4,2,4,
                    position=static(point(170,n*60-90)),rotation=static(n*60),
                    size=cycle(lambda q,n=n:[70+30*math.sin(TAU*q-n*TAU/6),100,100])))
        elif mode==3:
            for n in range(7):
                ls.append(layer(f'Lattice cell {n}',polygon(6,51,-30),4,2,2.5,
                    position=at(0,0) if n==0 else cycle(lambda q,n=n:point(95+25*(1-math.cos(TAU*q))/2,(n-1)*60-30))))
        elif mode==4:
            for n in range(6):
                ls.append(layer(f'Crown {n}',path([[-52,30],[0,0],[52,30]]),4,2,3,
                    position=static(point(180,n*60-90)),
                    rotation=animated([(0,n*60-25),(2,n*60+25),(4,n*60-25)])))
        elif mode==5:
            for n in range(2):
                ls.append(layer(f'Braid shell {n}',polygon(6,160,-30),4,2 if not n else 3,3,
                    position=cycle(lambda q,n=n:[500+50*math.sin(TAU*q+n*math.pi),500+30*math.cos(TAU*q+n*math.pi),0]),
                    size=cycle(lambda q,n=n:[75,65+30*math.sin(TAU*q+n*math.pi),100]),rotation=static(n*90)))
        else:
            ls.append(layer('Drawn cell',polygon(6,95,-30),4,3,3,draw=trim(0,animated([(0,30),(2,100),(4,30)]))))
            for n in range(6):
                ls.append(layer(f'Drawn corner {n}',path([[-38,25],[0,0],[38,25]]),4,2,3,
                    position=static(point(195,n*60-90)),rotation=static(n*60),
                    draw=trim(0,cycle(lambda q,n=n:[50+45*math.sin(TAU*q-n*TAU/6)]))))
        emit('Shapes','Hexagons',name,4,ls,desc[mode],[2,3] if mode in (1,5,6) else [2])


def flight(name,form,color,position,rotation=None,size=None,delay=0,end=1.9,width=3.2,draw=None):
    return layer(name,form,2,color,width,opacity=pulse(delay,end),position=position,
                 rotation=rotation,size=size,draw=draw)


def travel(samples):return animated([(t,p) for t,p in samples],linear=False)


def shots_circles():
    names=['Double_Shock','Arc_Spray','Ring_Drop','Halo_Fracture','Bubble_Thread','Circle_Pair_Burst','Expanding_Crescents']
    desc=['Два встречных кольцевых удара: короткое сжатие и быстрое расширение.',
          'Восемь коротких дуг выстреливают веером с вращением.',
          'Четыре кольца падают вниз из центра по расходящимся траекториям.',
          'Кольцо разламывается на четыре больших четверти, которые покидают центр.',
          'Шесть малых колец вытягиваются в две последовательные изогнутые нити.',
          'Три пары колец сначала расходятся поперёк, затем ускоряются к краям.',
          'Три растущих серпа последовательно разворачиваются и уходят из кадра.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(2):
                ls.append(flight(f'Shock {n}',ellipse(120),3,at(0,0),delay=n*.18,
                    size=scale([(0,32),(n*.18,32),(.25+n*.18,20),(.65+n*.18,180),(1.9,780),(2,780)]),width=2.2))
        elif mode==1:
            for n in range(8):
                a=-160+n*40
                ls.append(flight(f'Arc spray {n}',ellipse(48),3,
                    travel([(0,point(10,a)),(.5,point(180,a)),(1.9,point(940,a-22)),(2,point(980,a-22))]),
                    rotation=spin(a,a+150,2),size=scale([(0,15),(.7,100),(2,200)]),draw=trim(0,20),delay=n*.015))
        elif mode==2:
            for n in range(4):
                side=n-1.5
                ls.append(flight(f'Falling ring {n}',ellipse(38+n%2*12),3,
                    travel([(0,[500,500,0]),(.4,[500+side*65,430,0]),(1.2,[500+side*180,730,0]),(2,[500+side*300,1380,0])]),
                    size=scale([(0,10),(.5,100),(2,240)]),delay=n*.08))
        elif mode==3:
            for n in range(4):
                ls.append(flight(f'Quarter {n}',ellipse(160),3,
                    travel([(0,point(0,n*90)),(.4,point(35,n*90)),(1.85,point(800,n*90+30)),(2,point(900,n*90+30))]),
                    rotation=animated([(0,n*90),(1.85,n*90+80),(2,n*90+80)]),draw=trim(0,23)))
        elif mode==4:
            for n in range(6):
                side=-1 if n%2 else 1;delay=(n//2)*.15
                ls.append(flight(f'Thread bubble {n}',ellipse(22),3,
                    travel([(0,[500,500,0]),(delay,[500,500,0]),(.6+delay,[500+side*130,430-n*15,0]),
                            (1.8,[500+side*780,300+n*50,0]),(2,[500+side*900,300+n*50,0])]),
                    size=scale([(0,20),(.6,100),(2,220)]),delay=delay))
        elif mode==5:
            for n in range(6):
                a=n//2*120;side=-1 if n%2 else 1
                ls.append(flight(f'Paired circle {n}',ellipse(45),3,
                    travel([(0,point(0,a)),(.35,point(65,a+side*40)),(.85,point(260,a+side*20)),(2,point(940,a+side*15))]),
                    size=scale([(0,8),(.5,100),(2,220)]),delay=n//2*.08))
        else:
            for n in range(3):
                ls.append(flight(f'Crescent {n}',ellipse(140),3,at(0,0),delay=n*.17,
                    rotation=spin(n*120,n*120+150,2),size=scale([(0,5),(n*.17,5),(.5+n*.17,140),(1.9,680),(2,680)]),
                    draw=trim(0,35),width=2.5))
        emit('One_Shot','Circles',name,2,ls,desc[mode],[3])


def shots_squares():
    names=['Box_Flip','Bracket_Stream','Frame_Rupture','Tile_Rain','Square_Zipper','Diamond_Spray','Gate_Launch']
    desc=['Две квадратные оболочки раскрываются через сплющивание по горизонтали и уходят в края.',
          'Три пары скоб протягивают последовательный боковой проход.',
          'Восемь отдельных рёбер рамки срываются по нормалям, сохраняя квадратный рисунок старта.',
          'Шесть малых плиток взлетают и падают веером.',
          'Две зубчатые цепочки рамок расходятся как раскрывающаяся застёжка.',
          'Четыре ромба вылетают по диагоналям с поперечным покачиванием.',
          'Три прямоугольных портала раскрываются в широкий проезд и ускоряются к зрителю.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(2):
                ls.append(flight(f'Flipping box {n}',rectangle(130,2),1,at(0,0),delay=n*.16,
                    rotation=spin(-12+n*24,35-n*70,2),
                    size=animated([(0,[8,8,100]),(.3,[110,95,100]),(.65,[12,180,100]),(1.1,[290,230,100]),(2,[750,650,100])]),width=2.5))
        elif mode==1:
            for n in range(6):
                side=-1 if n%2 else 1;delay=n//2*.16
                ls.append(flight(f'Bracket stream {n}',path([[-35,-65],[0,-65],[0,65],[-35,65]]),1,
                    travel([(0,[500,500,0]),(delay,[500,500,0]),(.5+delay,[500+side*180,500,0]),(2,[500+side*1000,500,0])]),
                    rotation=static(0 if side>0 else 180),delay=delay,size=scale([(0,10),(.6,100),(2,240)])))
        elif mode==2:
            for n in range(8):
                a=n//2*90;shift=-65 if n%2 else 65
                ls.append(flight(f'Ruptured edge {n}',path([[-48,0],[48,0]]),1,
                    travel([(0,point(110,a)),(.3,point(150,a)),(1.85,point(900,a+(15 if n%2 else -15))),(2,point(1000,a))]),
                    rotation=animated([(0,a+90),(.4,a+90),(2,a+90+shift)]),delay=n*.015))
        elif mode==3:
            for n in range(6):
                x=n-2.5
                ls.append(flight(f'Tile rain {n}',rectangle(25,1),1,
                    travel([(0,[500,500,0]),(.4,[500+x*48,380-abs(x)*12,0]),(1,[500+x*125,570,0]),(2,[500+x*230,1260,0])]),
                    rotation=spin(0,x*95,2),size=scale([(0,12),(.6,100),(2,185)]),delay=n*.025))
        elif mode==4:
            for n in range(8):
                side=-1 if n%2 else 1;delay=n//2*.12
                ls.append(flight(f'Zipper tooth {n}',rectangle(22,1),1,
                    travel([(0,[500,500,0]),(delay,[500,500,0]),(.6+delay,[500+side*120,400+n//2*65,0]),(2,[500+side*830,400+n//2*90,0])]),
                    rotation=spin(0,side*70,2),delay=delay))
        elif mode==5:
            for n in range(4):
                a=45+n*90
                ls.append(flight(f'Diamond spray {n}',rectangle(37),1,
                    travel([(0,point(0,a)),(.5,point(150,a+18)),(1.2,point(430,a-10)),(2,point(980,a))]),
                    rotation=animated([(0,45),(.5,80),(1.2,15),(2,45)]),size=scale([(0,5),(.6,100),(2,200)])))
        else:
            for n in range(3):
                delay=n*.19
                ls.append(flight(f'Launched gate {n}',rectangle(100,2),1,at(0,0),delay=delay,
                    size=animated([(0,[3,3,100]),(delay,[3,3,100]),(.5+delay,[160,75,100]),(1.85,[900,440,100]),(2,[950,480,100])]),width=2))
        emit('One_Shot','Squares',name,2,ls,desc[mode],[1])


def shots_triangles():
    names=['Delta_Fanout','Arrow_Chain','Needle_Burst','Prism_Spinout','Wing_Split','Triad_Wave','Shard_Cascade']
    desc=['Пять длинных треугольных контуров раскрываются верхним веером и разгоняются.',
          'Три последовательных стрелы проходят через центр по восходящей диагонали.',
          'Три острых иглы растягиваются по направлению движения от центра.',
          'Три треугольника сначала закручиваются рядом с центром, затем вылетают по касательной.',
          'Парные стороны треугольника раскрываются крыльями и разлетаются влево и вправо.',
          'Три дуги из открытых треугольных шевронов расходятся волной по радиусам.',
          'Шесть коротких треугольных осколков с задержкой вылетают вниз по ступенчатой дуге.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(5):
                a=-150+n*30
                ls.append(flight(f'Fan delta {n}',polygon(3,55),0,
                    travel([(0,point(0,a)),(.35,point(85,a)),(1.1,point(400,a)),(2,point(1000,a))]),
                    rotation=static(a+90),size=animated([(0,[10,10,100]),(.5,[70,130,100]),(2,[150,280,100])]),delay=abs(n-2)*.03))
        elif mode==1:
            for n in range(3):
                delay=n*.22
                ls.append(flight(f'Chain arrow {n}',path([[-45,30],[0,-35],[45,30]]),0,
                    travel([(0,[470,530,0]),(delay,[470,530,0]),(.6+delay,[650,350,0]),(2,[1200,-200,0])]),
                    rotation=static(45),size=scale([(0,15),(.7,100),(2,250)]),delay=delay))
        elif mode==2:
            for n in range(3):
                a=-90+n*120
                ls.append(flight(f'Needle {n}',path([[-8,32],[0,-70],[8,32],[-8,32]]),0,
                    travel([(0,point(10,a)),(.4,point(110,a)),(2,point(1100,a))]),rotation=static(n*120),
                    size=animated([(0,[20,10,100]),(.6,[100,150,100]),(2,[120,330,100])]),width=2.8))
        elif mode==3:
            for n in range(3):
                pts=[]
                for k in range(17):
                    q=k/16;pts.append((q*2,point(20+950*q**2,n*120-90+q*220)))
                ls.append(flight(f'Spinout prism {n}',polygon(3,45),0,travel(pts),rotation=spin(n*120,n*120+420,2),
                                 size=scale([(0,15),(.6,100),(2,200)])))
        elif mode==4:
            for n in range(4):
                side=-1 if n%2 else 1;row=n//2
                ls.append(flight(f'Wing {n}',path([[0,-65],[0,65]]),0,
                    travel([(0,[500+side*35,500,0]),(.45,[500+side*120,500+row*50,0]),(2,[500+side*950,500+row*230,0])]),
                    rotation=animated([(0,side*30),(.5,side*75),(2,side*115)]),delay=row*.15))
        elif mode==5:
            for n in range(6):
                a=n//2*120-90;delay=n%2*.18
                ls.append(flight(f'Triad wave {n}',path([[-55,28],[0,-35],[55,28]]),0,
                    travel([(0,point(0,a)),(delay,point(0,a)),(.6+delay,point(210,a)),(2,point(1050,a))]),
                    rotation=static(a+90),size=scale([(0,5),(.5,100),(2,240)]),delay=delay))
        else:
            for n in range(6):
                a=20+n*28
                ls.append(flight(f'Cascade shard {n}',polygon(3,27),0,
                    travel([(0,[500,500,0]),(.4,point(100,a-35)),(1.1,point(400,a)),(2,point(1000,a+15))]),
                    rotation=spin(n*20,n*20+220,2),delay=n*.045,size=scale([(0,10),(.7,100),(2,190)])))
        emit('One_Shot','Triangles',name,2,ls,desc[mode],[0])


def shots_hexagons():
    names=['Cell_Burst','Facet_Streaks','Hex_Pulse_Chain','Honeycomb_Spread','Hex_Spiral','Radial_Cells','Facet_Peel']
    desc=['Шесть сот с коротким сжатием разлетаются по прямым радиусам.',
          'Двенадцать коротких граней расходятся парами вдоль шести осей.',
          'Три шестигранных импульса проходят по диагональной цепочке, не образуя тоннель.',
          'Два ряда ячеек разворачивают соты в горизонтальную полосу и расходятся.',
          'Четыре шестигранника идут по широкому спиральному разлёту с контрвращением.',
          'Шесть узких открытых граней вытягиваются в ячейки по мере удаления от центра.',
          'Три пары соседних рёбер отделяются от оболочки и разворачиваются как скобы.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(6):
                a=n*60-30
                ls.append(flight(f'Burst cell {n}',polygon(6,40,-30),2,
                    travel([(0,point(20,a)),(.15,point(10,a)),(.6,point(185,a)),(2,point(1000,a))]),
                    size=scale([(0,20),(.5,100),(2,230)])))
        elif mode==1:
            for n in range(12):
                a=n//2*60-90;side=-1 if n%2 else 1
                ls.append(flight(f'Facet streak {n}',path([[-22,0],[22,0]]),2,
                    travel([(0,point(0,a)),(.4,point(100,a+side*8)),(2,point(1000,a+side*12))]),
                    rotation=static(a),size=animated([(0,[5,100,100]),(.6,[140,100,100]),(2,[300,100,100])]),delay=n//2*.025))
        elif mode==2:
            for n in range(3):
                delay=n*.2
                ls.append(flight(f'Pulse chain {n}',polygon(6,85,-30),2,
                    travel([(0,[500,500,0]),(delay,[500,500,0]),(.7+delay,[500+(n-1)*130,500-(n-1)*100,0]),
                            (2,[500+(n-1)*800,500-(n-1)*650,0])]),delay=delay,
                    size=scale([(0,2),(delay,2),(.65+delay,120),(2,260)])))
        elif mode==3:
            for n in range(6):
                x=n%3-1;side=-1 if n//3 else 1
                ls.append(flight(f'Spreading cell {n}',polygon(6,50,-30),2,
                    travel([(0,[500,500,0]),(.55,[500+x*110,500+side*65,0]),(2,[500+x*700,500+side*620,0])]),
                    rotation=animated([(0,0),(.6,0),(2,side*35)]),size=scale([(0,10),(.6,100),(2,180)]),delay=abs(x)*.06))
        elif mode==4:
            for n in range(4):
                pts=[(q*2,point(20+900*q*q,n*90+q*120)) for q in [k/16 for k in range(17)]]
                ls.append(flight(f'Spiral cell {n}',polygon(6,34,-30),2,travel(pts),
                    rotation=spin(n*30,n*30-180,2),size=scale([(0,10),(.5,100),(2,250)])))
        elif mode==5:
            for n in range(6):
                a=n*60-90
                ls.append(flight(f'Opening cell {n}',polygon(6,55,-30),2,
                    travel([(0,point(30,a)),(.6,point(190,a)),(2,point(1050,a))]),rotation=static(n*60),
                    size=animated([(0,[5,5,100]),(.45,[35,110,100]),(1.2,[100,190,100]),(2,[170,260,100])]),
                    draw=trim(0,animated([(0,18),(.8,100),(2,100)]))))
        else:
            for n in range(3):
                a=n*120-90
                ls.append(flight(f'Peeling facet {n}',path([[-70,30],[0,0],[70,30]]),2,
                    travel([(0,point(100,a)),(.4,point(140,a)),(2,point(950,a+35))]),
                    rotation=spin(n*120,n*120+140,2),size=scale([(0,60),(.4,100),(2,190)]),delay=n*.08))
        emit('One_Shot','Hexagons',name,2,ls,desc[mode],[2])


def shots_lines():
    names=['Pinwheel_Stroke','V_Rockets','Staggered_Rays','Line_Zipper','Bent_Burst','Parallel_Flash','Comet_Fan']
    desc=['Шесть штрихов закручивают короткую вертушку и улетают по касательным.',
          'Четыре открытых V-образных следа ускоряются вдоль диагоналей.',
          'Двенадцать лучей вылетают волной задержки по окружности.',
          'Восемь поперечных штрихов расходятся от центральной вертикали двумя рядами.',
          'Четыре ломаных следа разгибаются и вытягиваются к краям.',
          'Три длинных параллельных штриха проходят через центр в разные стороны.',
          'Семь коротких следов образуют направленный изогнутый веер.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(6):
                a=n*60
                ls.append(flight(f'Pinwheel {n}',path([[-50,0],[50,0]]),3,
                    travel([(0,point(30,a)),(.45,point(100,a+50)),(1.1,point(350,a+85)),(2,point(1000,a+100))]),
                    rotation=spin(a+60,a+200,2),size=scale([(0,5),(.6,100),(2,210)])))
        elif mode==1:
            for n in range(4):
                a=45+n*90
                ls.append(flight(f'V rocket {n}',path([[-25,25],[0,-40],[25,25]]),3,
                    travel([(0,point(0,a)),(.5,point(150,a)),(2,point(1100,a))]),rotation=static(a+90),
                    size=animated([(0,[10,10,100]),(.6,[100,140,100]),(2,[160,280,100])]),delay=n*.035))
        elif mode==2:
            for n in range(12):
                a=n*30;delay=n*.035
                ls.append(flight(f'Delayed ray {n}',path([[0,0],[0,-70]]),3,
                    travel([(0,point(0,a)),(delay,point(0,a)),(.55+delay,point(190,a)),(2,point(1150,a))]),
                    rotation=static(a+90),delay=delay,size=scale([(0,5),(.7,100),(2,240)])))
        elif mode==3:
            for n in range(8):
                side=-1 if n%2 else 1;row=n//2;delay=row*.11
                ls.append(flight(f'Zipper line {n}',path([[-45,0],[45,0]]),3,
                    travel([(0,[500,500,0]),(delay,[500,500,0]),(.6+delay,[500+side*180,400+row*65,0]),
                            (2,[500+side*1100,350+row*95,0])]),delay=delay))
        elif mode==4:
            for n in range(4):
                form=morph([(0,[[-40,40],[0,-30],[40,40]]),(.7,[[-65,0],[0,0],[65,0]]),(2,[[-100,0],[0,0],[100,0]])])
                ls.append(flight(f'Bent stroke {n}',form,3,
                    travel([(0,point(0,n*90)),(.5,point(135,n*90)),(2,point(1050,n*90))]),rotation=static(n*90)))
        elif mode==5:
            for n in range(3):
                side=-1 if n==1 else 1;y=(n-1)*75
                ls.append(flight(f'Parallel {n}',path([[-170,0],[170,0]]),3,
                    travel([(0,[500,500+y,0]),(.4,[500+side*100,500+y,0]),(2,[500+side*1250,500+y,0])]),
                    delay=n*.1,size=animated([(0,[2,100,100]),(.45,[100,100,100]),(2,[170,100,100])]),width=3))
        else:
            for n in range(7):
                a=-170+n*22
                ls.append(flight(f'Fan comet {n}',path([[-30,0],[30,0]]),3,
                    travel([(0,point(0,a)),(.5,point(110,a+10)),(1.1,point(350,a+22)),(2,point(1100,a+35))]),
                    rotation=spin(a+20,a+60,2),size=scale([(0,10),(.6,100),(2,230)]),delay=n*.02))
        emit('One_Shot','Lines',name,2,ls,desc[mode],[3])


def shots_crosses():
    names=['Cross_Twist','Plus_Arms','Checker_Release','Cross_Waves','Cross_Compass','Split_Plus','Cross_Chain']
    desc=['Один крупный крест закручивается, сплющивается и расширяется за края.',
          'Четыре плеча плюса отделяются от центра и уходят по осям.',
          'Пять крестов строят шахматную диагональ и разъезжаются по разным траекториям.',
          'Две группы по четыре креста расходятся с чередованием радиуса и угла.',
          'Восемь малых крестов расходятся по направлениям компаса, сохраняя ориентацию.',
          'Два длинных штриха сначала образуют плюс, затем разворачиваются и разлетаются.',
          'Пять крестов последовательно проходят по S-образному маршруту из центра.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            ls.append(flight('Twisting cross',geometry('cross',125),2,at(0,0),rotation=spin(0,235,2),
                size=animated([(0,[8,8,100]),(.4,[100,100,100]),(.9,[180,60,100]),(2,[780,780,100])]),width=2.6))
        elif mode==1:
            for n in range(4):
                a=n*90
                ls.append(flight(f'Plus arm {n}',path([[0,0],[90,0]]),2,
                    travel([(0,point(0,a)),(.4,point(100,a)),(2,point(1150,a))]),rotation=static(a),
                    size=animated([(0,[15,100,100]),(.5,[100,100,100]),(2,[230,100,100])])) )
        elif mode==2:
            for n in range(5):
                x=(n-2)*60;y=(-1 if n%2 else 1)*60
                ls.append(flight(f'Checker cross {n}',geometry('cross',22),2,
                    travel([(0,[500,500,0]),(.4,[500+x,500+y,0]),(1.15,[500+x*2.5,500-y*2,0]),(2,[500+x*7,500+y*8,0])]),
                    rotation=spin(0,(n-2)*95,2),size=scale([(0,10),(.4,100),(2,180)]),delay=n*.035))
        elif mode==3:
            for n in range(8):
                wave=n//4;a=n%4*90+wave*45;delay=wave*.23
                ls.append(flight(f'Wave cross {n}',geometry('cross',24),2,
                    travel([(0,point(0,a)),(delay,point(0,a)),(.65+delay,point(230,a)),(2,point(1050,a))]),
                    rotation=spin(wave*45,wave*45+90,2),delay=delay,size=scale([(0,5),(.5,100),(2,210)])))
        elif mode==4:
            for n in range(8):
                a=n*45
                ls.append(flight(f'Compass cross {n}',geometry('cross',15),2,
                    travel([(0,point(0,a)),(.5,point(140,a)),(2,point(1000,a))]),
                    rotation=static(0),size=scale([(0,5),(.5,100),(2,260)]),delay=(n%2)*.1))
        elif mode==5:
            for n in range(2):
                side=-1 if n else 1
                ls.append(flight(f'Split plus {n}',path([[-140,0],[140,0]]),2,
                    travel([(0,[500,500,0]),(.35,[500,500,0]),(1.1,[500+side*280,500+side*100,0]),(2,[500+side*1100,500+side*450,0])]),
                    rotation=animated([(0,n*90),(.35,n*90),(2,n*90+side*110)]),size=scale([(0,5),(.4,100),(2,200)])))
        else:
            for n in range(5):
                delay=n*.12
                ls.append(flight(f'Chain cross {n}',geometry('cross',22),2,
                    travel([(0,[500,500,0]),(delay,[500,500,0]),(.45+delay,[610,450,0]),
                            (1.1+delay*.3,[420,250,0]),(2,[900,-500,0])]),
                    rotation=spin(0,200,2),delay=delay,size=scale([(0,10),(.7,100),(2,200)])))
        emit('One_Shot','Crosses',name,2,ls,desc[mode],[2])


def flow_glyph(name,form,color,fn,phase,rotation=0):
    pts=[];alphas=[]
    for k in range(81):
        t=k/10;q=((t+phase)%8)/8
        pts.append((t,fn(q)))
        alphas.append((t,max(0,min(1,q/.08,(1-q)/.08))*100))
    return layer(name,form,8,color,2.5,position=animated(pts,linear=True),
                 opacity=animated(alphas),rotation=static(rotation))


def bg_circles():
    names=['Slow_Precession','Ring_Archipelago','Vertical_Orbits','Twin_Lenses','Arc_Constellation','Ring_Current','Lunar_Lanes']
    desc=['Три большие незамкнутые эллиптические орбиты медленно меняют направление.',
          'Семь разрозненных колец мягко расширяются и сжимаются со сдвигом фаз.',
          'Четыре боковые вертикальные орбиты проводят маленькие спутники вдоль краёв.',
          'Две крупные линзы сдвигаются навстречу, оставляя просвет в центре.',
          'Восемь небольших дуг образуют медленно вращающееся периферийное созвездие.',
          'Боковой поток незамкнутых колец проходит сверху вниз с плавным переносом.',
          'Четыре дуговые дорожки скользят в противоположных направлениях вокруг центра.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(3):
                ls.append(layer(f'Precessing orbit {n}',ellipse(260+n*60),8,2,2,
                    size=static([100,42+n*10,100]),rotation=animated([(0,n*60),(4,n*60+28),(8,n*60)]),draw=trim(0,70)))
        elif mode==1:
            for n in range(7):
                a=n*360/7
                ls.append(layer(f'Island ring {n}',ellipse(42+n%3*12),8,2,2.4,
                    position=static(point(300+n%2*65,a)),
                    size=cycle(lambda q,n=n:[80+20*math.sin(TAU*q+n),80+20*math.sin(TAU*q+n),100],8)))
        elif mode==2:
            for n in range(4):
                x=(-1 if n%2 else 1)*(260+n//2*100)
                ls.append(layer(f'Vertical lane {n}',ellipse(160),8,2,2,position=at(x,0),size=static([30,100,100]),draw=trim(0,75)))
                ls.append(layer(f'Vertical moon {n}',ellipse(6),8,2,2.5,
                    position=cycle(lambda q,n=n,x=x:[500+x+48*math.cos(TAU*q+n),500+160*math.sin(TAU*q+n),0],8)))
        elif mode==3:
            for side in (-1,1):
                ls.append(layer(f'Large lens {side}',ellipse(250),8,2,2.2,
                    position=cycle(lambda q,side=side:[500+side*(210+45*math.sin(TAU*q)),500,0],8),
                    size=static([55,120,100]),draw=trim(5,92)))
        elif mode==4:
            for n in range(8):
                ls.append(layer(f'Constellation arc {n}',ellipse(35+n%3*12),8,2,2.5,
                    position=cycle(lambda q,n=n:point(290+n%2*90,n*45+q*22-11*math.sin(TAU*q)),8),
                    rotation=animated([(0,n*45),(4,n*45+55),(8,n*45)]),draw=trim(0,55)))
            # Positions must return exactly: use a periodic orbital wobble.
            for n,l in enumerate(ls):
                l['ks']['p']=cycle(lambda q,n=n:point(290+n%2*90,n*45+14*math.sin(TAU*q)),8)
        elif mode==5:
            for n in range(8):
                side=-1 if n%2 else 1
                ls.append(flow_glyph(f'Current ring {n}',ellipse(37+n%3*8),2,
                    lambda q,n=n,side=side:[500+side*(280+35*math.sin(TAU*q+n)), -90+1180*q,0],n,rotation=n*30))
                ls[-1]['shapes'].append(trim(0,76))
        else:
            for n in range(4):
                ls.append(layer(f'Lunar lane {n}',ellipse(255+n*45),8,2,2.1,
                    rotation=spin(n*45,n*45+(360 if n%2 else -360),8),
                    size=static([100,70,100]),draw=trim(n*8,n*8+28)))
        emit('BG','Circles',name,8,ls,desc[mode]+' Пиковая прозрачность 100%.',[2])


def bg_geometry():
    names=['Falling_Diamonds','Zigzag_Drifters','Breathing_Cells','Passing_Windows','Tilted_Grid','Loose_Fragments','Polygon_Orbits']
    desc=['Редкие ромбы плавно падают двумя боковыми потоками.',
          'Малые угловые знаки движутся по разным ломаным периферийным маршрутам.',
          'Небольшие группы сот медленно раздвигаются и собираются по краям.',
          'Узкие прямоугольные окна проходят по горизонтали в противоположных направлениях.',
          'Разреженная наклонная сетка из контурных плиток мягко качается.',
          'Короткие фрагменты многоугольников медленно вращаются в отдельных областях.',
          'Малые квадраты и треугольники проходят по широким эллиптическим орбитам.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(10):
                side=-1 if n%2 else 1
                ls.append(flow_glyph(f'Falling diamond {n}',rectangle(13+n%3*4),2,
                    lambda q,n=n,side=side:[500+side*(220+n%3*85)+20*math.sin(TAU*q),-70+1140*q,0],n*.8,45))
        elif mode==1:
            for n in range(8):
                side=-1 if n%2 else 1;x=side*(240+n%3*70);y=(n//2-1.5)*190
                ls.append(layer(f'Zigzag glyph {n}',path([[-13,10],[0,-10],[13,10]]),8,2,2.4,
                    position=animated([(0,[500+x,500+y,0]),(2,[500+x+side*38,500+y-24,0]),
                        (4,[500+x,500+y-48,0]),(6,[500+x-side*25,500+y-20,0]),(8,[500+x,500+y,0])]),rotation=static(n*45)))
        elif mode==2:
            for n in range(6):
                side=-1 if n%2 else 1;row=n//2
                ls.append(layer(f'Breathing cell {n}',polygon(6,32,-30),8,2,2.5,
                    position=at(side*310,(row-1)*220),size=cycle(lambda q,n=n:[80+20*math.sin(TAU*q+n),80+20*math.sin(TAU*q+n),100],8)))
        elif mode==3:
            for n in range(8):
                side=-1 if n%2 else 1;y=(-1 if n//2%2 else 1)*(220+n%3*50)
                ls.append(flow_glyph(f'Window {n}',rectangle(28,2),2,
                    lambda q,y=y,side=side:[-100+1200*(q if side==1 else 1-q),500+y,0],n,
                    rotation=side*6))
                ls[-1]['ks']['s']=static([65,100,100])
        elif mode==4:
            for n in range(12):
                x=(n%4-1.5)*230;y=(n//4-1)*290
                if abs(x)<180 and abs(y)<150:continue
                ls.append(layer(f'Tilted tile {n}',rectangle(22),8,2,2.1,position=at(x,y),
                    rotation=animated([(0,18),(4,32),(8,18)])))
        elif mode==5:
            for n in range(10):
                a=n*36;r=260+n%3*70
                ls.append(layer(f'Loose fragment {n}',path([[-17,9],[0,-10],[17,9]]),8,2 if n%3 else 4,2.5,
                    position=static(point(r,a)),rotation=spin(n*25,n*25+(360 if n%2 else -360),8)))
        else:
            for n in range(8):
                a=n*TAU/8
                ls.append(layer(f'Orbital glyph {n}',geometry('triangle' if n%2 else 'square',17),8,2,2.5,
                    position=cycle(lambda q,a=a:[500+380*math.cos(TAU*q+a),500+270*math.sin(TAU*q+a),0],8),
                    rotation=spin(0,180 if not n%2 else -360,8)))
        emit('BG','Geometry',name,8,ls,desc[mode]+' Пиковая прозрачность 100%.',[2,4] if mode==5 else [2])


def bg_lines():
    names=['Parallel_Current','Sine_Horizon','Diagonal_Shuttles','Peripheral_Rays','Ribbon_Lanes','Corner_Streams','Tension_Lines']
    desc=['Шесть коротких параллельных следов проходят по краям в двух направлениях.',
          'Три длинные волны колеблются поперёк горизонта с разной фазой.',
          'Восемь диагональных штрихов скользят между боковыми областями.',
          'Двенадцать коротких внешних лучей медленно качаются вокруг свободного центра.',
          'Четыре тонкие ленты меняют кривизну, не заполняя весь кадр.',
          'Четыре потока коротких линий уходят из углов к периферийным границам.',
          'Шесть натянутых нитей плавно меняют наклон и степень изгиба.']
    for mode,name in enumerate(names):
        ls=[]
        if mode==0:
            for n in range(6):
                side=-1 if n%2 else 1;y=(n//2-1)*290
                ls.append(flow_glyph(f'Parallel current {n}',path([[-55,0],[55,0]]),3,
                    lambda q,side=side,y=y:[-100+1200*(q if side==1 else 1-q),500+y,0],n*8/6))
        elif mode==1:
            for n in range(3):
                states=[]
                for k in range(9):
                    vertices=[[x, (n-1)*280+45*math.sin(x/600*math.pi+k*TAU/8+n)] for x in range(-600,601,50)]
                    states.append((k,vertices))
                ls.append(layer(f'Sine horizon {n}',morph(states),8,3,2.4))
        elif mode==2:
            for n in range(8):
                side=-1 if n%2 else 1;y=(n//2-1.5)*220
                ls.append(layer(f'Diagonal shuttle {n}',path([[-28,0],[28,0]]),8,3,2.6,rotation=static(side*45),
                    position=cycle(lambda q,n=n,side=side,y=y:[500+side*(270+70*math.sin(TAU*q+n)),500+y+30*math.cos(TAU*q+n),0],8)))
        elif mode==3:
            for n in range(12):
                ls.append(layer(f'Peripheral ray {n}',path([[300,0],[360+n%3*20,0]]),8,3,2.1,
                    rotation=animated([(0,n*30),(4,n*30+10),(8,n*30)])))
        elif mode==4:
            for n in range(4):
                states=[]
                for k in range(9):
                    states.append((k,[[x,(n-1.5)*210+75*math.sin(x/550*math.pi+k*TAU/8+n*.5)] for x in range(-550,551,55)]))
                ls.append(layer(f'Ribbon lane {n}',morph(states),8,3,2,rotation=static(14 if n%2 else -14),draw=trim(12,88)))
        elif mode==5:
            for n in range(8):
                a=45+n//2*90;phase=n%2*4
                pts=[];alpha=[]
                for k in range(81):
                    t=k/10;q=((t+phase)%8)/8
                    pts.append((t,point(260+500*q,a)))
                    alpha.append((t,max(0,min(1,q/.1,(1-q)/.1))*100))
                ls.append(layer(f'Corner stream {n}',path([[-24,0],[24,0]]),8,3,2.8,position=animated(pts,linear=True),
                    opacity=animated(alpha),rotation=static(a)))
        else:
            for n in range(6):
                states=[]
                for k in range(9):
                    bend=45*math.sin(k*TAU/8+n)
                    states.append((k,[[-500,0],[-250,bend],[0,bend*1.3],[250,bend],[500,0]]))
                ls.append(layer(f'Tension line {n}',morph(states),8,3,2,
                    position=at(0,(n-2.5)*150),rotation=static((n-2.5)*4)))
        emit('BG','Lines',name,8,ls,desc[mode]+' Пиковая прозрачность 100%.',[3])


def populate():
    MANIFEST.clear()
    shapes_circles();shapes_squares();shapes_triangles();shapes_hexagons()
    shots_circles();shots_squares();shots_triangles();shots_hexagons();shots_lines();shots_crosses()
    bg_circles();bg_geometry();bg_lines()
    assert len(MANIFEST)==91
    return list(MANIFEST)
