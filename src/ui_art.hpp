#pragma once
#include <gdiplus.h>
#include <memory>
namespace lpui {
constexpr COLORREF ink=RGB(9,12,18),panel=RGB(20,27,38),gold=RGB(234,196,113),white=RGB(255,255,255),muted=RGB(199,211,227);
inline void fill(HDC dc,RECT rect,COLORREF color){auto b=CreateSolidBrush(color);FillRect(dc,&rect,b);DeleteObject(b);}
inline void text(HDC dc,const std::wstring& label,RECT rect,HFONT font,COLORREF color=white,UINT flags=DT_CENTER|DT_VCENTER|DT_SINGLELINE){auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,label.c_str(),-1,&rect,flags);SelectObject(dc,old);}
inline void line(HDC dc,int x,int y,int xx,int yy,COLORREF color=gold,int width=4){auto p=CreatePen(PS_SOLID,width,color);auto old=SelectObject(dc,p);MoveToEx(dc,x,y,nullptr);LineTo(dc,xx,yy);SelectObject(dc,old);DeleteObject(p);}
inline void box(HDC dc,RECT r,COLORREF color=gold,int width=4){line(dc,r.left,r.top,r.right,r.top,color,width);line(dc,r.right,r.top,r.right,r.bottom,color,width);line(dc,r.right,r.bottom,r.left,r.bottom,color,width);line(dc,r.left,r.bottom,r.left,r.top,color,width);}
// Distinct vector drawings, rendered at the available size (no emoji/font dependency).
inline void instrument(HDC dc,RECT r,int category){
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
