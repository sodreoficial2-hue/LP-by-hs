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
  auto deadline=GetTickCount64()+60000;
  while(app.loading&&GetTickCount64()<deadline){while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(10);}
  assert(!app.loading&&app.engine&&!app.engine->loaded());
  // Test writes must never replace the musician's permanent Performance library.
  auto testSettings=app.base/L"vision-adapter-test-settings";std::filesystem::create_directories(testSettings);app.settingsPath=testSettings/L"audio.settings";
  auto storePath=testSettings/L"performances.lphs";std::filesystem::remove(storePath);app.performanceStore.load(storePath);app.show.performanceLibrary.clear();
  std::filesystem::remove(app.parameterMemoryPath());std::filesystem::remove(app.parameterMemoryPath().wstring()+L".backup");app.loadParameterMemory();
  auto command=[&](const wchar_t* action,int a=0,int b=0,const wchar_t* text=L""){app.visionCommand(L"LP1\t1\t"+std::wstring(action)+L"\t"+std::to_wstring(a)+L"\t"+std::to_wstring(b)+L"\t"+text);assert(app.visionLastError.empty());auto limit=GetTickCount64()+10000;while(app.engine->performanceLoading()&&GetTickCount64()<limit){while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(1);}assert(!app.engine->performanceLoading());};
  command(L"category",1);assert(app.screen==lp::Screen::Instruments&&!app.engine->loaded());
  command(L"select",0);assert(app.previewActive&&app.engine->loaded());auto oldReference=app.previewPatch.sfz;
  command(L"category",2);assert(app.previewPatch.sfz==oldReference&&app.engine->loaded());
  command(L"category",1);command(L"select",0);command(L"favorite",0);assert(app.show.favorites[0]);
  command(L"effects");assert(app.effects);command(L"parameter",1,420);assert(app.show.favorites[0]->parameters[1]==.42f);
  command(L"parameter",15,350);assert(app.show.favorites[0]->parameters[15]==.35f);command(L"master",350);assert(app.show.master==.35f);
  app.visionCommand(L"LP1\t1\tparameter\t15\t1001\t");assert(!app.visionLastError.empty()&&app.show.master==.35f);
  for(int tick=0;tick<1000;++tick)SendMessageW(app.window,WM_TIMER,1,0);assert(app.effects);
  command(L"escape");assert(app.effects);
  command(L"effects");assert(app.screen==lp::Screen::Home&&!app.effects);
  command(L"screen",2);command(L"performanceMode",1);command(L"performancePart",0,1);command(L"performancePart",1,2);command(L"performanceName",0,0,L"Apresenta%C3%A7%C3%A3o%20Soul");command(L"performancePlay");
  assert(app.editingPerformanceControls()&&app.editingPerformance.parts[0].high==47&&app.editingPerformance.parts[2].low==0);
  assert(app.show.performanceLibrary.empty()); // Audition never saves implicitly.
  command(L"partOctave",1,5);assert(app.editingPerformance.parts[0].transpose==0&&app.editingPerformance.parts[1].transpose==12);
  assert(SendMessageW(app.item(PartOctaveBase+1),CB_GETCURSEL,0,0)==5);
  command(L"editPart",1);command(L"effects");assert(app.editingPerformanceControls()&&!app.performanceControls());
  command(L"parameter",1,430);command(L"parameter",6,620);assert(app.editingPerformance.parts[1].patch.parameters[1]==.43f&&app.editingPerformance.parts[1].patch.adsrEnabled);
  assert(app.editingPerformance.parts[0].patch.parameters[1]!=.43f);
  app.learningPerformanceParameter=3;app.midiEvent(0x7f16b0);assert(app.editingPerformance.parts[1].patch.cc[3]==22&&app.editingPerformance.parts[1].patch.parameters[3]==1.f);assert(app.editingPerformance.parts[0].patch.cc[3]==-1);
  command(L"effects");assert(app.screen==lp::Screen::PerformanceEditor&&!app.effects);
  command(L"editPart",0);command(L"effects");command(L"parameter",11,810);command(L"effects");command(L"performanceSave");
  assert(!app.editingPerformanceValid&&!app.engine->loaded()&&!app.editorDraft);
  for(auto control:app.performancePartControls)assert(SendMessageW(control,CB_GETCURSEL,0,0)==0);
  for(int i=0;i<4;++i)assert(SendMessageW(app.item(PartOctaveBase+i),CB_GETCURSEL,0,0)==4&&!IsWindowEnabled(app.item(PartOctaveBase+i)));
  lp::PerformanceStore reopened;reopened.load(app.settingsPath.parent_path()/L"performances.lphs");lp::Session persisted;reopened.mergeInto(persisted);assert(persisted.performanceLibrary.size()==1&&persisted.performanceLibrary[0].parts[1].transpose==12);
  assert(app.show.performanceLibrary[0].parts[0].patch.parameters[11]==.81f&&app.show.performanceLibrary[0].parts[1].patch.parameters[6]==.62f);
  app.editSavedPerformance(0,false);assert(!app.workspace.original.empty()&&app.editingPerformance.parts[1].transpose==12);
  command(L"partVolume",0,780);command(L"partSolo",0);assert(app.workspace.solo==0);command(L"partSolo",0);command(L"partMute",1);assert(app.workspace.muted[1]);command(L"partMute",1);
  command(L"partCategory",0,1);command(L"partSearch",0,0,L"Rhodes");app.saveEditorDraft();command(L"category",1);command(L"screen",2);assert(app.workspace.searches[0]=="Rhodes"&&app.workspace.categories[0]==1&&app.editingPerformance.parts[0].patch.parameters[0]==.78f);
  command(L"copyPart",0,2);assert(app.editingPerformance.parts[2].instrument==app.editingPerformance.parts[0].instrument&&app.editingPerformance.parts[2].patch.parameters[0]==.78f);
  command(L"swapParts",1,3);assert(app.editingPerformance.parts[3].enabled);
  command(L"learnLow",3);app.midiEvent(0x7f3c90);assert(app.workspace.customZones&&app.editingPerformance.parts[3].low==60);command(L"learnHigh",3);app.midiEvent(0x7f4890);assert(app.editingPerformance.parts[3].high==72);command(L"swapParts",1,3);
  // Return the original volume before committing the edit for existing mix checks.
  command(L"partVolume",0,1000);command(L"performanceSave");assert(app.show.performanceLibrary.size()==1);
  app.editSavedPerformance(0,true);assert(app.workspace.original.empty());command(L"performanceSaveNew");assert(app.show.performanceLibrary.size()==2);
  command(L"category",13);command(L"select",0);command(L"effects");assert(app.performanceControls());
  command(L"parameter",0,270);command(L"effects");command(L"activateFavorite",0);command(L"category",13);command(L"select",0);
  assert(app.editingPerformance.parts[0].patch.parameters[0]==.27f); // live mix is now permanent outside favorites, too
  command(L"favorite",0);assert(app.show.favorites[1]&&app.show.current==1);
  for(int volume:{310,740,180}){
   command(L"effects");command(L"parameter",0,volume);command(L"parameter",1,650);command(L"effects");
   command(L"activateFavorite",0);command(L"activateFavorite",1);
   assert(app.editingPerformance.parts[0].patch.parameters[0]==volume/1000.f&&app.editingPerformance.parts[1].patch.parameters[0]==.65f);
   assert(app.show.performanceLibrary[0].parts[0].patch.parameters[0]==1.f);
  }
  const auto profile=app.base/L"performance-recall-test.lphs";app.show.save(profile);lp::Session restored;restored.load(profile);assert(restored.favorites[1]->performanceMix->volumes[0]==.18f);
  command(L"activateFavorite",0);command(L"screen",6);command(L"noteOn",60,100);assert(app.visionNotes[60]);command(L"releaseNotes");assert(!app.visionNotes[60]);
  // Mouse, Vision and MIDI use the same permanent per-instrument memory.
  app.show.favorites[0].reset();app.show.current.reset();app.previewActive=false;
  command(L"category",1);command(L"select",0);command(L"effects");assert(app.previewActive&&!app.show.current);
  app.savedProfile=app.profileSnapshot();
  for(int index=0;index<16;++index)command(L"parameter",index,100+index*40);
  app.learningPerformanceParameter=6;app.midiEvent(0x5b28b0);assert(app.previewPatch.cc[6]==40&&app.previewPatch.parameters[6]==91/127.f);
  command(L"master",610);assert(!app.profileDirty());auto expected=app.previewPatch;
  app.flushParameterMemory();lp::SoundParameterStore memoryAfterExit;memoryAfterExit.load(app.parameterMemoryPath());
  // Throw away every in-memory patch, as at a new program launch. No profile load.
  app.soundParameters=memoryAfterExit;app.previewPatch={};app.previewActive=false;app.show.current.reset();app.show.master=1;app.soundParameters.restoreSession(app.show,app.base);
  command(L"effects");command(L"category",1);command(L"select",0);
  assert(app.previewPatch.parameters==expected.parameters&&app.previewPatch.adsrEnabled&&app.previewPatch.cc==expected.cc&&app.show.master==.61f);
  command(L"favorite",0);assert(app.show.current);auto slot=*app.show.current;command(L"effects");command(L"parameter",6,250);command(L"effects");
  app.flushParameterMemory();lp::SoundParameterStore reopenedControls;reopenedControls.load(app.parameterMemoryPath());lp::Patch fresh;assert(reopenedControls.restore(lp::soundParameterKey(expected.sfz,app.base),fresh)&&fresh.parameters[6]==.25f);
  // Removing a favorite cannot erase the instrument's permanent sound settings.
  app.show.favorites[slot].reset();app.show.current.reset();app.previewActive=false;command(L"category",1);command(L"select",0);assert(app.previewPatch.parameters[6]==.25f);
  auto json=app.visionState();assert(json.find("\"version\":1")!=json.npos&&json.find("\"favorites\":[")!=json.npos);
  std::ofstream(app.base/L"vision-native-test-result.txt")<<"PASS: actual App bridge commands; catalog browsing preserves sound; favorite parameters and master; malformed command rejection; Performance split/name/save/mix; virtual note release. PortAudio/sfizz test doubles. No WebView2 or hardware validation.\n";
  // Startup must choose the first visible acoustic piano, wait for the worker,
  // and keep Home open. A hidden first piano must never be resurrected.
  app.show.current.reset();app.show.favorites={};app.previewActive=false;
  app.screen=lp::Screen::Home;app.effects=false;app.pendingRestore=false;
  app.instruments.clear();
  for(int i=0;i<3;++i){auto file=testSettings/(L"startup-"+std::to_wstring(i)+L".sfz");std::ofstream(file)<<"<region> sample=*sine\n";lp::Instrument instrument;instrument.sfz=file;instrument.name="Startup "+std::to_string(i);instrument.category=i?0:1;app.instruments.push_back(instrument);}
  app.hiddenSounds.exclude(lp::soundExclusionKey(lp::instrumentReference(app.instruments[1]),app.base),testSettings/L"hidden-test.catalog");
  app.loading=true;app.completeLibrary(L"Startup regression");
  assert(app.startupPianoPending&&app.loading&&app.categoryIndex==0);
  deadline=GetTickCount64()+10000;
  while(app.loading&&GetTickCount64()<deadline){while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}app.updateIndicators();Sleep(1);}
  assert(!app.loading&&app.engine->loaded()&&app.previewActive&&app.screen==lp::Screen::Home);
  assert(app.previewPatch.sfz==lp::instrumentReference(app.instruments[2]));
  app.lastMidiActivity.store(GetTickCount64());assert(app.midiActive());
  app.lastMidiActivity.store(GetTickCount64()-300);assert(!app.midiActive());
  app.engine->unload();app.instruments.clear();app.previewActive=false;
  app.loading=true;app.completeLibrary(L"Empty startup regression");
  assert(!app.loading&&!app.startupPianoPending&&!app.engine->loaded());
  DestroyWindow(window);
 }Gdiplus::GdiplusShutdown(token);CoUninitialize();return 0;
}
