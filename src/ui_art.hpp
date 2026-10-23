#pragma once
#include <gdiplus.h>
#include <memory>
namespace lpui {
constexpr COLORREF ink=RGB(12,15,21),panel=RGB(25,31,41),gold=RGB(238,197,119),white=RGB(247,249,252),muted=RGB(193,203,218);
inline void fill(HDC dc,RECT rect,COLORREF color){auto b=CreateSolidBrush(color);FillRect(dc,&rect,b);DeleteObject(b);}
inline void text(HDC dc,const std::wstring& label,RECT rect,HFONT font,COLORREF color=white,UINT flags=DT_CENTER|DT_VCENTER|DT_SINGLELINE){auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,label.c_str(),-1,&rect,flags);SelectObject(dc,old);}
inline void line(HDC dc,int x,int y,int xx,int yy,COLORREF color=gold,int width=4){auto p=CreatePen(PS_SOLID,width,color);auto old=SelectObject(dc,p);MoveToEx(dc,x,y,nullptr);LineTo(dc,xx,yy);SelectObject(dc,old);DeleteObject(p);}
inline void box(HDC dc,RECT r,COLORREF color=gold,int width=4){line(dc,r.left,r.top,r.right,r.top,color,width);line(dc,r.right,r.top,r.right,r.bottom,color,width);line(dc,r.right,r.bottom,r.left,r.bottom,color,width);line(dc,r.left,r.bottom,r.left,r.top,color,width);}
// Shared native design surfaces: smooth corners, subtle depth and restrained gold.
inline Gdiplus::Color color(COLORREF c){return Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c));}
inline void surface(HDC dc,RECT r,COLORREF top=panel,COLORREF bottom=RGB(18,23,32),COLORREF border=RGB(66,77,94),int radius=16){
 if(r.right<=r.left||r.bottom<=r.top)return;
 Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
 float x=static_cast<float>(r.left)+1,y=static_cast<float>(r.top)+1,w=static_cast<float>(r.right-r.left)-3,h=static_cast<float>(r.bottom-r.top)-3;
 float d=static_cast<float>(std::min(radius*2,static_cast<int>(std::min(w,h))));
 Gdiplus::GraphicsPath path;path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();
 Gdiplus::LinearGradientBrush brush(Gdiplus::PointF(x,y),Gdiplus::PointF(x,y+h),color(top),color(bottom));g.FillPath(&brush,&path);
 Gdiplus::Pen edge(color(border),1);g.DrawPath(&edge,&path);
}
// A consistent, scalable line-icon family. Coordinates use a 100-unit canvas.
inline void categoryIcon(HDC dc,RECT r,int category){
 using namespace Gdiplus;
 Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.TranslateTransform(static_cast<REAL>(r.left),static_cast<REAL>(r.top));g.ScaleTransform((r.right-r.left)/100.f,(r.bottom-r.top)/100.f);
 Pen p(color(gold),4);p.SetStartCap(LineCapRound);p.SetEndCap(LineCapRound);p.SetLineJoin(LineJoinRound);
 auto line=[&](int x,int y,int xx,int yy){g.DrawLine(&p,x,y,xx,yy);};
 auto rect=[&](int x,int y,int w,int h){g.DrawRectangle(&p,x,y,w,h);};
 auto ellipse=[&](int x,int y,int w,int h){g.DrawEllipse(&p,x,y,w,h);};
 auto keys=[&](int x,int y,int w,int h){rect(x,y,w,h);for(int i=1;i<7;++i)line(x+i*w/7,y,x+i*w/7,y+h);};
 switch(category){
 case 0:{GraphicsPath path;path.AddBezier(16,68,10,42,34,16,64,16);path.AddBezier(64,16,91,16,90,47,84,68);path.AddLine(84,68,16,68);g.DrawPath(&p,&path);keys(16,68,68,16);line(25,84,25,94);line(77,84,77,94);break;}
 case 1:rect(10,24,80,54);keys(14,48,72,26);rect(38,31,26,9);line(19,34,26,34);line(74,34,81,34);line(25,78,19,91);line(75,78,81,91);break;
 case 2:for(int i=0;i<5;++i){int x=16+i*15,y=12+std::abs(2-i)*10;rect(x,y,9,54-y);}keys(10,60,80,24);line(19,91,81,91);break;
 case 3:ellipse(30,42,40,44);ellipse(37,30,26,27);rect(46,8,8,47);line(33,50,64,81);line(17,85,83,15);line(18,90,87,21);break;
 case 4:{Point horn[]{{12,49},{65,49},{88,27},{88,77},{65,59},{12,59}};g.DrawPolygon(&p,horn,6);for(int i=0;i<3;++i){int x=30+i*12;line(x,47,x,29);line(x-4,29,x+4,29);}g.DrawArc(&p,18,54,42,30,0,180);break;}
 case 5:line(10,53,23,53);line(23,53,23,24);line(23,24,49,24);line(49,24,49,77);line(49,77,75,77);line(75,77,75,45);line(75,45,90,45);break;
 case 6:for(int i=0;i<3;++i){GraphicsPath wave;wave.AddBezier(10,31+i*19,30,9+i*19,60,53+i*19,90,31+i*19);g.DrawPath(&p,&wave);}break;
 case 7:ellipse(18,36,64,20);g.DrawArc(&p,18,58,64,22,0,180);line(18,46,18,68);line(82,46,82,68);line(28,51,32,74);line(49,55,49,77);line(70,51,66,74);line(25,12,65,39);line(75,12,35,39);break;
 case 8:ellipse(32,43,38,38);ellipse(12,22,32,26);ellipse(56,22,32,26);line(8,16,31,16);line(19,16,19,67);line(72,12,95,12);line(84,12,84,70);line(38,79,30,91);line(63,79,72,91);break;
 case 9:rect(9,25,20,52);keys(12,28,14,46);rect(31,29,37,44);for(int i=0;i<5;++i)line(34+i*7,30,34+i*7,72);rect(70,25,21,52);for(int y=35;y<71;y+=12)ellipse(77,y,4,4);break;
 case 10:line(40,25,40,70);line(40,25,77,15);line(77,15,77,59);ellipse(22,67,18,12);ellipse(59,57,18,12);break;
 case 11:case 12:ellipse(18,53,36,34);ellipse(28,41,27,28);line(46,57,78,18);line(51,60,83,21);rect(74,10,13,15);line(33,68,80,17);if(category==11){line(25,61,41,75);line(60,35,71,44);}else ellipse(32,58,10,10);break;
 case 13:rect(13,13,31,31);rect(56,13,31,31);rect(13,56,31,31);rect(56,56,31,31);line(23,28,34,28);line(71,23,71,34);line(23,71,34,71);line(66,71,77,71);break;
 case 14:{rect(12,32,76,50);line(12,32,12,20);line(12,20,42,20);line(42,20,52,32);line(50,43,50,70);line(37,57,63,57);break;}
 case 15:{PointF star[10];for(int i=0;i<10;++i){double a=-1.57079632679+i*3.14159265359/5;float radius=i%2?19.f:42.f;star[i]=PointF(50+static_cast<float>(std::cos(a))*radius,50+static_cast<float>(std::sin(a))*radius);}g.DrawPolygon(&p,star,10);break;}
 }
}
inline void studioKeyboard(HDC dc,RECT r,int category){
 int w=r.right-r.left,h=r.bottom-r.top;if(w<24||h<24)return;
 int y=r.top+h/4;RECT body{r.left,y,r.right,r.top+h*3/4};
 surface(dc,body,RGB(52,61,74),RGB(15,19,27),RGB(122,135,151),8);
 int margin=std::max(4,w/25),keyTop=y+(body.bottom-y)*2/5;
 int keys=w>200?21:10;int keyWidth=(w-margin*2)/keys;
 for(int i=0;i<keys;++i){RECT k{r.left+margin+i*keyWidth,keyTop,r.left+margin+(i+1)*keyWidth-1,body.bottom-margin};fill(dc,k,RGB(234,235,230));
  if(i%7!=2&&i%7!=6&&i<keys-1){RECT black{k.right-keyWidth/4,keyTop,k.right+keyWidth/3,keyTop+(body.bottom-margin-keyTop)*3/5};fill(dc,black,RGB(9,12,17));}}
 RECT display{r.left+w*2/5,y+margin,r.left+w*3/5,keyTop-margin};fill(dc,display,category==0?RGB(83,64,37):RGB(34,83,93));
 for(int i=0;i<4;++i){int x=r.left+margin+i*std::max(5,w/20);fill(dc,{x,y+margin,x+std::max(2,w/80),keyTop-margin},gold);}
 line(dc,r.left+margin,body.bottom+3,r.right-margin,body.bottom+3,RGB(4,6,10),3);
}
// Distinct vector drawings, rendered at the available size (no emoji/font dependency).
inline void instrument(HDC dc,RECT r,int category){
 if(category==0||category==1||category==2||category==5||category==6||category==9||category==13){studioKeyboard(dc,r,category);return;}
 int cx=(r.left+r.right)/2,cy=(r.top+r.bottom)/2;int w=std::min(r.right-r.left,r.bottom-r.top)*3/4,h=w*2/3;
 RECT body{cx-w/2,cy-h/2,cx+w/2,cy+h/2};
 auto brush=CreateSolidBrush(gold);auto pen=CreatePen(PS_SOLID,5,gold);auto ob=SelectObject(dc,brush),op=SelectObject(dc,pen);
 if(category==15){POINT star[10];for(int i=0;i<10;++i){double a=-1.57079632679+i*3.14159265359/5;int radius=i%2?w/5:w/2;star[i]={cx+static_cast<LONG>(std::cos(a)*radius),cy+static_cast<LONG>(std::sin(a)*radius)};}Polygon(dc,star,10);}
 else if(category==3||category==11||category==12){Ellipse(dc,cx-w/3,cy,cx+w/3,cy+h/2);Ellipse(dc,cx-w/4,cy-h/5,cx+w/4,cy+h/4);Rectangle(dc,cx-14,cy-h/2,cx+14,cy+h/3);for(int i=-1;i<=1;++i)line(dc,cx+i*5,cy-h/2,cx+i*5,cy+h/3,ink,2);}
 else if(category==8){Ellipse(dc,cx-w/4,cy-h/4,cx+w/4,cy+h/4);Ellipse(dc,cx-w/2,cy-h/2,cx-12,cy);Ellipse(dc,cx+12,cy-h/2,cx+w/2,cy);line(dc,cx-w/2,cy+h/2,cx-w/5,cy-h/4);line(dc,cx+w/2,cy+h/2,cx+w/5,cy-h/4);}
 else if(category==4){POINT horn[]{{cx-w/2,cy+20},{cx+w/5,cy+20},{cx+w/2,cy-h/2},{cx+w/2,cy+h/2},{cx+w/5,cy+50},{cx-w/2,cy+50}};Polygon(dc,horn,6);for(int i=0;i<3;++i)line(dc,cx-40+i*35,cy+20,cx-40+i*35,cy-40);}
 else if(category==7){Ellipse(dc,body.left,body.top,body.right,body.bottom);line(dc,cx-w/3,cy-h/2,cx+w/3,cy+h/2,ink,8);line(dc,cx+w/3,cy-h/2,cx-w/3,cy+h/2,ink,8);}
 else if(category==14){Rectangle(dc,body.left,body.top+30,body.right,body.bottom);Rectangle(dc,body.left,body.top,body.left+w/2,body.top+40);line(dc,cx,cy-20,cx,cy+50,ink,7);line(dc,cx-35,cy+15,cx+35,cy+15,ink,7);}
 else {
  Rectangle(dc,body.left,body.top,body.right,body.bottom);
  int keys=category==13?4:12;
  for(int i=0;i<keys;++i){int x=body.left+i*w/keys;RECT key{x+3,cy,body.left+(i+1)*w/keys-2,body.bottom-7};fill(dc,key,white);if(i%7!=2&&i%7!=6){RECT black{key.right-8,cy,key.right+8,cy+h/3};fill(dc,black,ink);}}
  if(category==9)for(int i=0;i<8;++i)line(dc,body.left+30+i*(w-60)/8,body.top+10,body.left+30+i*(w-60)/8,cy-12,ink,5);
  else for(int i=0;i<4;++i){RECT knob{body.left+20+i*45,body.top+16,body.left+40+i*45,body.top+36};fill(dc,knob,ink);}
 }
 SelectObject(dc,op);SelectObject(dc,ob);DeleteObject(pen);DeleteObject(brush);
}
inline std::unique_ptr<Gdiplus::Image> portrait(HINSTANCE instance){
 auto resource=FindResourceW(instance,MAKEINTRESOURCEW(2),RT_RCDATA);if(!resource)return {};
 auto size=SizeofResource(instance,resource);auto data=LockResource(LoadResource(instance,resource));auto memory=GlobalAlloc(GMEM_MOVEABLE,size);if(!memory)return {};
 auto ptr=GlobalLock(memory);memcpy(ptr,data,size);GlobalUnlock(memory);IStream* stream=nullptr;
 if(CreateStreamOnHGlobal(memory,TRUE,&stream)!=S_OK){GlobalFree(memory);return {};}
 auto source=std::unique_ptr<Gdiplus::Image>(Gdiplus::Image::FromStream(stream));
 std::unique_ptr<Gdiplus::Image> result;if(source&&source->GetLastStatus()==Gdiplus::Ok){auto bitmap=std::make_unique<Gdiplus::Bitmap>(source->GetWidth(),source->GetHeight(),PixelFormat32bppARGB);{Gdiplus::Graphics graphics(bitmap.get());graphics.DrawImage(source.get(),0,0);}result=std::move(bitmap);}source.reset();stream->Release();return result;
}
inline void photo(HDC dc,Gdiplus::Image* image,RECT r,bool round=false){
 if(!image)return;
 Gdiplus::Graphics g(dc);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
 Gdiplus::GraphicsPath path;if(round){path.AddEllipse(Gdiplus::Rect(r.left,r.top,r.right-r.left,r.bottom-r.top));g.SetClip(&path);}
 float sw=static_cast<float>(image->GetWidth()),sh=static_cast<float>(image->GetHeight());float target=static_cast<float>(r.right-r.left)/(r.bottom-r.top),source=sw/sh;
 float cw=source>target?sh*target:sw,ch=source>target?sh:sw/target;
 float sx=(sw-cw)/2,sy=(sh-ch)/2;if(round){cw=sw*.62f;ch=std::min(sh,cw);sx=(sw-cw)/2;sy=0;}
 g.DrawImage(image,Gdiplus::Rect(r.left,r.top,r.right-r.left,r.bottom-r.top),sx,sy,cw,ch,Gdiplus::UnitPixel);
 if(round){g.ResetClip();Gdiplus::Pen p(Gdiplus::Color(255,234,196,113),3);g.DrawEllipse(&p,Gdiplus::Rect(r.left+2,r.top+2,r.right-r.left-4,r.bottom-r.top-4));}
}
}
