"""Additional authored motion studies; each folder gets its own movement language."""
import math
from create_centerline import (MANIFEST, animated, static, scale, layer, path, polygon,
                               rectangle, ellipse, geometry, trim, emit)


def point(r, degrees):
    a = math.radians(degrees)
    return [500 + r * math.cos(a), 500 + r * math.sin(a), 0]


def morph(states):
    def value(vertices):
        return {"i": [[0, 0] for _ in vertices], "o": [[0, 0] for _ in vertices],
                "v": vertices, "c": False}
    result = path(states[0][1])
    result["ks"] = animated([(t, value(v)) for t, v in states])
    return result


def pulse(delay=0, end=1.9):
    return animated([(0, 0), (delay, 0), (delay+.09, 100),
                     (max(delay+.10, end-.4), 100), (end, 0), (2, 0)])


def shapes():
    # CIRCLES: orbit planes and a mechanical iris. No polygon substitution.
    planes=[]
    for n in range(3):
        planes.append(layer(f"Orbit plane {n+1}", ellipse(194), 4, 3, 3.2,
            size=animated([(0,[100,25+n*14,100]),(2,[100,65-n*12,100]),(4,[100,25+n*14,100])]),
            rotation=animated([(0,n*60),(2,n*60+32),(4,n*60)])))
    planes.append(layer("Stationary hub",ellipse(26),4,4,3))
    emit("Shapes","Circles","Orbit_Planes",4,planes,
         "Три сплющенные орбитальные плоскости меняют наклон вокруг неподвижного центра.",[3,4])
    iris=[]
    for n in range(6):
        iris.append(layer(f"Iris blade {n+1}",ellipse(112),4,3,4,
            position=animated([(0,point(83,n*60)),(2,point(127,n*60+12)),(4,point(83,n*60))]),
            rotation=animated([(0,n*60),(2,n*60+36),(4,n*60)]),draw=trim(8,42)))
    iris.append(layer("Fine boundary",ellipse(248),4,4,2,100,draw=trim(0,78)))
    emit("Shapes","Circles","Iris_Bloom",4,iris,
         "Шесть дуговых лепестков раскрываются как диафрагма, сохраняя круговую границу.",[3,4])

    # SQUARES: four hinged corners; and a compact block lattice that shears.
    corners=[]
    for n in range(4):
        corners.append(layer(f"Hinged corner {n+1}",path([[-56,0],[0,0],[0,56]]),4,1,4,
            position=animated([(0,point(152,45+n*90)),(2,point(225,45+n*90)),(4,point(152,45+n*90))]),
            rotation=animated([(0,n*90),(2,n*90+38),(4,n*90)])))
    corners.append(layer("Inner axis",path([[-38,0],[38,0]]),4,4,2.5,
                         rotation=animated([(0,0),(2,90),(4,0)])))
    emit("Shapes","Squares","Corner_Hinges",4,corners,
         "Четыре отдельные угловые скобы раскрываются на шарнирах и собираются обратно.",[1,4])
    tiles=[]
    for row in range(3):
        for col in range(3):
            x,y=(col-1)*112,(row-1)*112
            tiles.append(layer(f"Sliding tile {row}-{col}",rectangle(37,2),4,1 if row!=1 else 4,2.8,
                position=animated([(0,[500+x,500+y,0]),(2,[500+x+(row-1)*38,500+y,0]),(4,[500+x,500+y,0])]),
                rotation=animated([(0,0),(2,(col-1)*10),(4,0)])))
    emit("Shapes","Squares","Sliding_Tiles",4,tiles,
         "Компактная сетка из девяти рамок сдвигает ряды навстречу друг другу.",[1,4])

    # TRIANGLES: three hinged edges and a fan of pointers.
    folding=[]
    for n in range(3):
        folding.append(layer(f"Folding edge {n+1}",path([[-115,0],[115,0]]),4,0,4,
            position=animated([(0,point(100,90+n*120)),(2,point(154,90+n*120)),(4,point(100,90+n*120))]),
            rotation=animated([(0,n*120),(2,n*120+48),(4,n*120)])))
    folding.append(layer("Inner triangle",polygon(3,65),4,4,2.5,
                         rotation=animated([(0,0),(2,-30),(4,0)])))
    emit("Shapes","Triangles","Tripod_Fold",4,folding,
         "Треугольник разбирается на три поворачивающихся ребра вокруг малого знака.",[0,4])
    fan=[]
    for n in range(5):
        fan.append(layer(f"Fan vane {n+1}",polygon(3,180),4,0,2.8,
            position=static([500,528,0]),
            rotation=animated([(0,(n-2)*7),(2,(n-2)*34),(4,(n-2)*7)]),
            size=scale([(0,100),(2,100-abs(n-2)*9),(4,100)])))
    fan.append(layer("Fan pivot",ellipse(5),4,4,fill=True,position=static([500,588,0])))
    emit("Shapes","Triangles","Prism_Fan",4,fan,
         "Пять треугольных контуров расходятся веером и снова сходятся, без тоннельного масштаба.",[0,4])

    # HEXAGONS: successive edge drawing, then six retracting radial petals.
    runners=[]
    for n in range(6):
        runners.append(layer(f"Edge runner {n+1}",polygon(6,214,-30),4,2,4,
            rotation=animated([(0,n*60),(4,n*60+360)],linear=True),draw=trim(0,10)))
    runners.append(layer("Reference hex",polygon(6,136,-30),4,4,2.2))
    emit("Shapes","Hexagons","Edge_Runners",4,runners,
         "Шесть коротких световых участков непрерывно бегут по шестигранной границе.",[2,4])
    petals=[]
    for n in range(6):
        petals.append(layer(f"Petal {n+1}",path([[-36,0],[0,-45],[36,0],[0,45],[-36,0]]),4,2,3.2,
            position=animated([(0,point(120,n*60-30)),(2,point(180,n*60-30)),(4,point(120,n*60-30))]),
            rotation=static(n*60+60)))
    petals.append(layer("Center hex",polygon(6,40,-30),4,3,3))
    emit("Shapes","Hexagons","Hex_Petals",4,petals,
         "Шесть ромбических лепестков раздвигаются по осям соты, сохраняя центральный шестигранник.",[2,3])


def shots():
    # Circle motion: curls launch tangentially; a spiral unwind uses open arcs.
    arcs=[]
    for n in range(6):
        a=n*60
        arcs.append(layer(f"Escaping arc {n+1}",ellipse(74),2,3,4,
            opacity=pulse(n*.025,1.85),draw=trim(0,36),
            position=animated([(0,point(0,a)),(.35,point(90,a)),(.95,point(310,a+36)),(1.8,point(760,a+65)),(2,point(850,a+65))]),
            rotation=animated([(0,a),(.8,a+80),(2,a+200)]),size=scale([(0,20),(.6,100),(2,180)])))
    emit("One_Shot","Circles","Orbit_Release",2,arcs,
         "Дуги сначала раскрывают круг, затем покидают центр по касательным дуговым траекториям.",[3])
    spiral=[]
    for n in range(3):
        verts=[]
        for k in range(41):
            q=k/40;r=14+240*q;a=math.radians(q*440+n*120)
            verts.append([r*math.cos(a),r*math.sin(a)])
        spiral.append(layer(f"Unwinding arm {n+1}",path(verts),2,3,2.4,opacity=pulse(n*.06,1.88),
            rotation=animated([(0,0),(2,100)]),size=scale([(0,12),(.65,100),(1.88,320),(2,320)]),
            draw=trim(0,animated([(0,0),(.35,42),(.9,100),(2,100)]))))
    emit("One_Shot","Circles","Spiral_Unwind",2,spiral,
         "Три открытые спирали рисуются из центра и раскручиваются наружу.",[3])

    brackets=[]
    for n in range(4):
        a=45+n*90
        brackets.append(layer(f"Peeling corner {n+1}",path([[-75,0],[0,0],[0,75]]),2,1,4,
            opacity=pulse(0,1.85),position=animated([(0,point(75,a)),(.25,point(120,a)),(.8,point(320,a)),(1.85,point(900,a)),(2,point(950,a))]),
            rotation=animated([(0,n*90),(.45,n*90),(1.85,n*90+70),(2,n*90+70)])))
    emit("One_Shot","Squares","Corner_Peel",2,brackets,
         "Квадрат распадается на четыре угловые скобы, которые разлетаются по диагоналям.",[1])
    split=[]
    for n,(dx,dy) in enumerate([(-1,-1),(1,-1),(-1,1),(1,1)]):
        split.append(layer(f"Ejected panel {n+1}",rectangle(55,2),2,1,3.2,
            opacity=pulse(n*.035,1.9),position=animated([(0,[500,500,0]),(.3,[500+dx*65,500+dy*65,0]),
                (.9,[500+dx*225,500+dy*80,0]),(1.8,[500+dx*760,500+dy*340,0]),(2,[500+dx*850,500+dy*450,0])]),
            size=animated([(0,[10,10,100]),(.3,[100,100,100]),(1.8,[220,40,100]),(2,[220,40,100])]),
            rotation=animated([(0,0),(.7,0),(1.8,dx*35),(2,dx*35)])))
    emit("One_Shot","Squares","Panel_Split",2,split,
         "Четыре квадратные панели расходятся горизонтальными парами и сплющиваются в полёте.",[1])

    chevrons=[]
    for n in range(3):
        a=-90+n*120
        for j in range(2):
            delay=j*.13
            chevrons.append(layer(f"Chevron {n}-{j}",path([[-40,30],[0,-32],[40,30]]),2,0,3.6,
                opacity=pulse(delay,1.8+delay*.5),rotation=static(n*120),
                position=animated([(0,point(8,a)),(delay,point(8,a)),(.4+delay,point(125,a)),
                                   (1.25+delay,point(590,a)),(2,point(950,a))]),
                size=scale([(0,15),(.5,100),(2,220)])))
    emit("One_Shot","Triangles","Chevron_Dash",2,chevrons,
         "Три направленных потока парных шевронов уходят от центра; это стрелы, а не кольца.",[0])
    shards=[]
    for n in range(3):
        a=n*120-90
        shards.append(layer(f"Triangle side {n+1}",path([[-100,0],[100,0]]),2,0,4,
            opacity=pulse(n*.08,1.9),rotation=animated([(0,n*120),(.4,n*120),(1.9,n*120+150),(2,n*120+150)]),
            position=animated([(0,point(45,a)),(.4,point(95,a)),(1.9,point(800,a+25)),(2,point(900,a+25))]),
            size=scale([(0,50),(.3,100),(1.9,175),(2,175)])))
    emit("One_Shot","Triangles","Prism_Shards",2,shards,
         "Треугольная вспышка разбирается на три длинных вращающихся ребра.",[0])

    edges=[]
    for n in range(6):
        a=n*60-90
        edges.append(layer(f"Hex side {n+1}",path([[-52,0],[52,0]]),2,2,3.8,
            opacity=pulse(n*.025,1.88),position=animated([(0,point(90,a)),(.35,point(135,a)),(1.8,point(850,a)),(2,point(950,a))]),
            rotation=animated([(0,n*60),(.4,n*60),(1.8,n*60+90),(2,n*60+90)])))
    emit("One_Shot","Hexagons","Hex_Disassembly",2,edges,
         "Шесть рёбер отделяются от шестигранника и расходятся, постепенно разворачиваясь в лучи.",[2])
    cells=[]
    for n in range(7):
        a=(n-1)*60-30
        p=[500,500,0] if n==0 else point(135,a)
        delay=0 if n==0 else .16+(n-1)*.035
        cells.append(layer(f"Hive cell {n}",polygon(6,74,-30),2,2,2.8,
            opacity=pulse(delay,1.8),position=animated([(0,[500,500,0]),(delay,[500,500,0]),
                    (.7,p),(1.8,[500+(p[0]-500)*3.8,500+(p[1]-500)*3.8,0]),(2,[500+(p[0]-500)*4,500+(p[1]-500)*4,0])]),
            size=scale([(0,3),(delay,3),(.7,100),(1.8,160),(2,160)])))
    emit("One_Shot","Hexagons","Hive_Ripple",2,cells,
         "Импульс строит компактную соту из семи ячеек, затем раздвигает её в стороны.",[2])

    sweeps=[]
    for n in range(4):
        delay=n*.07
        sweeps.append(layer(f"Axis streak {n+1}",path([[-150,0],[150,0]]),2,3,3.4,
            opacity=pulse(delay,1.85),rotation=static(n*90),
            position=animated([(0,point(0,n*90)),(delay,point(0,n*90)),(.4+delay,point(140,n*90)),(1.8,point(900,n*90)),(2,point(1000,n*90))]),
            size=animated([(0,[3,100,100]),(.5,[100,100,100]),(1.8,[200,100,100]),(2,[200,100,100])]),
            draw=trim(0,animated([(0,0),(.25,100),(2,100)]))))
    emit("One_Shot","Lines","Axis_Sweeps",2,sweeps,
         "Четыре длинных штриха вытягиваются по главным осям, создавая направленный крестообразный пролёт.",[3])
    comets=[]
    for n in range(5):
        points=[]
        for k in range(17):
            q=k/16;a=math.radians(n*72+q*90);r=28+850*q*q
            points.append((q*2,[500+r*math.cos(a),500+r*math.sin(a),0]))
        comets.append(layer(f"Curved tracer {n+1}",path([[-46,0],[46,0]]),2,3,3.5,
            opacity=pulse(n*.025,1.9),position=animated(points),
            rotation=animated([(0,n*72+60),(2,n*72+150)]),
            size=scale([(0,10),(.7,100),(2,220)])))
    emit("One_Shot","Lines","Curved_Comets",2,comets,
         "Пять коротких световых следов разгоняются по изогнутым спиральным траекториям.",[3])

    broken=[]
    for row in range(3):
        for col in range(3):
            dx,dy=col-1,row-1
            angle=math.atan2(dy,dx) if dx or dy else 0
            target=point(850,math.degrees(angle)) if dx or dy else [500,500,0]
            broken.append(layer(f"Grid cross {row}-{col}",geometry('cross',15),2,2,3,
                opacity=pulse((abs(dx)+abs(dy))*.06,1.85),
                position=animated([(0,[500+dx*45,500+dy*45,0]),(.35,[500+dx*85,500+dy*85,0]),(1.85,target),(2,target)]),
                rotation=animated([(0,0),(.35,0),(1.85,(dx-dy)*100),(2,(dx-dy)*100)]),
                size=scale([(0,12),(.35,100),(1.85,170),(2,170)])))
    emit("One_Shot","Crosses","Cross_Grid_Break",2,broken,
         "Сначала проявляется сетка из девяти крестов, затем крайние элементы разлетаются с вращением.",[2])
    orbiting=[]
    for n in range(4):
        pts=[]
        for k in range(17):
            q=k/16;pts.append((q*2,point(35+800*q*q,n*90+q*135)))
        orbiting.append(layer(f"Orbit cross {n+1}",geometry('cross',25),2,2,3.3,
            opacity=pulse(0,1.9),position=animated(pts),
            rotation=animated([(0,45),(2,315)]),size=scale([(0,15),(.6,100),(2,210)])))
    emit("One_Shot","Crosses","Cross_Corkscrew",2,orbiting,
         "Четыре вращающихся креста разгоняются по винтовому разлёту вместо прямого радиального движения.",[2])


def backgrounds():
    lanes=[]
    for n in range(3):
        lanes.append(layer(f"Distant track {n}",ellipse(230+n*60),8,2,2.2,
                           size=static([100,45+n*12,100]),rotation=static(n*55+10),draw=trim(0,72)))
        positions=[]
        tilt=math.radians(n*55+10)
        for k in range(65):
            a=math.radians(n*120+k*360/64)
            x=(230+n*60)*math.cos(a);y=(230+n*60)*(.45+n*.12)*math.sin(a)
            positions.append((k*.125,[500+x*math.cos(tilt)-y*math.sin(tilt),
                                      500+x*math.sin(tilt)+y*math.cos(tilt),0]))
        lanes.append(layer(f"Moving satellite {n}",ellipse(9),8,2,3,
                           position=animated(positions,linear=True)))
    emit("BG","Circles","Orbital_Lanes",8,lanes,
         "Три наклонные эллиптические трассы с маленькими движущимися спутниками; прозрачность 100%.",[2])
    drifts=[]
    for n in range(5):
        a=n*72
        drifts.append(layer(f"Drifting ring {n}",ellipse(110+n%2*25),8,2,2.2,
            position=animated([(0,point(210,a)),(4,point(260,a+24)),(8,point(210,a))]),
            size=animated([(0,[100,75+n*3,100]),(4,[75,105,100]),(8,[100,75+n*3,100])]),
            rotation=static(a),draw=trim(0,70,n*48)))
    emit("BG","Circles","Drifting_Rings",8,drifts,
         "Пять незамкнутых колец медленно дрейфуют вокруг пустого центра без тоннельного масштабирования.",[2])

    gates=[]
    for n in range(4):
        side=-1 if n%2 else 1;row=n//2
        gates.append(layer(f"Parallax gate {n}",rectangle(80,3),8,2,2.4,
            position=animated([(0,[500+side*300,350+row*300,0]),(4,[500+side*235,330+row*340,0]),(8,[500+side*300,350+row*300,0])]),
            size=animated([(0,[45,100,100]),(4,[85,110,100]),(8,[45,100,100])]),
            rotation=animated([(0,side*12),(4,-side*8),(8,side*12)])))
    emit("BG","Geometry","Parallax_Gates",8,gates,
         "Четыре боковые рамки меняют ширину и наклон, создавая тихий параллакс вокруг свободного центра.",[2])
    flow=[]
    for n in range(12):
        phase=n*8/12;side=-1 if n%2 else 1
        positions=[];alphas=[]
        for step in range(81):
            t=step/10;q=((t+phase)%8)/8
            positions.append((t,[500+side*(210+(n%3)*80),-80+1160*(q if side==1 else 1-q),0]))
            fade=min(1,q/.1,(1-q)/.1)
            alphas.append((t,max(0,fade)*100))
        flow.append(layer(f"Counterflow glyph {n}",geometry('square' if n%3 else 'triangle',15+n%2*7),8,
            2 if n%3 else 4,2.4,opacity=animated(alphas),position=animated(positions,linear=True),rotation=static(n*23)))
    emit("BG","Geometry","Counterflow_Field",8,flow,
         "Два боковых встречных потока небольших знаков. На переносе за краем — короткий fade 0–100%.",[2,4])

    rails=[]
    for side in (-1,1):
        for n in range(4):
            phase=n*8/4;states=[];alphas=[]
            for step in range(65):
                t=step/8;q=((t+phase)%8)/8
                y=35+530*q*q
                states.append((t,[[side*90,side*y*.18],[side*680,side*y]]))
                alphas.append((t,max(0,min(1,q/.08,(1-q)/.08))*100))
            rails.append(layer(f"Perspective rail {side}-{n}",morph(states),8,3,2.2,opacity=animated(alphas)))
    emit("BG","Lines","Horizon_Rails",8,rails,
         "Восемь расходящихся перспективных рельсов проходят по краям; глубина строится линиями, не кольцами.",[3])
    waves=[]
    for n in range(5):
        states=[]
        for k in range(9):
            t=k
            verts=[]
            for j in range(25):
                x=-600+j*50
                y=(n-2)*170+35*math.sin(j/24*math.tau+t/8*math.tau+n*.8)
                verts.append([x,y])
            states.append((t,verts))
        waves.append(layer(f"Wave filament {n}",morph(states),8,3,2,
                           rotation=static(-12),draw=trim(8,92)))
    emit("BG","Lines","Wave_Filaments",8,waves,
         "Пять тонких открытых нитей несут медленную поперечную волну; все линии имеют прозрачность 100%.",[3])


def populate():
    MANIFEST.clear()
    shapes()
    shots()
    backgrounds()
    assert len(MANIFEST)==26
    return list(MANIFEST)
