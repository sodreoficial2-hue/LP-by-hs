#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shlobj.h>
#include "session.hpp"
#include "instrument_catalog.hpp"
#include "midi.hpp"
#include "audio_host.hpp"
#include "sf2_reference.hpp"
#include "audio_preferences.hpp"
#include <memory>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
constexpr int Catalog=100,Favorites=101,Save=102,Open=103,Midi=104,Effects=105,Import=106,ImportSf2=114,AudioDriver=107,AudioBuffer=108,AudioStart=109,AudioStop=110,AudioPanel=111,Panic=112,Category=113,Knob=200,MidiLearnKnob=115,PerformanceButton=116,PerformanceModeControl=117,PerformancePartBase=118,PerformancePlay=122,PerformanceSave=123,PerformanceName=124,PerformanceEditPart=125,PerformanceClearBase=126;
constexpr UINT BeginParameterLearn=WM_APP+40;
// Do not rely on a native trackbar forwarding its mouse context request.
LRESULT CALLBACK parameterProc(HWND control,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR parameter,DWORD_PTR){
 switch(msg){
 case WM_RBUTTONDOWN:return 0;
 case WM_RBUTTONUP:case WM_CONTEXTMENU:
  SendMessageW(GetParent(control),BeginParameterLearn,parameter,0);return 0;
 case WM_KEYDOWN:case WM_SYSKEYDOWN:
  if(wp==VK_APPS||(wp==VK_F10&&(GetKeyState(VK_SHIFT)&0x8000))||(wp=='L'&&(GetKeyState(VK_CONTROL)&0x8000))){SendMessageW(GetParent(control),BeginParameterLearn,parameter,0);return 0;}break;
 case WM_KEYUP:case WM_SYSKEYUP:if(wp==VK_APPS||wp==VK_F10)return 0;break;
 case WM_NCDESTROY:RemoveWindowSubclass(control,parameterProc,parameter);break;
 }
 return DefSubclassProc(control,msg,wp,lp);
}
struct App {
 HWND window{},catalog{},favorites{},status{},midi{},effectsButton{},category{};
 std::array<HWND,14> sliders{},labels{};
 HFONT font{}; HBRUSH background=CreateSolidBrush(RGB(12,16,24));
 lp::Session show; lp::MidiRouter router{show};
 std::vector<lp::Instrument> instruments;
 std::vector<std::size_t> visibleInstruments;
 std::vector<std::size_t> visiblePerformances;
 int activePerformanceIndex=-1;
 lp::Patch previewPatch{};
 bool previewActive=false;
 std::unique_ptr<lp::AudioHost> engine;
 HWND audioDriver{},audioBuffer{},performanceMode{},performancePlay{},performanceName{},performanceEditPart{},performanceSave{};
 lp::Performance editingPerformance{};
 bool editingPerformanceValid=false;
 int editingPart=0;
 int learningPerformanceParameter=-1;
 std::array<HWND,4> performancePartControls{};
 std::array<HWND,4> performanceClearButtons{};
 bool performanceVisible=false;
 std::vector<lp::AudioDevice> audioDevices;
 const std::array<unsigned long,7> buffers{0,64,128,256,512,1024,2048};
 std::filesystem::path settingsPath;
 std::filesystem::path mySoundsPath() const {return settingsPath.parent_path()/L"my_sounds.catalog";}
 bool addMySound(lp::Instrument item){
  auto reference=lp::instrumentReference(item);
  for(const auto& existing:instruments)if(existing.category==14&&lp::instrumentReference(existing)==reference)return false;
  item.category=14;instruments.push_back(std::move(item));return true;
 }
 void saveMySounds(){
  if(settingsPath.empty())throw std::runtime_error("Pasta de Meus Sons indisponível");
  auto target=mySoundsPath(),temp=target;temp+=L".tmp";
  {
   std::ofstream out(temp,std::ios::binary|std::ios::trunc);
   if(!out)throw std::runtime_error("Não foi possível gravar Meus Sons");
   out<<"LPHS_MY_SOUNDS 1\n";
   for(const auto& item:instruments)if(item.category==14)
    out<<std::quoted(item.sfz.u8string())<<' '<<std::quoted(item.name)<<'\n';
   out.flush();if(!out)throw std::runtime_error("Falha ao salvar Meus Sons");
  }
  if(!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
   throw std::runtime_error("Não foi possível atualizar Meus Sons");
 }
 void loadMySounds(){
  if(settingsPath.empty())return;
  std::ifstream in(mySoundsPath(),std::ios::binary);
  if(!in)return;
  std::string header;if(!std::getline(in,header)||header!="LPHS_MY_SOUNDS 1")
   throw std::runtime_error("Catálogo Meus Sons inválido");
  std::string line;
  while(std::getline(in,line)){
   if(line.empty())continue;
   std::istringstream row(line);std::string reference,name,extra;
   if(!(row>>std::quoted(reference)>>std::quoted(name))||(row>>extra)||reference.empty()||name.empty())
    throw std::runtime_error("Entrada inválida em Meus Sons");
   lp::Instrument item;item.sfz=std::filesystem::u8path(reference);item.name=name;item.category=14;
   addMySound(std::move(item));
  }
 }

 std::string selectedMidiName() const {auto index=SendMessageW(midi,CB_GETCURSEL,0,0);if(index<=0)return {};MIDIINCAPSW caps{};return midiInGetDevCapsW(static_cast<UINT>(index-1),&caps,sizeof(caps))==MMSYSERR_NOERROR?std::filesystem::path(caps.szPname).u8string():std::string{};}
 void persistAudioAndMidi(){if(!engine||!engine->running())return;lp::rememberConnectedAudio(*engine,settingsPath);auto prefs=lp::AudioSettings::load(settingsPath);prefs.midi=selectedMidiName();prefs.save(settingsPath);}
 void openMidiSelection(){closeMidi();auto index=SendMessageW(midi,CB_GETCURSEL,0,0);if(index>0){auto result=midiInOpen(&midiHandle,static_cast<UINT>(index-1),reinterpret_cast<DWORD_PTR>(window),0,CALLBACK_WINDOW);if(result!=MMSYSERR_NOERROR)throw std::runtime_error("Cannot open MIDI input");if(midiInStart(midiHandle)!=MMSYSERR_NOERROR){closeMidi();throw std::runtime_error("Cannot start MIDI input");}}}
 bool pendingRestore=false;unsigned restoreAttempts=0;
 HMIDIIN midiHandle{};bool effects=false;
 std::filesystem::path base;
 ~App(){closeMidi();engine.reset();if(font)DeleteObject(font);DeleteObject(background);}
 void closeMidi(){router.cancelLearn();if(engine)engine->panic();if(midiHandle){midiInStop(midiHandle);midiInReset(midiHandle);midiInClose(midiHandle);midiHandle=nullptr;}}
 void message(const std::wstring& text){SetWindowTextW(status,text.c_str());}
 void error(const std::exception& e){auto s=std::string(e.what());std::wstring text(s.begin(),s.end());MessageBoxW(window,text.c_str(),L"Live Performance",MB_OK|MB_ICONERROR);}
 HWND control(const wchar_t* kind,const wchar_t* text,DWORD style,int id){
  auto h=CreateWindowExW(WS_EX_CLIENTEDGE,kind,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
  SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;
 }
 void initialize(HWND w){
  window=w;wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);base=std::filesystem::path(exe).parent_path();
  font=CreateFontW(-24,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  control(L"STATIC",L"Live Performance by Hugo Sodré — reconstrução",SS_LEFT,1);
  control(L"BUTTON",L"Abrir perfil",BS_PUSHBUTTON|WS_TABSTOP,Open);
  control(L"BUTTON",L"Salvar perfil",BS_PUSHBUTTON|WS_TABSTOP,Save);
  control(L"BUTTON",L"Importar SFZ",BS_PUSHBUTTON|WS_TABSTOP,Import);
  control(L"BUTTON",L"Importar SF2",BS_PUSHBUTTON|WS_TABSTOP,ImportSf2);
  effectsButton=control(L"BUTTON",L"Parâmetros (E)",BS_PUSHBUTTON|WS_TABSTOP,Effects);
  category=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,Category);
  for(const auto* name:lp::instrumentCategories){auto text=std::filesystem::u8path(name).wstring();SendMessageW(category,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));}
  SendMessageW(category,CB_SETCURSEL,0,0);
  catalog=control(L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP,Catalog);
  favorites=control(L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP,Favorites);
  midi=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,Midi);
  SendMessageW(midi,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"MIDI desligado"));
  for(UINT i=0;i<midiInGetNumDevs();++i){MIDIINCAPSW caps{};if(midiInGetDevCapsW(i,&caps,sizeof(caps))==MMSYSERR_NOERROR)SendMessageW(midi,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(caps.szPname));else SendMessageW(midi,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dispositivo indisponível"));}
  SendMessageW(midi,CB_SETCURSEL,0,0);
  status=control(L"STATIC",L"Selecione seu driver ASIO, clique em Conectar ASIO e escolha um timbre.",SS_LEFT,2);
  audioDriver=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,AudioDriver);
  audioBuffer=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,AudioBuffer);
  control(L"BUTTON",L"Conectar ASIO",BS_PUSHBUTTON|WS_TABSTOP,AudioStart);
  control(L"BUTTON",L"Desconectar",BS_PUSHBUTTON|WS_TABSTOP,AudioStop);
  control(L"BUTTON",L"ASIO Control",BS_PUSHBUTTON|WS_TABSTOP,AudioPanel);
  control(L"BUTTON",L"MIDI Learn knob (Ctrl+L)",BS_PUSHBUTTON|WS_TABSTOP,MidiLearnKnob);
  control(L"BUTTON",L"Performance (P)",BS_PUSHBUTTON|WS_TABSTOP,PerformanceButton);
  performanceMode=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,PerformanceModeControl);
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Camadas: quatro timbres em todo teclado"));
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dividir: duas partes (2 + 2)"));
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dividir: duas partes (3 + 1)"));
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dividir: três partes (2 + 1 + 1)"));
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dividir: quatro partes (1 + 1 + 1 + 1)"));
  SendMessageW(performanceMode,CB_SETCURSEL,0,0);
  for(int i=0;i<4;++i)performancePartControls[i]=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,PerformancePartBase+i);
  for(int i=0;i<4;++i)performanceClearButtons[i]=control(L"BUTTON",L"X",BS_PUSHBUTTON|WS_TABSTOP,PerformanceClearBase+i);
  performancePlay=control(L"BUTTON",L"Ativar Performance",BS_PUSHBUTTON|WS_TABSTOP,PerformancePlay);
  performanceName=control(L"EDIT",L"Minha Performance",ES_AUTOHSCROLL|WS_TABSTOP,PerformanceName);
  performanceEditPart=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,PerformanceEditPart);
  for(int i=0;i<4;++i){auto label=L"Editar timbre "+std::to_wstring(i+1);SendMessageW(performanceEditPart,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
  SendMessageW(performanceEditPart,CB_SETCURSEL,0,0);
  performanceSave=control(L"BUTTON",L"Salvar Performance",BS_PUSHBUTTON|WS_TABSTOP,PerformanceSave);
  control(L"BUTTON",L"Pânico (F12)",BS_PUSHBUTTON|WS_TABSTOP,Panic);
  SendMessageW(audioDriver,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Selecione driver ASIO"));
  SendMessageW(audioDriver,CB_SETCURSEL,0,0);
  for(auto size:buffers){auto text=size?L"Buffer "+std::to_wstring(size):L"Buffer do driver";SendMessageW(audioBuffer,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));}
  SendMessageW(audioBuffer,CB_SETCURSEL,0,0);
  for(int i=0;i<14;++i){labels[i]=control(L"STATIC",L"",SS_LEFT|SS_NOTIFY,Knob+100+i);sliders[i]=control(TRACKBAR_CLASSW,L"",TBS_HORZ|WS_TABSTOP,Knob+i);SendMessageW(sliders[i],TBM_SETRANGE,TRUE,MAKELPARAM(0,1000));
   if(!SetWindowSubclass(sliders[i],parameterProc,i,0)||!SetWindowSubclass(labels[i],parameterProc,i,0))throw std::runtime_error("Não foi possível habilitar o clique direito dos parâmetros");}
  auto samples=base/L"samples";
  if(!std::filesystem::is_directory(samples))samples=base/L"recovery/extracted/live/samples";
  if(!std::filesystem::is_directory(samples))samples=std::filesystem::current_path()/L"recovery/extracted/live/samples";
  if(std::filesystem::exists(base/L"soundbank/catalog.txt"))instruments=lp::loadInstrumentBank(base/L"soundbank");
  else for(const auto& path:lp::catalog(samples)){lp::Instrument p;p.sfz=path;p.name=path.stem().u8string();p.category=1;instruments.push_back(p);}
  if(!instruments.empty())SendMessageW(category,CB_SETCURSEL,instruments.front().category,0);
  refreshCatalog();refreshFavorites();
  for(int i=0;i<4;++i){SendMessageW(performancePartControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Desativado"));SendMessageW(performancePartControls[i],CB_SETCURSEL,0,0);}
  try{auto dll=base/L"sfizz.dll";if(!std::filesystem::exists(dll))dll=base/L"recovery/extracted/live/sfizz.dll";engine=std::make_unique<lp::AudioHost>(dll);
   // Discover SF2 evaluation presets without changing the approved SFZ bank.
   // Keep SF2 entries in memory only; the original curated catalog remains intact.
   // Scan standard banks and a user-owned "Red Pianos" folder placed beside the EXE.
   // Never package or redistribute purchased banks with the public executable.
   const std::array<std::filesystem::path,3> sf2Folders={
    base/L"soundbank/sf2",base/L"Red Pianos",base/L"Red Pianos SF2 Punch Pack - Vol 1"};
   for(const auto& sf2Folder:sf2Folders){
    if(!std::filesystem::is_directory(sf2Folder))continue;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(
      sf2Folder,std::filesystem::directory_options::skip_permission_denied)){
     if(entry.depth()>5){entry.disable_recursion_pending();continue;}
     if(!entry.is_regular_file()||entry.path().extension()!=L".sf2")continue;
     try{
      auto presets=engine->sf2Presets(entry.path());
      auto file=entry.path().stem().u8string();
      // Licensed Red Punch Pack: category by original filename prefix, never by guessed preset.
      unsigned category=10;
      if(file.rfind("Red-PN-",0)==0)category=0;
      else if(file.rfind("Red-EP-",0)==0)category=1;
      else if(file.rfind("Red-OR-",0)==0)category=2;
      else if(file.rfind("Red-ST-",0)==0)category=3;
      else if(file.rfind("Red-BR-",0)==0)category=4;
      else if(file.rfind("Red-LD-",0)==0||file.rfind("Red-SY-",0)==0)category=5;
      else if(file.rfind("Red-PD-",0)==0||file.rfind("Red-VO-",0)==0)category=6;
      else if(file.rfind("Red-BL-",0)==0)category=7;
      else if(file.rfind("Red-RD-",0)==0)category=9;
      else if(file.rfind("Red-GT-",0)==0)category=12;
      if(file.find("Fantasia")!=std::string::npos||file.find("Staccato Heaven")!=std::string::npos)category=6;
      if(file.find("Galileo Bell")!=std::string::npos||file.find("Stack Bells")!=std::string::npos)category=7;
      for(const auto& preset:presets){
       if(preset.name=="EOP")continue; // SF2 end-of-presets marker, not a playable timbre.
       lp::Instrument p;
       p.sfz=std::filesystem::u8path(lp::makeSf2Reference(entry.path(),preset.bank,preset.program));
       p.name=file+" / "+preset.name;
       p.category=category;
       instruments.push_back(std::move(p));
      }
     }catch(const std::exception& e){
      // A missing FluidSynth runtime or damaged bank must not hide approved SFZ.
      OutputDebugStringA(e.what());OutputDebugStringA("\\n");
     }
    }
   }
   refreshCatalog();
   audioDevices=engine->devices();for(const auto& device:audioDevices){auto text=std::filesystem::u8path(device.name).wstring();SendMessageW(audioDriver,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));}
   if(audioDevices.empty())message(L"Nenhum driver ASIO estéreo encontrado. Instale ou habilite seu driver ASIO. Não há fallback Windows.");
   PWSTR roaming=nullptr;auto folderResult=SHGetKnownFolderPath(FOLDERID_RoamingAppData,KF_FLAG_CREATE,nullptr,&roaming);
   if(FAILED(folderResult)||!roaming){if(roaming)CoTaskMemFree(roaming);throw std::runtime_error("Pasta permanente de configurações de áudio indisponível");}
   auto folder=std::filesystem::path(roaming)/L"Live Performance by Hugo Sodre";CoTaskMemFree(roaming);
   std::filesystem::create_directories(folder);settingsPath=folder/L"audio.settings";
   try{loadMySounds();refreshCatalog();}catch(const std::exception& e){error(e);}
   if(std::filesystem::exists(settingsPath)){
    auto saved=lp::AudioSettings::load(settingsPath);int device=lp::resolveAsioDriver(audioDevices,saved.driver);
    for(std::size_t i=0;i<audioDevices.size();++i)if(audioDevices[i].id==device)SendMessageW(audioDriver,CB_SETCURSEL,i+1,0);
    auto buffer=std::find(buffers.begin(),buffers.end(),saved.buffer);if(buffer==buffers.end())throw std::runtime_error("Buffer ASIO salvo inválido");
    SendMessageW(audioBuffer,CB_SETCURSEL,buffer-buffers.begin(),0);
    if(!saved.midi.empty()){for(UINT i=0;i<midiInGetNumDevs();++i){MIDIINCAPSW caps{};if(midiInGetDevCapsW(i,&caps,sizeof(caps))==MMSYSERR_NOERROR&&std::filesystem::path(caps.szPname).u8string()==saved.midi){SendMessageW(midi,CB_SETCURSEL,i+1,0);openMidiSelection();break;}}}

    pendingRestore=true;SetTimer(window,2,750,nullptr);message(L"Reconectando automaticamente o último driver ASIO...");
   }
  }catch(const std::exception& e){error(e);message(L"Áudio não inicializado ou configurações não restauradas. Confira o driver ASIO e tente conectar.");}
  layout();SetFocus(catalog);SetTimer(window,1,500,nullptr);
 }
 void layout(){RECT r{};GetClientRect(window,&r);int width=r.right,height=r.bottom,half=(width-48)/2;
  MoveWindow(GetDlgItem(window,1),16,12,width-32,40,TRUE);
  MoveWindow(GetDlgItem(window,Open),16,60,190,44,TRUE);MoveWindow(GetDlgItem(window,Save),214,60,190,44,TRUE);
  MoveWindow(GetDlgItem(window,Import),412,60,175,44,TRUE);MoveWindow(GetDlgItem(window,ImportSf2),595,60,175,44,TRUE);MoveWindow(effectsButton,778,60,190,44,TRUE);
  MoveWindow(midi,16,116,width-32,320,TRUE);
  MoveWindow(audioDriver,16,170,half,320,TRUE);MoveWindow(audioBuffer,32+half,170,half,320,TRUE);
  MoveWindow(GetDlgItem(window,AudioStart),16,224,210,44,TRUE);MoveWindow(GetDlgItem(window,AudioStop),234,224,190,44,TRUE);
  MoveWindow(GetDlgItem(window,AudioPanel),432,224,200,44,TRUE);MoveWindow(GetDlgItem(window,PerformanceButton),1150,224,240,44,TRUE);MoveWindow(GetDlgItem(window,MidiLearnKnob),858,224,270,44,TRUE);MoveWindow(GetDlgItem(window,Panic),640,224,210,44,TRUE);
  MoveWindow(category,16,284,half,350,TRUE);
  MoveWindow(catalog,16,338,half,height-428,TRUE);MoveWindow(favorites,32+half,284,half,height-374,TRUE);
  MoveWindow(performanceMode,16,284,width-32,250,TRUE);
  for(int i=0;i<4;++i){MoveWindow(performancePartControls[i],16,342+i*64,width-108,250,TRUE);MoveWindow(performanceClearButtons[i],width-76,342+i*64,60,44,TRUE);}
  MoveWindow(performanceName,16,604,420,44,TRUE);
  MoveWindow(performanceEditPart,448,604,310,200,TRUE);
  MoveWindow(performancePlay,16,662,320,48,TRUE);
  MoveWindow(performanceSave,352,662,340,48,TRUE);
  ShowWindow(performanceMode,performanceVisible?SW_SHOW:SW_HIDE);
  for(auto control:performancePartControls)ShowWindow(control,performanceVisible?SW_SHOW:SW_HIDE);
  for(auto control:performanceClearButtons)ShowWindow(control,performanceVisible?SW_SHOW:SW_HIDE);
  ShowWindow(performancePlay,performanceVisible?SW_SHOW:SW_HIDE);
  ShowWindow(performanceName,performanceVisible?SW_SHOW:SW_HIDE);
  ShowWindow(performanceEditPart,performanceVisible?SW_SHOW:SW_HIDE);
  ShowWindow(performanceSave,performanceVisible?SW_SHOW:SW_HIDE);
  ShowWindow(category,(effects||performanceVisible)?SW_HIDE:SW_SHOW);
  ShowWindow(catalog,(effects||performanceVisible)?SW_HIDE:SW_SHOW);ShowWindow(favorites,(effects||performanceVisible)?SW_HIDE:SW_SHOW);
  for(int i=0;i<14;++i){int col=i/7,row=i%7;int x=16+col*(half+16),y=284+row*((height-384)/7);
   MoveWindow(labels[i],x,y,half,30,TRUE);MoveWindow(sliders[i],x,y+30,half,32,TRUE);
   ShowWindow(labels[i],(effects&&!performanceVisible)?SW_SHOW:SW_HIDE);ShowWindow(sliders[i],(effects&&!performanceVisible)?SW_SHOW:SW_HIDE);
  }
  MoveWindow(status,16,height-78,width-32,70,TRUE);InvalidateRect(window,nullptr,TRUE);
 }
 void cancelAudioRestore(){pendingRestore=false;KillTimer(window,2);}
 void restoreAudio(){
  if(!pendingRestore||!engine)return;
  try{lp::restoreRememberedAudio(*engine,settingsPath);cancelAudioRestore();audioStatus();}
  catch(const std::exception& e){if(++restoreAttempts>=3){cancelAudioRestore();error(e);message(L"Não foi possível reconectar o ASIO salvo. Confira seu driver e clique em Conectar ASIO.");}}
 }
 void audioStatus(){if(engine&&engine->running())message(L"ASIO conectado — 48 kHz. Latência de saída informada pelo driver: "+std::to_wstring(static_cast<int>(engine->latency()*10000)/10.0)+L" ms. "+(engine->loaded()?L"Timbre carregado.":L"Selecione um timbre e pressione Enter."));else message(L"ASIO desconectado. Selecione o driver e clique em Conectar ASIO.");}
 int driverIndex(){auto selected=SendMessageW(audioDriver,CB_GETCURSEL,0,0);if(selected<1||static_cast<std::size_t>(selected)>audioDevices.size())throw std::runtime_error("Select an ASIO driver");return static_cast<int>(selected-1);}
 void audition(){try{if(SendMessageW(category,CB_GETCURSEL,0,0)==13){openCatalogPerformance();return;}auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=visibleInstruments.size())return;if(!engine)throw std::runtime_error("Audio engine unavailable");auto path=instruments[visibleInstruments[selected]].sfz;previewPatch=lp::Patch{};previewPatch.sfz=lp::instrumentReference(instruments[visibleInstruments[selected]]);previewActive=true;editingPerformanceValid=false;auto sf2=lp::parseSf2Reference(path.u8string());if(sf2)engine->loadSf2(sf2->path,sf2->bank,sf2->program,lp::Patch{}.parameters);else engine->load(path,lp::Patch{}.parameters);router.cancelLearn();show.current.reset();refreshParameters();audioStatus();}catch(const std::exception& e){error(e);}}
 std::wstring parameterName(int index){return std::wstring(lp::parameterNames[index],lp::parameterNames[index]+std::char_traits<char>::length(lp::parameterNames[index]));}
 void beginParameterLearn(std::size_t index){
  if(index>=sliders.size())return;
  if(editingPerformanceValid&&engine&&engine->performanceActive()){
   if(!midiHandle){message(L"Selecione a entrada MIDI antes de aprender o controle.");return;}
   if(!editingPerformance.parts[editingPart].enabled){message(L"Selecione uma parte ativa.");return;}
   router.cancelLearn();learningPerformanceParameter=static_cast<int>(index);
   message(L"MIDI Learn da parte "+std::to_wstring(editingPart+1)+L": mova o controle fisico. ESC cancela.");return;
  }
  if((!show.current||!show.favorites[*show.current])&&!previewActive){message(L"Selecione um timbre antes de mapear os parâmetros MIDI.");return;}
  if(!midiHandle){message(L"Selecione a entrada MIDI do seu teclado antes de iniciar o MIDI Learn.");return;}
  try{if(previewActive&&!show.current){learningPerformanceParameter=static_cast<int>(index);}else router.learn(index);refreshParameters();SetFocus(sliders[index]);message(L"MIDI Learn — "+parameterName(static_cast<int>(index))+L": mova o fader ou knob físico do teclado que envia CC. ESC cancela.");}catch(const std::exception& e){error(e);}
 }
 void cancelMidiLearn(){bool wasLearning=router.learning()||learningPerformanceParameter>=0;learningPerformanceParameter=-1;router.cancelLearn();refreshParameters();if(wasLearning)message(L"MIDI Learn cancelado.");}
 void refreshCatalog(){
  SendMessageW(catalog,LB_RESETCONTENT,0,0);visibleInstruments.clear();visiblePerformances.clear();
  auto selected=SendMessageW(category,CB_GETCURSEL,0,0);
  if(selected==13){
   for(std::size_t i=0;i<show.performanceLibrary.size();++i){
    visiblePerformances.push_back(i);
    auto name=std::filesystem::u8path(show.performanceLibrary[i].name).wstring();
    SendMessageW(catalog,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
   }
  }else for(std::size_t i=0;i<instruments.size();++i){
   const auto& p=instruments[i];if(selected<0||p.category!=static_cast<unsigned>(selected))continue;
   visibleInstruments.push_back(i);auto name=std::filesystem::u8path(p.name).wstring();
   SendMessageW(catalog,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
  }
  if(SendMessageW(catalog,LB_GETCOUNT,0,0)>0)SendMessageW(catalog,LB_SETCURSEL,0,0);
 }
 void openCatalogPerformance(){
  auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);
  if(selected<0||static_cast<std::size_t>(selected)>=visiblePerformances.size())return;
  if(!engine)throw std::runtime_error("Audio engine unavailable");
  auto index=visiblePerformances[static_cast<std::size_t>(selected)];
  auto loaded=lp::restorePerformance(show.performanceLibrary[index]);
  engine->loadPerformance(loaded);editingPerformance=std::move(loaded);
  editingPerformanceValid=true;activePerformanceIndex=static_cast<int>(index);
  editingPart=0;show.current.reset();router.cancelLearn();
  SetWindowTextW(performanceName,std::filesystem::u8path(editingPerformance.name).wstring().c_str());
  refreshParameters();message(L"Performance carregada do perfil de show.");
 }
 void refreshFavorites(){SendMessageW(favorites,LB_RESETCONTENT,0,0);for(int i=0;i<20;++i){std::wstring name=std::to_wstring(i+1)+L". ";name+=show.performances[i]?std::filesystem::u8path(show.performances[i]->name).wstring():(show.favorites[i]?std::filesystem::u8path(lp::instrumentName(show.favorites[i]->sfz,instruments)).wstring():L"Vazio");SendMessageW(favorites,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}refreshParameters();}
 void refreshParameters(){for(int i=0;i<14;++i){float value=editingPerformanceValid&&engine&&engine->performanceActive()?editingPerformance.parts[editingPart].patch.parameters[i]:(show.current&&show.favorites[*show.current]?show.favorites[*show.current]->parameters[i]:(previewActive?previewPatch.parameters[i]:0));auto text=std::wstring(lp::parameterNames[i],lp::parameterNames[i]+std::char_traits<char>::length(lp::parameterNames[i]))+L": "+std::to_wstring(static_cast<int>(value*100))+L"%";if(i>=1&&i<=4&&show.current&&show.favorites[*show.current]&&!show.favorites[*show.current]->adsrEnabled)text+=L" (SFZ original)";if(show.current&&show.favorites[*show.current]){int cc=show.favorites[*show.current]->cc[i];if(cc>=0)text+=L" — CC "+std::to_wstring(cc);}
  if(router.pendingParameter()==static_cast<std::size_t>(i))text+=L" — aguardando CC";
  SetWindowTextW(labels[i],text.c_str());SendMessageW(sliders[i],TBM_SETPOS,TRUE,static_cast<int>(value*1000));EnableWindow(sliders[i],(show.current.has_value()||editingPerformanceValid||previewActive));}}
 void applyParameters(){
  if(!engine)return;
  if(editingPerformanceValid&&engine->performanceActive()&&performanceVisible){
   auto& part=editingPerformance.parts[editingPart];
   if(part.enabled)engine->setPerformancePartParameters(editingPart,part.patch.parameters,part.patch.adsrEnabled);
  }else if(show.current&&show.favorites[*show.current]&&!show.performances[*show.current])
   engine->parameters(show.favorites[*show.current]->parameters,show.favorites[*show.current]->adsrEnabled);
  else if(previewActive)engine->parameters(previewPatch.parameters,previewPatch.adsrEnabled);
 }
 void activate(unsigned slot){if(slot>=20||!show.favorites[slot])return;try{activePerformanceIndex=-1;if(!engine)throw std::runtime_error("Audio engine unavailable");if(show.performances[slot]){
    editingPerformance=lp::restorePerformance(*show.performances[slot]);editingPerformanceValid=true;editingPart=0;
    engine->loadPerformance(editingPerformance);router.cancelLearn();show.programChange(slot);SetWindowTextW(performanceName,std::filesystem::u8path(editingPerformance.name).wstring().c_str());
    SendMessageW(favorites,LB_SETCURSEL,slot,0);refreshParameters();message(L"Performance carregada dos Favoritos.");return;
   }
   editingPerformanceValid=false;previewActive=false;
   auto reference=show.favorites[slot]->sfz;auto sf2=lp::parseSf2Reference(reference);if(sf2)engine->loadSf2(sf2->path,sf2->bank,sf2->program,show.favorites[slot]->parameters,show.favorites[slot]->adsrEnabled);else engine->load(lp::resolveInstrument(reference,instruments),show.favorites[slot]->parameters,show.favorites[slot]->adsrEnabled);router.cancelLearn();show.programChange(slot);applyParameters();SendMessageW(favorites,LB_SETCURSEL,slot,0);refreshParameters();audioStatus();}catch(const std::exception& e){error(e);}}
 void favorite(){if(SendMessageW(category,CB_GETCURSEL,0,0)==13){openCatalogPerformance();return;}auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=visibleInstruments.size())return;auto path=lp::instrumentReference(instruments[visibleInstruments[selected]]);for(unsigned i=0;i<20;++i)if(show.favorites[i]&&show.favorites[i]->sfz==path){activate(i);return;}for(unsigned i=0;i<20;++i)if(!show.favorites[i]){show.favorites[i]=(previewActive&&previewPatch.sfz==path)?previewPatch:lp::Patch{};show.favorites[i]->sfz=path;previewActive=false;show.performances[i].reset();refreshFavorites();activate(i);return;}message(L"Os 20 favoritos estão ocupados.");}
 bool fileDialog(bool saving,bool sfz,std::filesystem::path& path){wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=sfz?L"Instrumento SFZ\0*.sfz\0\0":L"Perfil Live Performance\0*.lphs\0\0";dialog.lpstrDefExt=sfz?L"sfz":L"lphs";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(saving?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);bool ok=saving?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog);if(ok)path=file;else if(CommDlgExtendedError())throw std::runtime_error("File dialog failed");return ok;}
 bool sf2Dialog(std::filesystem::path& path){wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"SoundFont SF2\0*.sf2\0\0";dialog.lpstrDefExt=L"sf2";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;bool ok=GetOpenFileNameW(&dialog);if(ok)path=file;else if(CommDlgExtendedError())throw std::runtime_error("SF2 file dialog failed");return ok;}
 void clearPerformancePart(int index){
  if(index<0||index>=4)return;
  SendMessageW(performancePartControls[index],CB_SETCURSEL,0,0);
  if(editingPerformanceValid){
   auto& part=editingPerformance.parts[index];part.enabled=false;part.instrument.clear();
   if(index==editingPart)learningPerformanceParameter=-1;
   // Re-activate explicitly to rebuild the audio engine with the new layer layout.
   message(L"Timbre removido da parte "+std::to_wstring(index+1)+L". Clique Ativar Performance para aplicar ao audio.");
  }
  SetFocus(performancePartControls[index]);
 }
 void command(int id,int event){try{
  if(id>=PerformanceClearBase&&id<PerformanceClearBase+4){clearPerformancePart(id-PerformanceClearBase);return;}
  if((id==AudioDriver||id==AudioBuffer)&&event==CBN_SELCHANGE&&SendMessageW(audioDriver,CB_GETCURSEL,0,0)<1)return;
  if(id==AudioStart||((id==AudioDriver||id==AudioBuffer)&&event==CBN_SELCHANGE)){cancelAudioRestore();if(!engine)throw std::runtime_error("Audio engine unavailable; check sfizz.dll and ASIO");auto index=driverIndex();auto selected=SendMessageW(audioBuffer,CB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=buffers.size())throw std::runtime_error("Select ASIO buffer");lp::connectAndRemember(*engine,audioDevices[index].id,buffers[selected],settingsPath);persistAudioAndMidi();audioStatus();}
  if(id==AudioStop&&engine){cancelAudioRestore();engine->stop();audioStatus();}
  if(id==AudioPanel){if(!engine)throw std::runtime_error("Audio engine unavailable");engine->controlPanel(audioDevices[driverIndex()].id,window);audioStatus();}
  if(id==MidiLearnKnob){int selected=-1;auto focus=GetFocus();for(int i=0;i<14;++i)if(focus==sliders[i]||focus==labels[i]){selected=i;break;}if(selected<0){message(L"Abra Parâmetros (E), selecione o knob desejado com TAB e clique MIDI Learn ou pressione Ctrl+L.");}else beginParameterLearn(static_cast<std::size_t>(selected));}
  if(id==Panic&&engine){engine->panic();message(L"Pânico: notas e caudas interrompidas.");}
  if(id==PerformanceButton){performanceVisible=!performanceVisible;effects=false;
   for(int i=0;i<4;++i){SendMessageW(performancePartControls[i],CB_RESETCONTENT,0,0);SendMessageW(performancePartControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Desativado"));for(const auto& instrument:instruments){auto name=std::filesystem::u8path(instrument.name).wstring();SendMessageW(performancePartControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}SendMessageW(performancePartControls[i],CB_SETCURSEL,0,0);}
   if(editingPerformanceValid){SetWindowTextW(performanceName,std::filesystem::u8path(editingPerformance.name).wstring().c_str());SendMessageW(performanceEditPart,CB_SETCURSEL,editingPart,0);}
   layout();SetFocus(performanceVisible?performanceMode:catalog);}
  if(id==PerformanceEditPart&&event==CBN_SELCHANGE&&editingPerformanceValid){learningPerformanceParameter=-1;
   editingPart=static_cast<int>(SendMessageW(performanceEditPart,CB_GETCURSEL,0,0));
   if(editingPart>=0&&editingPart<4){
    const auto& patch=editingPerformance.parts[editingPart].patch;
    for(int i=0;i<14;++i)SendMessageW(sliders[i],TBM_SETPOS,TRUE,static_cast<int>(patch.parameters[i]*1000));refreshParameters();
   }
  }
  if(id==PerformanceSave){
   if(!editingPerformanceValid)throw std::runtime_error("Ative a Performance antes de salvar");
   wchar_t name[256]{};GetWindowTextW(performanceName,name,256);
   editingPerformance.name=std::filesystem::path(name).u8string();
   if(!editingPerformance.valid())throw std::runtime_error("Nome ou partes invalidos");
   auto saved=lp::savePerformance(editingPerformance);
   if(activePerformanceIndex>=0&&static_cast<std::size_t>(activePerformanceIndex)<show.performanceLibrary.size())
    show.performanceLibrary[static_cast<std::size_t>(activePerformanceIndex)]=std::move(saved);
   else {
    if(show.performanceLibrary.size()>=256)throw std::runtime_error("Limite de 256 Performances por perfil");
    activePerformanceIndex=static_cast<int>(show.performanceLibrary.size());
    show.performanceLibrary.push_back(std::move(saved));
   }
   SendMessageW(category,CB_SETCURSEL,13,0);refreshCatalog();
   SendMessageW(catalog,LB_SETCURSEL,activePerformanceIndex,0);
   message(L"Performance salva na categoria Performance deste perfil. Use Salvar perfil para gravar em disco.");
  }
  if(id==PerformancePlay){activePerformanceIndex=-1;learningPerformanceParameter=-1;if(!engine)throw std::runtime_error("Audio engine unavailable");lp::Performance selected;selected.name="Performance";int mode=static_cast<int>(SendMessageW(performanceMode,CB_GETCURSEL,0,0));selected.mode=mode==0?lp::PerformanceMode::Layer:lp::PerformanceMode::Split;
   constexpr int boundaries[5][5]={{0,0,0,0,128},{0,0,64,64,128},{0,0,0,0,128},{0,0,0,64,128},{0,32,64,96,128}};
   for(int i=0;i<4;++i){int item=static_cast<int>(SendMessageW(performancePartControls[i],CB_GETCURSEL,0,0));if(item<=0)continue;auto& part=selected.parts[i];part.enabled=true;part.instrument=lp::instrumentReference(instruments[static_cast<std::size_t>(item-1)]);if(mode==1){part.low=i<2?0:64;part.high=i<2?63:127;}else if(mode==2){part.low=i<3?0:64;part.high=i<3?63:127;}else if(mode==3){part.low=i<2?0:(i==2?43:86);part.high=i<2?42:(i==2?85:127);}else if(mode==4){part.low=i*32;part.high=i==3?127:i*32+31;}}
   wchar_t name[256]{};GetWindowTextW(performanceName,name,256);selected.name=std::filesystem::path(name).u8string();
   engine->loadPerformance(selected);editingPerformance=selected;editingPerformanceValid=true;editingPart=0;show.current.reset();refreshParameters();
   message(L"Performance ativa: quatro partes editaveis. Selecione timbre e abra Parametros (E).");}
  if(id==Effects){effects=!effects;if(effects)performanceVisible=false;if(!effects)cancelMidiLearn();layout();refreshParameters();SetFocus(effects?sliders[0]:catalog);}
  if(id==Save||id==Open){std::filesystem::path path;if(fileDialog(id==Save,false,path)){if(id==Save){
    if(activePerformanceIndex>=0&&editingPerformanceValid&&static_cast<std::size_t>(activePerformanceIndex)<show.performanceLibrary.size())
     show.performanceLibrary[static_cast<std::size_t>(activePerformanceIndex)]=lp::savePerformance(editingPerformance);
    show.save(path);
   }else{show.load(path);router.cancelLearn();editingPerformanceValid=false;activePerformanceIndex=-1;previewActive=false;show.current.reset();if(engine)engine->unload();refreshFavorites();refreshCatalog();}message(L"Perfil processado. Formato novo .lphs; INI antigo ainda não é compatível.");}}
  if(id==Import){std::filesystem::path path;if(fileDialog(false,true,path)){lp::Instrument p;p.sfz=path;p.name=path.stem().u8string();p.category=14;if(addMySound(std::move(p)))saveMySounds();SendMessageW(category,CB_SETCURSEL,14,0);refreshCatalog();SendMessageW(catalog,LB_SETCURSEL,visibleInstruments.size()-1,0);}}
  if(id==ImportSf2){std::filesystem::path path;if(sf2Dialog(path)){if(!engine)throw std::runtime_error("Audio engine unavailable");auto presets=engine->sf2Presets(path);if(presets.empty())throw std::runtime_error("SF2 has no presets");for(const auto& preset:presets){lp::Instrument p;p.sfz=std::filesystem::u8path(lp::makeSf2Reference(path,preset.bank,preset.program));p.name=path.stem().u8string()+" / "+preset.name+" ("+std::to_string(preset.bank)+":"+std::to_string(preset.program)+")";p.category=14;addMySound(std::move(p));}saveMySounds();SendMessageW(category,CB_SETCURSEL,14,0);refreshCatalog();message(L"SF2 importado: escolha um preset na lista Meus Sons.");}}
  if(id==Category&&event==CBN_SELCHANGE)refreshCatalog();
  if(id==Catalog&&event==LBN_DBLCLK)audition();
  if(id==Favorites&&event==LBN_SELCHANGE){auto slot=SendMessageW(favorites,LB_GETCURSEL,0,0);if(slot>=0)activate(static_cast<unsigned>(slot));}
  if(id==Midi&&event==CBN_SELCHANGE){cancelMidiLearn();openMidiSelection();persistAudioAndMidi();}
 }catch(const std::exception& e){error(e);}}
 void midiEvent(DWORD packed){try{auto midiMessage=lp::MidiMessage::packed(packed);
  if(editingPerformanceValid&&engine&&engine->performanceActive()&&lp::knobBinding(midiMessage)>=0){
   bool consumed=false;
   if(learningPerformanceParameter>=0){
    auto& part=editingPerformance.parts[editingPart];int index=learningPerformanceParameter;learningPerformanceParameter=-1;
    for(auto& cc:part.patch.cc)if(cc==lp::knobBinding(midiMessage))cc=-1;
    part.patch.cc[index]=lp::knobBinding(midiMessage);
    part.patch.parameters[index]=lp::knobValue(midiMessage);
    if(index>=1&&index<=4)part.patch.adsrEnabled=true;
    engine->setPerformancePartParameters(editingPart,part.patch.parameters,part.patch.adsrEnabled);
    message(L"CC "+std::to_wstring(midiMessage.data1)+L" atribuido a parte "+std::to_wstring(editingPart+1)+L".");
    consumed=true;
   }else{
    for(int p=0;p<4;++p)if(editingPerformance.parts[p].enabled){
     auto& part=editingPerformance.parts[p];
     for(int index=0;index<14;++index)if(part.patch.cc[index]==lp::knobBinding(midiMessage)){
      part.patch.parameters[index]=lp::knobValue(midiMessage);
      if(index>=1&&index<=4)part.patch.adsrEnabled=true;
      engine->setPerformancePartParameters(p,part.patch.parameters,part.patch.adsrEnabled);
      consumed=true;
     }
    }
   }
   if(consumed){refreshParameters();return;}
  }
  if(previewActive&&!show.current&&lp::knobBinding(midiMessage)>=0){
   auto binding=lp::knobBinding(midiMessage);
   if(learningPerformanceParameter>=0){
    for(auto& cc:previewPatch.cc)if(cc==binding)cc=-1;
    previewPatch.cc[learningPerformanceParameter]=binding;learningPerformanceParameter=-1;
   }
   for(int i=0;i<14;++i)if(previewPatch.cc[i]==binding){
    previewPatch.parameters[i]=lp::knobValue(midiMessage);
    if(i>=1&&i<=4)previewPatch.adsrEnabled=true;
    applyParameters();refreshParameters();return;
   }
  }
  auto result=router.route(midiMessage);if(result.action==lp::MidiAction::WrongProgram){message(L"Aguardando PC "+std::to_wstring(result.index+1)+L". Recebido PC "+std::to_wstring(result.value+1)+L". Configure o botão do controlador para o número do favorito.");}
 else if(result.action==lp::MidiAction::FavoriteLearned){message(L"Program Change "+std::to_wstring(result.index+1)+L" aprendido. Use Salvar perfil para guardar o vínculo.");}
 else if(result.action==lp::MidiAction::Favorite)activate(result.index);else if(result.action==lp::MidiAction::Parameter||result.action==lp::MidiAction::Learned){applyParameters();refreshParameters();if(result.action==lp::MidiAction::Learned)message(L"CC "+std::to_wstring(result.value)+L" aprendido para "+parameterName(result.index)+L". O controle já está ativo. Use Salvar perfil para guardar o mapeamento.");}
 if(engine&&lp::forwardsToEngine(result,midiMessage))engine->send(midiMessage);}catch(const std::exception& e){error(e);}}
};
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){auto app=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(msg==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}if(!app)return DefWindowProcW(w,msg,wp,lp);
 switch(msg){case WM_CREATE:try{app->initialize(w);}catch(const std::exception& e){app->error(e);return -1;}return 0;
 case WM_TIMER:if(wp==2){app->restoreAudio();return 0;}if(app->engine&&app->engine->takeFault()){try{app->engine->stop();}catch(const std::exception& e){app->error(e);}app->message(L"Falha no motor/driver. Áudio interrompido; reconecte ASIO.");}return 0;
 case WM_SIZE:if(app->catalog)app->layout();return 0;
 case WM_COMMAND:app->command(LOWORD(wp),HIWORD(wp));return 0;
 case WM_KEYDOWN:if(wp=='P'&&!(GetKeyState(VK_CONTROL)&0x8000)){app->command(PerformanceButton,BN_CLICKED);return 0;}if(wp=='L'&&(GetKeyState(VK_CONTROL)&0x8000)){auto focus=GetFocus();for(int i=0;i<14;++i)if(focus==app->sliders[i]||focus==app->labels[i]){app->beginParameterLearn(i);return 0;}}break;
 case MM_MIM_DATA:if(reinterpret_cast<HMIDIIN>(wp)==app->midiHandle)app->midiEvent(static_cast<DWORD>(lp));return 0;
 case BeginParameterLearn:app->beginParameterLearn(static_cast<std::size_t>(wp));return 0;
 case WM_HSCROLL:for(int i=0;i<14;++i)if(reinterpret_cast<HWND>(lp)==app->sliders[i]){
  float value=static_cast<float>(SendMessageW(app->sliders[i],TBM_GETPOS,0,0))/1000;
  if(app->editingPerformanceValid&&app->engine&&app->engine->performanceActive()){
   auto& part=app->editingPerformance.parts[app->editingPart];if(part.enabled){part.patch.parameters[i]=value;if(i>=1&&i<=4)part.patch.adsrEnabled=true;app->engine->setPerformancePartParameters(app->editingPart,part.patch.parameters,part.patch.adsrEnabled);}
  }else if(app->show.current){app->show.parameter(i,value);app->applyParameters();app->refreshParameters();}else if(app->previewActive){app->previewPatch.parameters[i]=value;if(i>=1&&i<=4)app->previewPatch.adsrEnabled=true;app->applyParameters();app->refreshParameters();}
 }return 0;
 case WM_CONTEXTMENU:if(reinterpret_cast<HWND>(wp)==app->favorites){
  auto slot=SendMessageW(app->favorites,LB_GETCURSEL,0,0);
  if(lp!=-1){POINT pt{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};ScreenToClient(app->favorites,&pt);auto hit=SendMessageW(app->favorites,LB_ITEMFROMPOINT,0,MAKELPARAM(pt.x,pt.y));slot=HIWORD(hit)?-1:LOWORD(hit);}
  if(slot>=0){try{app->router.learnFavorite(static_cast<std::size_t>(slot));app->message(L"MIDI Learn: pressione o botão Program Change correspondente ao favorito (1 a 20). ESC cancela.");}catch(const std::exception& e){app->error(e);}}return 0;
 }for(int i=0;i<14;++i){bool match=reinterpret_cast<HWND>(wp)==app->sliders[i]||reinterpret_cast<HWND>(wp)==app->labels[i];
  // Disabled trackbars route mouse requests to their parent: explain the prerequisite.
  if(!match&&lp!=-1&&app->effects){POINT pt{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};RECT slider{},label{};GetWindowRect(app->sliders[i],&slider);GetWindowRect(app->labels[i],&label);match=PtInRect(&slider,pt)||PtInRect(&label,pt);}
  if(match){app->beginParameterLearn(i);return 0;}
 }break;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:SetTextColor(reinterpret_cast<HDC>(wp),RGB(255,255,255));SetBkColor(reinterpret_cast<HDC>(wp),RGB(12,16,24));return reinterpret_cast<LRESULT>(app->background);
 case WM_ERASEBKGND:{RECT r{};GetClientRect(w,&r);FillRect(reinterpret_cast<HDC>(wp),&r,app->background);return 1;}
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={1000,900};return 0;
 case WM_CLOSE:try{if(app->engine)app->persistAudioAndMidi();}catch(const std::exception& e){app->error(e);}break;
 case WM_DESTROY:app->closeMidi();PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};InitCommonControlsEx(&common);App app;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LPbyHSReconstruction";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);auto w=CreateWindowW(cls.lpszClassName,L"Live Performance by Hugo Sodré",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1200,940,nullptr,nullptr,instance,&app);if(!w)return 1;ShowWindow(w,show);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_KEYDOWN){if(msg.wParam==VK_ESCAPE){app.cancelMidiLearn();app.effects=false;app.performanceVisible=false;app.layout();SetFocus(app.catalog);continue;}if(msg.wParam==VK_F12){app.command(Panic,0);continue;}if(msg.wParam=='P'&&!(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)){app.command(PerformanceButton,BN_CLICKED);continue;}if(msg.wParam==VK_DELETE&&app.performanceVisible){bool cleared=false;for(int i=0;i<4;++i)if(GetFocus()==app.performancePartControls[i]){app.clearPerformancePart(i);cleared=true;break;}if(cleared)continue;}if(msg.wParam==VK_RETURN&&GetFocus()==app.catalog){app.audition();continue;}if(msg.wParam=='E'){app.command(Effects,0);continue;}if(msg.wParam==VK_SPACE&&GetFocus()==app.catalog){app.favorite();continue;}}if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}return static_cast<int>(msg.wParam);}
