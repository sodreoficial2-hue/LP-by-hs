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
 capture(w,app.base/L"layout-splash.bmp",false);pump();assert(!app.loading&&app.engine&&app.photo&&app.categoryPhotos&&!app.instruments.empty());assert(app.categoryPhotos->GetWidth()==2048&&app.categoryPhotos->GetHeight()==1536);assert(!lpui::resourceText(instance,5).empty());capture(w,app.base/L"layout-home.bmp");
 // All sixteen categories are direct buttons, fully within one viewport.
 auto checkGrid=[&]{assert(app.pageScroll.maximum()==0);RECT viewport{};GetWindowRect(app.page,&viewport);
  for(int i=0;i<16;++i){auto card=app.item(CategoryCardBase+i);assert(card&&IsWindowVisible(card));assert(GetWindowLongPtrW(card,GWL_STYLE)&WS_TABSTOP);
   RECT bounds{};GetWindowRect(card,&bounds);assert(bounds.left>=viewport.left&&bounds.right<=viewport.right&&bounds.top>=viewport.top&&bounds.bottom<=viewport.bottom);}};
 checkGrid();
 bool wasLoaded=app.engine->loaded();auto previousPatch=app.previewPatch.sfz;
 // Merely browsing, keyboard focus, and notification messages must never load audio.
 MSG key{};key.message=WM_KEYDOWN;key.wParam=VK_RIGHT;int selected=app.categoryIndex;
 SetFocus(app.item(CategoryCardBase+5));assert(app.keyCommand(key));assert(GetFocus()==app.item(CategoryCardBase+6)&&app.categoryIndex==selected);
 app.command(CategoryCardBase+15,BN_SETFOCUS);assert(app.screen==lp::Screen::Home);
 app.command(CategoryCardBase+15,BN_CLICKED);assert(app.screen==lp::Screen::Instruments&&GetFocus()==app.favorites);
 assert(app.engine->loaded()==wasLoaded&&app.previewPatch.sfz==previousPatch);
 key.wParam=VK_RIGHT;assert(!app.keyCommand(key)&&app.categoryIndex==15);
 app.command(HomeButton,0);checkGrid();app.command(KeyboardButton,BN_CLICKED);assert(app.screen==lp::Screen::Keyboard);capture(w,app.base/L"layout-keyboard.bmp");app.command(HomeButton,0);
 // Choose a category containing at least two sounds, independently of the fixture ordering.
 int testCategory=-1;for(int c=0;c<15;++c){int n=0;for(const auto& instrument:app.instruments)if(instrument.category==static_cast<unsigned>(c))++n;if(n>=2){testCategory=c;break;}}
 assert(testCategory>=0);app.command(CategoryCardBase+testCategory,BN_CLICKED);
 assert(app.engine->loaded()==wasLoaded&&app.previewPatch.sfz==previousPatch);capture(w,app.base/L"layout-list.bmp");
 // Clicking an already highlighted sound still loads it; native up/down remain automatic.
 SendMessageW(app.catalog,LB_SETCURSEL,1,0);SendMessageW(app.catalog,WM_LBUTTONUP,0,MAKELPARAM(30,70));
 assert(app.previewPatch.sfz==lp::instrumentReference(app.instruments[app.visibleInstruments[1]]));
 SendMessageW(app.catalog,WM_KEYDOWN,VK_UP,0);assert(app.previewPatch.sfz==lp::instrumentReference(app.instruments[app.visibleInstruments[0]]));
 assert(SendMessageW(app.catalog,LB_GETCURSEL,0,0)==0);
 app.favorite();assert(app.show.favorites[0]);app.toggleParameters();assert(app.effects&&IsWindowVisible(app.sliders[15]));SetFocus(app.sliders[0]);key.wParam=VK_RIGHT;assert(!app.keyCommand(key));app.setPanelValue(15,.4f);assert(app.show.master==.4f);key.wParam=VK_ESCAPE;app.keyCommand(key);assert(app.effects);capture(w,app.base/L"layout-parameters.bmp");app.toggleParameters();assert(!app.effects&&app.screen==lp::Screen::Home);
 app.command(PerformanceButton,0);assert(app.screen==lp::Screen::PerformanceEditor);SendMessageW(app.performancePartControls[0],CB_SETCURSEL,1,0);SendMessageW(app.performancePartControls[1],CB_SETCURSEL,2,0);SendMessageW(app.performanceMode,CB_SETCURSEL,1,0);app.command(PerformancePlay,0);assert(app.performanceControls()&&app.editingPerformance.parts[0].high==47);capture(w,app.base/L"layout-editor.bmp");app.command(PerformanceSave,0);assert(app.show.performanceLibrary.size()==1);
 app.toggleParameters();assert(IsWindowVisible(app.sliders[3])&&!IsWindowVisible(app.sliders[4])&&!IsWindowVisible(app.sliders[15]));app.setPanelValue(0,.32f);assert(app.show.performanceLibrary[0].parts[0].patch.parameters[0]==.32f);capture(w,app.base/L"layout-volumes.bmp");app.parameterPanel.lastUse=GetTickCount64()-30001;SendMessageW(w,WM_TIMER,1,0);assert(!app.effects&&app.screen==lp::Screen::Home);
 app.activate(0);assert(!app.performanceControls());app.openScreen(lp::Screen::AudioMidi);capture(w,app.base/L"layout-audio.bmp");app.openScreen(lp::Screen::Settings);capture(w,app.base/L"layout-settings.bmp");app.openScreen(lp::Screen::About);capture(w,app.base/L"layout-about.bmp");
 // A consistent native header and large owner-drawn selectors across all forms.
 RECT headerReference{};GetWindowRect(app.item(SettingsButton),&headerReference);
 for(auto screen:{lp::Screen::AudioMidi,lp::Screen::Settings,lp::Screen::About,lp::Screen::Keyboard}){
  app.openScreen(screen);assert(app.pageTop()==156);RECT header{};GetWindowRect(app.item(SettingsButton),&header);
  assert(header.left==headerReference.left&&header.top==headerReference.top&&IsWindowVisible(app.item(SettingsButton)));
 }
 assert(GetWindowLongPtrW(app.audioDriver,GWL_STYLE)&CBS_OWNERDRAWFIXED);
 assert(SendMessageW(app.audioDriver,CB_GETITEMHEIGHT,static_cast<WPARAM>(-1),0)>=40);
 app.openScreen(lp::Screen::AudioMidi);SendMessageW(app.audioDriver,CB_SHOWDROPDOWN,TRUE,0);capture(w,app.base/L"layout-audio-dropdown.bmp");SendMessageW(app.audioDriver,CB_SHOWDROPDOWN,FALSE,0);
 // A 600-pixel window must expose all functions through the page scrollbar.
 MoveWindow(w,0,0,1024,600,TRUE);pump();app.goHome();pump();checkGrid();capture(w,app.base/L"layout-home-compact.bmp");app.openScreen(lp::Screen::Settings);SetFocus(w);
 RECT headerBefore{},headerAfter{},buttonBefore{},buttonAfter{};GetWindowRect(app.item(SettingsButton),&headerBefore);GetWindowRect(app.item(Open),&buttonBefore);
 key.wParam=VK_DOWN;assert(app.keyCommand(key)&&app.pageScroll.offset==48);
 GetWindowRect(app.item(SettingsButton),&headerAfter);GetWindowRect(app.item(Open),&buttonAfter);assert(headerBefore.top==headerAfter.top&&buttonAfter.top==buttonBefore.top-48);
 SCROLLINFO si{};si.cbSize=sizeof(si);si.fMask=SIF_POS;GetScrollInfo(app.page,SB_VERT,&si);assert(si.nPos==app.pageScroll.offset);
 SetFocus(app.item(ImportSf2));pump();RECT controlRect{},viewportRect{};GetWindowRect(app.item(ImportSf2),&controlRect);GetWindowRect(app.page,&viewportRect);assert(controlRect.top>=viewportRect.top&&controlRect.bottom<=viewportRect.bottom);
 app.command(PerformanceButton,0);SetFocus(app.performanceMode);key.wParam=VK_DOWN;assert(!app.keyCommand(key));
 SendMessageW(app.performanceMode,CB_SETCURSEL,0,0);SendMessageW(app.performanceMode,WM_KEYDOWN,VK_DOWN,0);assert(SendMessageW(app.performanceMode,CB_GETCURSEL,0,0)==1);
 app.openScreen(lp::Screen::About);SetFocus(w);assert(app.keyCommand(key)&&GetFocus()==app.item(AboutText));
 app.goHome();app.toggleParameters();SetFocus(w);key.wParam=VK_NEXT;assert(app.keyCommand(key)&&app.pageScroll.offset>0&&app.effects);app.toggleParameters();
 std::ofstream result(app.base/L"layout-test-result.txt");result<<"PASS: native Win32 page scroll/focus, automatic mouse/arrow sound loading, combo arrows, portraits, arrows/Enter/ESC/E, 16 controls, Performance volumes, B2 split, Master, favorites and idle timer. PortAudio test double; no hardware validation.\n";DestroyWindow(w);
 }Gdiplus::GdiplusShutdown(token);return 0;
}
