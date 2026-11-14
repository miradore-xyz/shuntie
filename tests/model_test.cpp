#include "../src/model.hpp"
#include <cassert>
#include <iostream>
int main(){
    using namespace snip;
    auto area=normalized({600,500},{100,80});assert(area.width()==500&&area.height()==420&&area.contains({300,300}));
    Shape arrow;arrow.tool=Tool::Arrow;arrow.a={100,100};arrow.b={300,300};assert(hit(arrow,{200,204}));assert(!hit(arrow,{110,280}));
    arrow.move(-80,50);assert(arrow.a.x==20&&arrow.b.y==350);
    Shape rectangle;rectangle.a={50,50};rectangle.b={200,200};assert(hit(rectangle,{52,100}));assert(hit(rectangle,{100,100}));assert(!hit(rectangle,{230,230}));
    Shape pen;pen.tool=Tool::Pen;pen.points={{5,20},{20,50},{100,10}};auto b=pen.bounds();assert(b.l==5&&b.t==10&&b.r==100&&b.b==50);assert(hit(pen,{20,50}));pen.move(10,-5);assert(pen.bounds().l==15&&pen.bounds().t==5);
    Shape mosaic;mosaic.tool=Tool::Mosaic;mosaic.a={80,90};mosaic.b={20,30};assert(hit(mosaic,{50,50}));assert(!hit(mosaic,{90,20}));
    Shape rotated;rotated.a={10,10};rotated.b={110,60};rotated.rotation=90;auto center=rotated.center();auto local=rotated.toLocal(rotated.toWorld({25,35}));assert(std::abs(local.x-25)<.001f&&std::abs(local.y-35)<.001f);assert(hit(rotated,center));auto bounds=rotated.bounds();assert(std::abs(bounds.width()-50)<.01f&&std::abs(bounds.height()-100)<.01f);auto anchor=rotated.toWorld({10,10});rotated.resizeTo(rotated.toWorld({160,100}));auto resized=rotated.rawBounds();auto kept=rotated.toWorld({resized.l,resized.t});assert(std::hypot(anchor.x-kept.x,anchor.y-kept.y)<.01f);
    Box region{100,100,400,300};assert(areaHandle(region,{100,100},7)==0);assert(areaHandle(region,{400,200},7)==3);assert(areaHandle(region,{250,200},7)==-1);auto expanded=resizeArea(region,4,{900,800},800,600);assert(expanded.r==800&&expanded.b==600&&expanded.l==100);auto clipped=resizeArea(region,0,{-40,400},800,600);assert(clipped.l==0&&clipped.t==292&&clipped.r==400&&clipped.b==300);
    std::cout<<"Geometry, reverse selection, object hit testing and movement passed. / 几何、反向选区、对象命中和移动通过。\n";
}
