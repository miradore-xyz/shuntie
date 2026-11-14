#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// 独立的标注模型：所有坐标均为桌面物理像素，避免高分屏选区偏移。
namespace snip {
struct Point { float x=0, y=0; };
struct Box { float l=0,t=0,r=0,b=0;
    float width() const { return r-l; }
    float height() const { return b-t; }
    bool contains(Point p) const { return p.x>=l && p.x<=r && p.y>=t && p.y<=b; }
};
inline Box normalized(Point a, Point b) { return {std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y)}; }
inline Point rotate(Point p,Point c,float degrees){float r=degrees*0.01745329252f,cs=std::cos(r),sn=std::sin(r);return {c.x+(p.x-c.x)*cs-(p.y-c.y)*sn,c.y+(p.x-c.x)*sn+(p.y-c.y)*cs};}
enum class Tool { Select, Rectangle, Arrow, Pen, Mosaic };
struct Shape {
    Tool tool=Tool::Rectangle;
    Point a,b;
    unsigned color=0xFF6554;
    float stroke=3;
    float rotation=0;
    std::vector<Point> points;
    Box rawBounds() const {
        Box z=normalized(a,b);
        if(tool==Tool::Pen && !points.empty()) {
            z={points[0].x,points[0].y,points[0].x,points[0].y};
            for(auto p:points){z.l=std::min(z.l,p.x);z.t=std::min(z.t,p.y);z.r=std::max(z.r,p.x);z.b=std::max(z.b,p.y);}
        }
        return z;
    }
    Point center() const {auto b=rawBounds();return {(b.l+b.r)/2,(b.t+b.b)/2};}
    Point toWorld(Point p) const {return rotate(p,center(),rotation);}
    Point toLocal(Point p) const {return rotate(p,center(),-rotation);}
    Point resizeHandle() const {auto b=rawBounds();return toWorld({b.r,b.b});}
    Point rotateHandle(float gap) const {auto b=rawBounds();return toWorld({(b.l+b.r)/2,b.t-gap});}
    Box bounds() const {auto b=rawBounds();Point points[]={toWorld({b.l,b.t}),toWorld({b.r,b.t}),toWorld({b.r,b.b}),toWorld({b.l,b.b})};Box out{points[0].x,points[0].y,points[0].x,points[0].y};for(auto p:points){out.l=std::min(out.l,p.x);out.r=std::max(out.r,p.x);out.t=std::min(out.t,p.y);out.b=std::max(out.b,p.y);}return out;}
    void resizeTo(Point world){auto box=rawBounds();auto anchor=toWorld({box.l,box.t});Point q=rotate(world,anchor,-rotation);float sx=std::max(4.f,q.x-anchor.x)/std::max(1.f,box.width()),sy=std::max(4.f,q.y-anchor.y)/std::max(1.f,box.height());auto scale=[&](Point& p){p.x=box.l+(p.x-box.l)*sx;p.y=box.t+(p.y-box.t)*sy;};scale(a);scale(b);for(auto& p:points)scale(p);auto after=toWorld({box.l,box.t});move(anchor.x-after.x,anchor.y-after.y);}
    void move(float x,float y){ a.x+=x;a.y+=y;b.x+=x;b.y+=y;for(auto& p:points){p.x+=x;p.y+=y;} }
};
inline float segmentDistance(Point p,Point a,Point b) {
    float x=b.x-a.x,y=b.y-a.y, d=x*x+y*y;
    float t=d>0?std::clamp(((p.x-a.x)*x+(p.y-a.y)*y)/d,0.f,1.f):0;
    return std::hypot(p.x-a.x-t*x,p.y-a.y-t*y);
}
inline bool hit(const Shape& s,Point p) {
    p=s.toLocal(p);auto b=s.rawBounds();float gap=std::max(8.f,s.stroke+3);
    if(s.tool==Tool::Mosaic) return b.contains(p);
    if(s.tool==Tool::Arrow) return segmentDistance(p,s.a,s.b)<=gap;
    if(s.tool==Tool::Pen){for(size_t i=1;i<s.points.size();++i) if(segmentDistance(p,s.points[i-1],s.points[i])<=gap)return true;return false;}
    // 方框内部同样可以抓取，避免只能精确点击细边线。
    return b.contains(p) || segmentDistance(p,{b.l,b.t},{b.r,b.t})<=gap || segmentDistance(p,{b.r,b.t},{b.r,b.b})<=gap || segmentDistance(p,{b.r,b.b},{b.l,b.b})<=gap || segmentDistance(p,{b.l,b.b},{b.l,b.t})<=gap;
}
// GIF 的时间单位为百分之一秒；60 帧目标由编码器分配 1/2 厘秒延时。
}
