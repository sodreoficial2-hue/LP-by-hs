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
inline std::unique_ptr<Gdiplus::Image> resourceImage(HINSTANCE instance,int id){
 auto resource=FindResourceW(instance,MAKEINTRESOURCEW(id),RT_RCDATA);if(!resource)return {};
 auto size=SizeofResource(instance,resource);auto data=LockResource(LoadResource(instance,resource));auto memory=GlobalAlloc(GMEM_MOVEABLE,size);if(!memory)return {};
 auto ptr=GlobalLock(memory);memcpy(ptr,data,size);GlobalUnlock(memory);IStream* stream=nullptr;
 if(CreateStreamOnHGlobal(memory,TRUE,&stream)!=S_OK){GlobalFree(memory);return {};}
 auto source=std::unique_ptr<Gdiplus::Image>(Gdiplus::Image::FromStream(stream));
 std::unique_ptr<Gdiplus::Image> result;if(source&&source->GetLastStatus()==Gdiplus::Ok){auto bitmap=std::make_unique<Gdiplus::Bitmap>(source->GetWidth(),source->GetHeight(),PixelFormat32bppARGB);{Gdiplus::Graphics graphics(bitmap.get());graphics.DrawImage(source.get(),0,0);}result=std::move(bitmap);}source.reset();stream->Release();return result;
}
inline std::unique_ptr<Gdiplus::Image> portrait(HINSTANCE instance){return resourceImage(instance,2);}
inline std::wstring resourceText(HINSTANCE instance,int id){
 auto resource=FindResourceW(instance,MAKEINTRESOURCEW(id),RT_RCDATA);if(!resource)return {};
 auto bytes=static_cast<int>(SizeofResource(instance,resource));auto data=static_cast<const char*>(LockResource(LoadResource(instance,resource)));
 int length=MultiByteToWideChar(CP_UTF8,0,data,bytes,nullptr,0);std::wstring result(length,L'\0');if(length)MultiByteToWideChar(CP_UTF8,0,data,bytes,result.data(),length);return result;
}
// All photos are decoded once from one embedded atlas; painting never accesses disk/network.
inline void categoryPhoto(HDC dc,Gdiplus::Image* atlas,RECT r,int category){
 if(!atlas||category<0||category>=16||r.right<=r.left||r.bottom<=r.top)return;
 Gdiplus::Graphics g(dc);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
 int cw=static_cast<int>(atlas->GetWidth()/4),ch=static_cast<int>(atlas->GetHeight()/4);
 // Fit the full image without stretching or cutting off recognisable instrument parts.
 double scale=std::min(static_cast<double>(r.right-r.left)/cw,static_cast<double>(r.bottom-r.top)/ch);
 int w=static_cast<int>(cw*scale),h=static_cast<int>(ch*scale);r.left+=(r.right-r.left-w)/2;r.top+=(r.bottom-r.top-h)/2;r.right=r.left+w;r.bottom=r.top+h;
 // The atlas keeps a generous frame around each instrument to preserve recognition.
 g.DrawImage(atlas,Gdiplus::Rect(r.left,r.top,r.right-r.left,r.bottom-r.top),static_cast<INT>((category%4)*cw),static_cast<INT>((category/4)*ch),cw,ch,Gdiplus::UnitPixel);
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
