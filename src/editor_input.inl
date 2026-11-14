void editorMouseDown(HWND hwnd,snip::Point p){
    SetFocus(hwnd);editor.last=p;editor.start=p;editor.dragging=true;editor.moving=false;editor.resizing=false;editor.rotating=false;editor.adjustingArea=false;editor.selecting=!editor.selected;
    bool resize=false,rotate=false;
    if(editor.selected&&editor.active>=0&&editor.tool==snip::Tool::Select){auto& shape=editor.shapes[editor.active];float padding=snip::selectionPadding(shape,mainScale);auto a=shape.resizeHandle(padding),b=shape.rotateHandle(28*mainScale,padding);resize=std::hypot(p.x-a.x,p.y-a.y)<12*mainScale;rotate=std::hypot(p.x-b.x,p.y-b.y)<12*mainScale;}
    if(editor.selecting){POINT screen{LONG(p.x)+editor.desktop.left,LONG(p.y)+editor.desktop.top};editor.selection=snapAt(editor.snapWindows,screen,editor.desktop);editor.hoverSnap=false;}
    else if(rotate){editor.checkpoint();editor.rotating=true;auto& shape=editor.shapes[editor.active];auto c=shape.center();editor.initialRotation=shape.rotation;editor.initialPolar=std::atan2(p.y-c.y,p.x-c.x);}
    else if(resize){editor.checkpoint();editor.resizing=true;}
    else if((editor.areaEdge=snip::areaHandle(editor.selection,p,7*mainScale))>=0){editor.adjustingArea=true;editor.areaBefore=editor.selection;editor.toolbarVisible=false;HDWP positions=BeginDeferWindowPos(int(editor.toolbar.size()));for(HWND b:editor.toolbar)positions=DeferWindowPos(positions,b,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_HIDEWINDOW);if(positions)EndDeferWindowPos(positions);}
    else if(editor.selection.contains(p)&&!editor.recording){
        if(editor.tool==snip::Tool::Select){editor.active=-1;for(int i=int(editor.shapes.size())-1;i>=0;--i)if(snip::hit(editor.shapes[i],p)){editor.active=i;break;}if(editor.active>=0){editor.checkpoint();editor.moving=true;}}
        else{editor.checkpoint();snip::Shape shape;shape.tool=editor.tool;shape.a=p;shape.b=p;shape.color=colors[editor.colorIndex];shape.stroke=editor.stroke;if(shape.tool==snip::Tool::Pen)shape.points.push_back(p);editor.shapes.push_back(shape);editor.active=int(editor.shapes.size())-1;}
    }else editor.dragging=false;
    if(editor.dragging)SetCapture(hwnd);InvalidateRect(hwnd,nullptr,FALSE);
}
void editorMouseMove(HWND hwnd,snip::Point p){
    if(!editor.dragging){if(!editor.selected){POINT screen{LONG(p.x)+editor.desktop.left,LONG(p.y)+editor.desktop.top};auto next=snapAt(editor.snapWindows,screen,editor.desktop);auto previous=editor.hoverSelection;if(!editor.hoverSnap||next.l!=previous.l||next.t!=previous.t||next.r!=previous.r||next.b!=previous.b){editor.hoverSelection=next;editor.hoverSnap=true;InvalidateRect(hwnd,nullptr,FALSE);}}return;}
    p.x=std::clamp(p.x,0.f,float(editor.screen.width));p.y=std::clamp(p.y,0.f,float(editor.screen.height));
    if(editor.adjustingArea)editor.selection=snip::resizeArea(editor.areaBefore,editor.areaEdge,p,float(editor.screen.width),float(editor.screen.height));
    else if(editor.selecting){if(std::hypot(p.x-editor.start.x,p.y-editor.start.y)>4*mainScale)editor.selection=snip::normalized(editor.start,p);}
    else if(editor.active>=0){p.x=std::clamp(p.x,editor.selection.l,editor.selection.r);p.y=std::clamp(p.y,editor.selection.t,editor.selection.b);auto& shape=editor.shapes[editor.active];
        if(editor.moving){auto b=shape.bounds();float dx=b.width()<=editor.selection.width()?std::clamp(p.x-editor.last.x,editor.selection.l-b.l,editor.selection.r-b.r):0,dy=b.height()<=editor.selection.height()?std::clamp(p.y-editor.last.y,editor.selection.t-b.t,editor.selection.b-b.b):0;shape.move(dx,dy);}
        else if(editor.rotating){auto c=shape.center();shape.rotation=editor.initialRotation+(std::atan2(p.y-c.y,p.x-c.x)-editor.initialPolar)*57.29577951f;if(GetKeyState(VK_SHIFT)<0)shape.rotation=std::round(shape.rotation/15)*15;}
        else if(editor.resizing){float padding=snip::selectionPadding(shape,mainScale);auto local=shape.toLocal(p);shape.resizeTo(shape.toWorld({local.x-padding,local.y-padding}));}
        else if(editor.tool!=snip::Tool::Select){shape.b=p;if(shape.tool==snip::Tool::Pen)shape.points.push_back(p);}
    }editor.last=p;InvalidateRect(hwnd,nullptr,FALSE);
}
void editorMouseUp(HWND hwnd){
    if(!editor.dragging)return;ReleaseCapture();editor.dragging=false;
    if(editor.selecting){editor.selected=editor.selection.width()>6&&editor.selection.height()>6;if(editor.selected)createToolbar();}
    else if(editor.adjustingArea){editor.adjustingArea=false;layoutToolbar();}
    else if(editor.active>=0&&!editor.moving&&!editor.resizing&&!editor.rotating&&editor.tool!=snip::Tool::Select){auto b=editor.shapes[editor.active].bounds();if(b.width()<2&&b.height()<2){editor.shapes.pop_back();editor.active=-1;}else{editor.tool=snip::Tool::Select;toolHint();for(HWND b:editor.toolbar)InvalidateRect(b,nullptr,FALSE);}}
    editor.selecting=false;InvalidateRect(hwnd,nullptr,FALSE);
}
bool editorCursor(HWND hwnd){
    POINT pos;GetCursorPos(&pos);if(WindowFromPoint(pos)!=hwnd)return false;ScreenToClient(hwnd,&pos);snip::Point point{float(pos.x),float(pos.y)};
    if(editor.selected){if(editor.active>=0&&editor.tool==snip::Tool::Select){auto& shape=editor.shapes[editor.active];float padding=snip::selectionPadding(shape,mainScale);auto resize=shape.resizeHandle(padding),rotate=shape.rotateHandle(28*mainScale,padding);if(std::hypot(point.x-rotate.x,point.y-rotate.y)<12*mainScale){SetCursor(LoadCursorW(nullptr,IDC_CROSS));return true;}if(std::hypot(point.x-resize.x,point.y-resize.y)<12*mainScale){SetCursor(LoadCursorW(nullptr,IDC_SIZENWSE));return true;}}
        int edge=editor.adjustingArea?editor.areaEdge:snip::areaHandle(editor.selection,point,7*mainScale);if(edge>=0){const wchar_t* cursor=edge==0||edge==4?IDC_SIZENWSE:edge==2||edge==6?IDC_SIZENESW:edge==1||edge==5?IDC_SIZENS:IDC_SIZEWE;SetCursor(LoadCursorW(nullptr,cursor));return true;}
        if(editor.tool==snip::Tool::Select)for(auto& shape:editor.shapes)if(snip::hit(shape,point)){SetCursor(LoadCursorW(nullptr,IDC_SIZEALL));return true;}
    }SetCursor(LoadCursorW(nullptr,IDC_CROSS));return true;
}
