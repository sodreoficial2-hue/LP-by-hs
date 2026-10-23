// Native Win32 integration test. PortAudio is a test double; sfizz can be real.
#define wWinMain lp_application_entry
#include "../src/win32_app.cpp"
#undef wWinMain
#include <cassert>
static void pump(){MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}}
static void capture(HWND w,const std::filesystem::path& path,bool process=true){
 if(process){pump();}
 RedrawWindow(w,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
 RECT r{};GetClientRect(w,&r);auto dc=GetDC(w),mem=CreateCompatibleDC(dc);auto bmp=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(mem,bmp);BitBlt(mem,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 std::vector<unsigned char> bytes(static_cast<std::size_t>(r.right)*r.bottom*4);SelectObject(mem,old);GetDIBits(dc,bmp,0,r.bottom,bytes.data(),&info,DIB_RGB_COLORS);
 BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+static_cast<DWORD>(bytes.size());
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(&header),sizeof(header));out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader));out.write(reinterpret_cast<char*>(bytes.data()),bytes.size());DeleteObject(bmp);DeleteDC(mem);ReleaseDC(w,dc);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
 INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};InitCommonControlsEx(&common);Gdiplus::GdiplusStartupInput input;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&input,nullptr);
 {App app;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LPLayoutTest";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
 auto w=CreateWindowW(cls.lpszClassName,L"Live Performance layout test",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,1360,900,nullptr,nullptr,instance,&app);assert(w);ShowWindow(w,SW_SHOW);UpdateWindow(w);
 capture(w,app.base/L"layout-splash.bmp",false);pump();assert(!app.loading&&app.engine&&app.photo&&!app.instruments.empty());capture(w,app.base/L"layout-home.bmp");
 MSG key{};key.message=WM_KEYDOWN;key.wParam=VK_RIGHT;int before=app.categoryIndex;assert(app.keyCommand(key)&&app.categoryIndex==(before+1)%16);app.chooseCategory(-1);key.wParam=VK_RETURN;app.keyCommand(key);assert(app.screen==lp::Screen::Instruments);capture(w,app.base/L"layout-list.bmp");
 app.audition();app.favorite();assert(app.show.favorites[0]);app.toggleParameters();assert(app.effects&&IsWindowVisible(app.sliders[15]));app.setPanelValue(15,.4f);assert(app.show.master==.4f);key.wParam=VK_ESCAPE;app.keyCommand(key);assert(app.effects);capture(w,app.base/L"layout-parameters.bmp");app.toggleParameters();assert(!app.effects&&app.screen==lp::Screen::Home);
 app.command(PerformanceButton,0);assert(app.screen==lp::Screen::PerformanceEditor);SendMessageW(app.performancePartControls[0],CB_SETCURSEL,1,0);SendMessageW(app.performancePartControls[1],CB_SETCURSEL,2,0);SendMessageW(app.performanceMode,CB_SETCURSEL,1,0);app.command(PerformancePlay,0);assert(app.performanceControls()&&app.editingPerformance.parts[0].high==47);capture(w,app.base/L"layout-editor.bmp");app.command(PerformanceSave,0);assert(app.show.performanceLibrary.size()==1);
 app.toggleParameters();assert(IsWindowVisible(app.sliders[3])&&!IsWindowVisible(app.sliders[4])&&!IsWindowVisible(app.sliders[15]));app.setPanelValue(0,.32f);assert(app.show.performanceLibrary[0].parts[0].patch.parameters[0]==.32f);capture(w,app.base/L"layout-volumes.bmp");app.parameterPanel.lastUse=GetTickCount64()-30001;SendMessageW(w,WM_TIMER,1,0);assert(!app.effects&&app.screen==lp::Screen::Home);
 app.activate(0);assert(!app.performanceControls());app.openScreen(lp::Screen::AudioMidi);capture(w,app.base/L"layout-audio.bmp");app.openScreen(lp::Screen::Settings);capture(w,app.base/L"layout-settings.bmp");app.openScreen(lp::Screen::About);capture(w,app.base/L"layout-about.bmp");
 std::ofstream result(app.base/L"layout-test-result.txt");result<<"PASS: native Win32 screens, portraits, arrows/Enter/ESC/E, 16 controls, Performance volumes, B2 split, Master, favorites and idle timer. PortAudio test double; no hardware validation.\n";DestroyWindow(w);
 }Gdiplus::GdiplusShutdown(token);return 0;
}
