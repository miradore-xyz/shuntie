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
    Point resizeHandle(float padding=0) const {auto b=rawBounds();return toWorld({b.r+padding,b.b+padding});}
    Point rotateHandle(float gap,float padding=0) const {auto b=rawBounds();return toWorld({(b.l+b.r)/2,b.t-padding-gap});}
    Box bounds() const {auto b=rawBounds();Point points[]={toWorld({b.l,b.t}),toWorld({b.r,b.t}),toWorld({b.r,b.b}),toWorld({b.l,b.b})};Box out{points[0].x,points[0].y,points[0].x,points[0].y};for(auto p:points){out.l=std::min(out.l,p.x);out.r=std::max(out.r,p.x);out.t=std::min(out.t,p.y);out.b=std::max(out.b,p.y);}return out;}
    void resizeTo(Point world){auto box=rawBounds();auto anchor=toWorld({box.l,box.t});Point q=rotate(world,anchor,-rotation);float sx=std::max(4.f,q.x-anchor.x)/std::max(1.f,box.width()),sy=std::max(4.f,q.y-anchor.y)/std::max(1.f,box.height());auto scale=[&](Point& p){p.x=box.l+(p.x-box.l)*sx;p.y=box.t+(p.y-box.t)*sy;};scale(a);scale(b);for(auto& p:points)scale(p);auto after=toWorld({box.l,box.t});move(anchor.x-after.x,anchor.y-after.y);}
    void move(float x,float y){ a.x+=x;a.y+=y;b.x+=x;b.y+=y;for(auto& p:points){p.x+=x;p.y+=y;} }
};
// 只在画笔落笔期间隐藏选中框，移动、缩放和旋转时仍保持可见。
inline bool selectionFrameVisible(Tool tool,bool drawing){return tool!=Tool::Pen||!drawing;}
inline float selectionPadding(const Shape& shape,float scale){return std::max(4.f*scale,shape.stroke*.5f+2.f*scale);}
inline int areaHandle(Box b,Point p,float tolerance){
    Point handles[]={{b.l,b.t},{(b.l+b.r)/2,b.t},{b.r,b.t},{b.r,(b.t+b.b)/2},{b.r,b.b},{(b.l+b.r)/2,b.b},{b.l,b.b},{b.l,(b.t+b.b)/2}};
    for(int i=0;i<8;++i)if(std::hypot(p.x-handles[i].x,p.y-handles[i].y)<=tolerance)return i;
    if(p.x>=b.l&&p.x<=b.r){if(std::abs(p.y-b.t)<=tolerance)return 1;if(std::abs(p.y-b.b)<=tolerance)return 5;}
    if(p.y>=b.t&&p.y<=b.b){if(std::abs(p.x-b.l)<=tolerance)return 7;if(std::abs(p.x-b.r)<=tolerance)return 3;}return -1;
}
inline Box resizeArea(Box original,int handle,Point p,float width,float height){
    Box b=original;float minW=std::min(8.f,b.width()),minH=std::min(8.f,b.height());
    if(handle==0||handle==6||handle==7)b.l=std::clamp(p.x,0.f,b.r-minW);
    if(handle==2||handle==3||handle==4)b.r=std::clamp(p.x,b.l+minW,width);
    if(handle==0||handle==1||handle==2)b.t=std::clamp(p.y,0.f,b.b-minH);
    if(handle==4||handle==5||handle==6)b.b=std::clamp(p.y,b.t+minH,height);
    return b;
}
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
