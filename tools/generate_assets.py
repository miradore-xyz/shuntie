"""生成 Lucide 的 Direct2D 图标路径与应用图标；SVG 原始文件保留在 assets。"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
names = ['scan-line','clipboard-paste','video','copy','folder-open','minus','x','mouse-pointer-2','square','move-up-right','pencil','grid-2x2','palette','undo-2','redo-2','save','pin','refresh-cw','circle-stop','sliders-horizontal','play','pause']

def num(v):
    return f'{float(v):.6g}f' if '.' in f'{float(v):.6g}' else f'{float(v):.6g}.0f'

def point(x,y): return '{'+num(x)+','+num(y)+'}'

def path_code(data):
    tokens=re.findall(r'[a-zA-Z]|[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?',data)
    out=['{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;', 'if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){']
    i=0;x=y=0;start=(0,0);control=(0,0);previous='';opened=False;cmd=''
    while i<len(tokens):
        if tokens[i].isalpha(): cmd=tokens[i];i+=1
        op=cmd.upper();relative=cmd.islower();count={'M':2,'L':2,'H':1,'V':1,'C':6,'S':4,'Q':4,'T':2,'A':7,'Z':0}[op]
        values=list(map(float,tokens[i:i+count]));i+=count
        def xy(a,b):return (a+x,b+y) if relative else (a,b)
        if op=='M':
            if opened:out.append('sink->EndFigure(D2D1_FIGURE_END_OPEN);')
            x,y=xy(*values);start=(x,y);opened=True;out.append(f'sink->BeginFigure({point(x,y)},D2D1_FIGURE_BEGIN_HOLLOW);');cmd='l' if relative else 'L'
        elif op in ('L','H','V'):
            if op=='L':x,y=xy(*values)
            if op=='H':x=values[0]+x if relative else values[0]
            if op=='V':y=values[0]+y if relative else values[0]
            out.append(f'sink->AddLine({point(x,y)});')
        elif op in ('C','S'):
            if op=='C':a=xy(*values[:2]);b=xy(*values[2:4]);end=xy(*values[4:])
            else:a=(2*x-control[0],2*y-control[1]) if previous in ('C','S') else (x,y);b=xy(*values[:2]);end=xy(*values[2:])
            out.append(f'sink->AddBezier({{{point(*a)},{point(*b)},{point(*end)}}});');control=b;x,y=end
        elif op in ('Q','T'):
            a=xy(*values[:2]) if op=='Q' else ((2*x-control[0],2*y-control[1]) if previous in ('Q','T') else (x,y))
            end=xy(*values[-2:]);out.append(f'sink->AddQuadraticBezier({{{point(*a)},{point(*end)}}});');control=a;x,y=end
        elif op=='A':
            rx,ry,rotation,large,sweep,xx,yy=values;x,y=xy(xx,yy)
            out.append(f'sink->AddArc({{{point(x,y)},{{{num(rx)},{num(ry)}}},{num(rotation)},D2D1_SWEEP_DIRECTION_{"CLOCKWISE" if sweep else "COUNTER_CLOCKWISE"},D2D1_ARC_SIZE_{"LARGE" if large else "SMALL"}}});')
        elif op=='Z':out.append('sink->EndFigure(D2D1_FIGURE_END_CLOSED);');opened=False;x,y=start
        previous=op
    if opened:out.append('sink->EndFigure(D2D1_FIGURE_END_OPEN);')
    out.append('sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }')
    return out

header=['// 由 tools/generate_assets.py 根据 Lucide SVG 自动生成，许可见 assets/lucide/LICENSE。','#pragma once','#include "graphics.hpp"','enum class Icon {'+','.join(n.replace('-','_') for n in names)+'};','inline void drawIcon(ID2D1RenderTarget* rt,Icon icon,float x,float y,float size,unsigned color){','ID2D1SolidColorBrush* brush=nullptr; ID2D1StrokeStyle* strokeStyle=nullptr; rt->CreateSolidColorBrush(rgb(color),&brush); if(!brush)return;','auto props=D2D1::StrokeStyleProperties();props.startCap=props.endCap=props.dashCap=D2D1_CAP_STYLE_ROUND;props.lineJoin=D2D1_LINE_JOIN_ROUND;d2d->CreateStrokeStyle(props,nullptr,0,&strokeStyle);','D2D1_MATRIX_3X2_F old;rt->GetTransform(&old);rt->SetTransform(D2D1::Matrix3x2F::Scale(size/24,size/24)*D2D1::Matrix3x2F::Translation(x,y)*old);','switch(icon){']
for name in names:
    svg=ET.parse(ROOT/'assets/lucide'/f'{name}.svg').getroot();header.append('case Icon::'+name.replace('-','_')+':')
    for e in svg:
        tag=e.tag.split('}')[-1];a=e.attrib
        if tag=='path':header.extend(path_code(a['d']))
        elif tag=='circle':header.append(f'rt->DrawEllipse(D2D1::Ellipse({point(a["cx"],a["cy"])},{num(a["r"])},{num(a["r"])}),brush,1.7f,strokeStyle);')
        elif tag=='rect':
            x,y,w,h=[float(a.get(k,0)) for k in ('x','y','width','height')];radius=a.get('rx',0)
            header.append(f'rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF({num(x)},{num(y)},{num(x+w)},{num(y+h)}),{num(radius)},{num(radius)}),brush,1.7f,strokeStyle);')
        elif tag=='line':header.append(f'rt->DrawLine({point(a["x1"],a["y1"])},{point(a["x2"],a["y2"])},brush,1.7f,strokeStyle);')
        elif tag in ('polyline','polygon'):
            pts=re.findall(r'[-+]?\d*\.?\d+',a['points']);data='M'+' '.join(pts[:2])+'L'+' '.join(pts[2:])+('Z' if tag=='polygon' else '');header.extend(path_code(data))
        else:raise ValueError(tag)
    header.append('break;')
header.append('}rt->SetTransform(old);release(strokeStyle);release(brush);}')
(ROOT/'src/icons.hpp').write_text('\n'.join(header),encoding='utf-8')

# 简约取景框加一枚独立的图钉圆点，使用同一套薄荷绿视觉。
logo=Image.new('RGBA',(1024,1024),(0,0,0,0));d=ImageDraw.Draw(logo)
d.rounded_rectangle((20,20,1004,1004),radius=240,fill='#161a1f')
mint='#7ce6c5';line=76
for a,b in [((290,610),(290,350)),((290,350),(550,350)),((730,450),(730,710)),((730,710),(470,710))]:
    d.line((a,b),fill=mint,width=line);radius=line//2
    for x,y in (a,b):d.ellipse((x-radius,y-radius,x+radius,y+radius),fill=mint)
d.ellipse((658,250,790,382),fill=mint)
logo.save(ROOT/'assets/logo.png');logo.save(ROOT/'assets/app.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
(ROOT/'assets/logo.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256"><rect x="5" y="5" width="246" height="246" rx="60" fill="#161a1f"/><path d="M72 152V87h65m45 25v65h-65" fill="none" stroke="#7ce6c5" stroke-width="19" stroke-linecap="round" stroke-linejoin="round"/><circle cx="181" cy="79" r="16.5" fill="#7ce6c5"/></svg>',encoding='utf-8')
print('已生成 Lucide 矢量图标、Logo 与多尺寸应用图标。')
