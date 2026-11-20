// 由 tools/generate_assets.py 根据 Lucide SVG 自动生成，许可见 assets/lucide/LICENSE。
#pragma once
#include "graphics.hpp"
enum class Icon {scan_line,clipboard_paste,video,copy,folder_open,minus,x,mouse_pointer_2,square,move_up_right,pencil,grid_2x2,palette,undo_2,redo_2,save,pin,refresh_cw,circle_stop,sliders_horizontal,play,pause,chevrons_down,mouse_pointer_click};
inline void drawIcon(ID2D1RenderTarget* rt,Icon icon,float x,float y,float size,unsigned color){
ID2D1SolidColorBrush* brush=nullptr; ID2D1StrokeStyle* strokeStyle=nullptr; rt->CreateSolidColorBrush(rgb(color),&brush); if(!brush)return;
auto props=D2D1::StrokeStyleProperties();props.startCap=props.endCap=props.dashCap=D2D1_CAP_STYLE_ROUND;props.lineJoin=D2D1_LINE_JOIN_ROUND;d2d->CreateStrokeStyle(props,nullptr,0,&strokeStyle);
D2D1_MATRIX_3X2_F old;rt->GetTransform(&old);rt->SetTransform(D2D1::Matrix3x2F::Scale(size/24,size/24)*D2D1::Matrix3x2F::Translation(x,y)*old);
switch(icon){
case Icon::scan_line:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({3.0f,7.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({3.0f,5.0f});
sink->AddArc({{5.0f,3.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({7.0f,3.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({17.0f,3.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({19.0f,3.0f});
sink->AddArc({{21.0f,5.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({21.0f,7.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({21.0f,17.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({21.0f,19.0f});
sink->AddArc({{19.0f,21.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({17.0f,21.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({7.0f,21.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({5.0f,21.0f});
sink->AddArc({{3.0f,19.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({3.0f,17.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({7.0f,12.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({17.0f,12.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::clipboard_paste:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({11.0f,14.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({21.0f,14.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({16.0f,4.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({18.0f,4.0f});
sink->AddArc({{20.0f,6.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({20.0f,7.344f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({17.0f,18.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({21.0f,14.0f});
sink->AddLine({17.0f,10.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({8.0f,4.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({6.0f,4.0f});
sink->AddArc({{4.0f,6.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({4.0f,20.0f});
sink->AddArc({{6.0f,22.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({18.0f,22.0f});
sink->AddArc({{19.793f,20.887f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(8.0f,2.0f,16.0f,6.0f),1.0f,1.0f),brush,1.7f,strokeStyle);
break;
case Icon::video:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({16.0f,13.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({21.223f,16.482f});
sink->AddArc({{22.0f,16.066f},{0.5f,0.5f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({22.0f,7.87f});
sink->AddArc({{21.248f,7.438f},{0.5f,0.5f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({16.0f,10.5f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(2.0f,6.0f,16.0f,18.0f),2.0f,2.0f),brush,1.7f,strokeStyle);
break;
case Icon::copy:
rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(8.0f,8.0f,22.0f,22.0f),2.0f,2.0f),brush,1.7f,strokeStyle);
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({4.0f,16.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddBezier({{2.9f,16.0f},{2.0f,15.1f},{2.0f,14.0f}});
sink->AddLine({2.0f,4.0f});
sink->AddBezier({{2.0f,2.9f},{2.9f,2.0f},{4.0f,2.0f}});
sink->AddLine({14.0f,2.0f});
sink->AddBezier({{15.1f,2.0f},{16.0f,2.9f},{16.0f,4.0f}});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::folder_open:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({6.0f,14.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({7.5f,11.1f});
sink->AddArc({{9.24f,10.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({20.0f,10.0f});
sink->AddArc({{21.94f,12.5f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({20.4f,18.5f});
sink->AddArc({{18.45f,20.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({4.0f,20.0f});
sink->AddArc({{2.0f,18.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({2.0f,5.0f});
sink->AddArc({{4.0f,3.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({7.9f,3.0f});
sink->AddArc({{9.59f,3.9f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({10.4f,5.1f});
sink->AddArc({{12.07f,6.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({18.0f,6.0f});
sink->AddArc({{20.0f,8.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({20.0f,10.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::minus:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({5.0f,12.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({19.0f,12.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::x:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({18.0f,6.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({6.0f,18.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({6.0f,6.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({18.0f,18.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::mouse_pointer_2:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({4.037f,4.688f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddArc({{4.688f,4.037f},{0.495f,0.495f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({20.688f,10.537f});
sink->AddArc({{20.625f,11.484f},{0.5f,0.5f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({14.501f,13.064f});
sink->AddArc({{13.063f,14.499f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({11.484f,20.625f});
sink->AddArc({{10.537f,20.688f},{0.5f,0.5f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->EndFigure(D2D1_FIGURE_END_CLOSED);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::square:
rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(3.0f,3.0f,21.0f,21.0f),2.0f,2.0f),brush,1.7f,strokeStyle);
break;
case Icon::move_up_right:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({13.0f,5.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({19.0f,5.0f});
sink->AddLine({19.0f,11.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({19.0f,5.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({5.0f,19.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::pencil:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({21.174f,6.812f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddArc({{17.188f,2.825f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({3.842f,16.174f});
sink->AddArc({{3.342f,17.004f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({2.021f,21.356f});
sink->AddArc({{2.644f,21.978f},{0.5f,0.5f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({6.997f,20.658f});
sink->AddArc({{7.827f,20.161f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->EndFigure(D2D1_FIGURE_END_CLOSED);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({15.0f,5.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({19.0f,9.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::grid_2x2:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({12.0f,3.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({12.0f,21.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({3.0f,12.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({21.0f,12.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(3.0f,3.0f,21.0f,21.0f),2.0f,2.0f),brush,1.7f,strokeStyle);
break;
case Icon::palette:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({12.0f,22.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddArc({{12.0f,2.0f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddArc({{22.0f,11.0f},{10.0f,9.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddArc({{17.0f,16.0f},{5.0f,5.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({14.75f,16.0f});
sink->AddArc({{13.35f,18.8f},{1.75f,1.75f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({13.65f,19.2f});
sink->AddArc({{12.25f,22.0f},{1.75f,1.75f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->EndFigure(D2D1_FIGURE_END_CLOSED);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
rt->DrawEllipse(D2D1::Ellipse({13.5f,6.5f},0.5f,0.5f),brush,1.7f,strokeStyle);
rt->DrawEllipse(D2D1::Ellipse({17.5f,10.5f},0.5f,0.5f),brush,1.7f,strokeStyle);
rt->DrawEllipse(D2D1::Ellipse({6.5f,12.5f},0.5f,0.5f),brush,1.7f,strokeStyle);
rt->DrawEllipse(D2D1::Ellipse({8.5f,7.5f},0.5f,0.5f),brush,1.7f,strokeStyle);
break;
case Icon::undo_2:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({9.0f,14.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({4.0f,9.0f});
sink->AddLine({9.0f,4.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({4.0f,9.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({14.5f,9.0f});
sink->AddArc({{20.0f,14.5f},{5.5f,5.5f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddArc({{14.5f,20.0f},{5.5f,5.5f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({11.0f,20.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::redo_2:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({15.0f,14.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({20.0f,9.0f});
sink->AddLine({15.0f,4.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({20.0f,9.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({9.5f,9.0f});
sink->AddArc({{4.0f,14.5f},{5.5f,5.5f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddArc({{9.5f,20.0f},{5.5f,5.5f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({13.0f,20.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::save:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({15.2f,3.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddArc({{16.6f,3.6f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({20.4f,7.4f});
sink->AddArc({{21.0f,8.8f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({21.0f,19.0f});
sink->AddArc({{19.0f,21.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({5.0f,21.0f});
sink->AddArc({{3.0f,19.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({3.0f,5.0f});
sink->AddArc({{5.0f,3.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->EndFigure(D2D1_FIGURE_END_CLOSED);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({17.0f,21.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({17.0f,14.0f});
sink->AddArc({{16.0f,13.0f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({8.0f,13.0f});
sink->AddArc({{7.0f,14.0f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({7.0f,21.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({7.0f,3.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({7.0f,7.0f});
sink->AddArc({{8.0f,8.0f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({15.0f,8.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::pin:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({12.0f,17.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({12.0f,22.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({9.0f,10.76f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddArc({{7.89f,12.55f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({6.11f,13.45f});
sink->AddArc({{5.0f,15.24f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({5.0f,16.0f});
sink->AddArc({{6.0f,17.0f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({18.0f,17.0f});
sink->AddArc({{19.0f,16.0f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({19.0f,15.24f});
sink->AddArc({{17.89f,13.45f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({16.11f,12.55f});
sink->AddArc({{15.0f,10.76f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({15.0f,7.0f});
sink->AddArc({{16.0f,6.0f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddArc({{16.0f,2.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({8.0f,2.0f});
sink->AddArc({{8.0f,6.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddArc({{9.0f,7.0f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->EndFigure(D2D1_FIGURE_END_CLOSED);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::refresh_cw:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({3.0f,12.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddArc({{12.0f,3.0f},{9.0f,9.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddArc({{18.74f,5.74f},{9.75f,9.75f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({21.0f,8.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({21.0f,3.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({21.0f,8.0f});
sink->AddLine({16.0f,8.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({21.0f,12.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddArc({{12.0f,21.0f},{9.0f,9.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddArc({{5.26f,18.26f},{9.75f,9.75f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({3.0f,16.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({8.0f,16.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({3.0f,16.0f});
sink->AddLine({3.0f,21.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::circle_stop:
rt->DrawEllipse(D2D1::Ellipse({12.0f,12.0f},10.0f,10.0f),brush,1.7f,strokeStyle);
rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(9.0f,9.0f,15.0f,15.0f),1.0f,1.0f),brush,1.7f,strokeStyle);
break;
case Icon::sliders_horizontal:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({10.0f,5.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({3.0f,5.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({12.0f,19.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({3.0f,19.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({14.0f,3.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({14.0f,7.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({16.0f,17.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({16.0f,21.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({21.0f,12.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({12.0f,12.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({21.0f,19.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({16.0f,19.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({21.0f,5.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({14.0f,5.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({8.0f,10.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({8.0f,14.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({8.0f,12.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({3.0f,12.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::play:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({5.0f,5.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddArc({{8.008f,3.272f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({20.005f,10.27f});
sink->AddArc({{20.008f,13.728f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({8.008f,20.728f});
sink->AddArc({{5.0f,19.0f},{2.0f,2.0f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->EndFigure(D2D1_FIGURE_END_CLOSED);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::pause:
rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(14.0f,3.0f,19.0f,21.0f),1.0f,1.0f),brush,1.7f,strokeStyle);
rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(5.0f,3.0f,10.0f,21.0f),1.0f,1.0f),brush,1.7f,strokeStyle);
break;
case Icon::chevrons_down:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({7.0f,6.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({12.0f,11.0f});
sink->AddLine({17.0f,6.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({7.0f,13.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({12.0f,18.0f});
sink->AddLine({17.0f,13.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
case Icon::mouse_pointer_click:
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({14.0f,4.1f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({12.0f,6.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({5.1f,8.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({2.2f,7.2f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({6.0f,12.0f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({4.1f,14.0f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({7.2f,2.2f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddLine({8.0f,5.1f});
sink->EndFigure(D2D1_FIGURE_END_OPEN);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
{ ID2D1PathGeometry* geometry=nullptr; ID2D1GeometrySink* sink=nullptr;
if(SUCCEEDED(d2d->CreatePathGeometry(&geometry)) && SUCCEEDED(geometry->Open(&sink))){
sink->BeginFigure({9.037f,9.69f},D2D1_FIGURE_BEGIN_HOLLOW);
sink->AddArc({{9.69f,9.037f},{0.498f,0.498f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({20.69f,13.537f});
sink->AddArc({{20.616f,14.486f},{0.5f,0.5f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({16.267f,15.527f});
sink->AddArc({{15.527f,16.266f},{1.0f,1.0f},0.0f,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->AddLine({14.487f,20.616f});
sink->AddArc({{13.537f,20.69f},{0.5f,0.5f},0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
sink->EndFigure(D2D1_FIGURE_END_CLOSED);
sink->Close(); rt->DrawGeometry(geometry,brush,1.7f,strokeStyle); } release(sink); release(geometry); }
break;
}rt->SetTransform(old);release(strokeStyle);release(brush);}