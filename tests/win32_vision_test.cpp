// Actual App command adapter, with PortAudio and sfizz test doubles. No hardware claim.
#define LP_VISION_UI
#define LP_VISION_TEST
#define wWinMain lp_vision_application_entry
#include "../src/win32_app.cpp"
#undef wWinMain
#include <cassert>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
 CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};InitCommonControlsEx(&common);
 Gdiplus::GdiplusStartupInput input;ULONG_PTR token;assert(Gdiplus::GdiplusStartup(&token,&input,nullptr)==Gdiplus::Ok);
 {App app;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LPVisionAdapterTest";RegisterClassW(&cls);
  auto window=CreateWindowW(cls.lpszClassName,L"Vision adapter regression",WS_OVERLAPPEDWINDOW,0,0,1360,900,nullptr,nullptr,instance,&app);assert(window);
  MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
  assert(!app.loading&&app.engine&&!app.engine->loaded());
  auto command=[&](const wchar_t* action,int a=0,int b=0,const wchar_t* text=L""){app.visionCommand(L"LP1\t1\t"+std::wstring(action)+L"\t"+std::to_wstring(a)+L"\t"+std::to_wstring(b)+L"\t"+text);assert(app.visionLastError.empty());};
  command(L"category",1);assert(app.screen==lp::Screen::Instruments&&!app.engine->loaded());
  command(L"select",0);assert(app.previewActive&&app.engine->loaded());auto oldReference=app.previewPatch.sfz;
  command(L"category",2);assert(app.previewPatch.sfz==oldReference&&app.engine->loaded());
  command(L"category",1);command(L"select",0);command(L"favorite",0);assert(app.show.favorites[0]);
  command(L"effects");assert(app.effects);command(L"parameter",1,420);assert(app.show.favorites[0]->parameters[1]==.42f);
  command(L"parameter",15,350);assert(app.show.master==.35f);
  app.visionCommand(L"LP1\t1\tparameter\t15\t1001\t");assert(!app.visionLastError.empty()&&app.show.master==.35f);
  for(int tick=0;tick<1000;++tick)SendMessageW(app.window,WM_TIMER,1,0);assert(app.effects);
  command(L"escape");assert(app.effects);
  command(L"effects");assert(app.screen==lp::Screen::Home&&!app.effects);
  command(L"screen",2);command(L"performanceMode",1);command(L"performancePart",0,1);command(L"performancePart",1,2);command(L"performanceName",0,0,L"Apresenta%C3%A7%C3%A3o%20Soul");command(L"performancePlay");
  assert(app.editingPerformanceControls()&&app.editingPerformance.parts[0].high==47&&app.editingPerformance.parts[2].low==0);
  command(L"performanceSave");assert(app.show.performanceLibrary.size()==1&&app.show.performanceLibrary[0].name=="Apresentação Soul");
  command(L"editPart",1);command(L"effects");assert(app.editingPerformanceControls()&&!app.performanceControls());
  command(L"parameter",1,430);command(L"parameter",6,620);assert(app.editingPerformance.parts[1].patch.parameters[1]==.43f&&app.editingPerformance.parts[1].patch.adsrEnabled);
  assert(app.editingPerformance.parts[0].patch.parameters[1]!=.43f);
  app.learningPerformanceParameter=3;app.midiEvent(0x7f16b0);assert(app.editingPerformance.parts[1].patch.cc[3]==22&&app.editingPerformance.parts[1].patch.parameters[3]==1.f);assert(app.editingPerformance.parts[0].patch.cc[3]==-1);
  command(L"effects");assert(app.screen==lp::Screen::PerformanceEditor&&!app.effects);
  command(L"editPart",0);command(L"effects");command(L"parameter",11,810);command(L"effects");command(L"performanceSave");
  assert(app.show.performanceLibrary[0].parts[0].patch.parameters[11]==.81f&&app.show.performanceLibrary[0].parts[1].patch.parameters[6]==.62f);
  command(L"category",13);command(L"select",0);command(L"effects");assert(app.performanceControls());
  command(L"parameter",0,270);command(L"effects");command(L"activateFavorite",0);command(L"category",13);command(L"select",0);
  assert(app.editingPerformance.parts[0].patch.parameters[0]==1.f); // unfavorited live mix discarded
  command(L"favorite",0);assert(app.show.favorites[1]&&app.show.current==1);
  for(int volume:{310,740,180}){
   command(L"effects");command(L"parameter",0,volume);command(L"parameter",1,650);command(L"effects");
   command(L"activateFavorite",0);command(L"activateFavorite",1);
   assert(app.editingPerformance.parts[0].patch.parameters[0]==volume/1000.f&&app.editingPerformance.parts[1].patch.parameters[0]==.65f);
   assert(app.show.performanceLibrary[0].parts[0].patch.parameters[0]==1.f);
  }
  const auto profile=app.base/L"performance-recall-test.lphs";app.show.save(profile);lp::Session restored;restored.load(profile);assert(restored.favorites[1]->performanceMix->volumes[0]==.18f);
  command(L"activateFavorite",0);command(L"screen",6);command(L"noteOn",60,100);assert(app.visionNotes[60]);command(L"releaseNotes");assert(!app.visionNotes[60]);
  auto json=app.visionState();assert(json.find("\"version\":1")!=json.npos&&json.find("\"favorites\":[")!=json.npos);
  std::ofstream(app.base/L"vision-native-test-result.txt")<<"PASS: actual App bridge commands; catalog browsing preserves sound; favorite parameters and master; malformed command rejection; Performance split/name/save/mix; virtual note release. PortAudio/sfizz test doubles. No WebView2 or hardware validation.\n";
  DestroyWindow(window);
 }Gdiplus::GdiplusShutdown(token);CoUninitialize();return 0;
}
