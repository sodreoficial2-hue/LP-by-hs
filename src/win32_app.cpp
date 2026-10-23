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
#include "ui_state.hpp"
#include "ui_art.hpp"

namespace {
constexpr int Catalog=100,Favorites=101,Save=102,Open=103,Midi=104,Effects=105,Import=106,ImportSf2=114,AudioDriver=107,AudioBuffer=108,AudioStart=109,AudioStop=110,AudioPanel=111,Panic=112,Category=113,Knob=200,MidiLearnKnob=115,PerformanceButton=116,PerformanceModeControl=117,PerformancePartBase=118,PerformancePlay=122,PerformanceSave=123,PerformanceName=124,PerformanceEditPart=125,PerformanceClearBase=126,AudioMidiButton=140,SettingsButton=141,AboutButton=142,CategoryTile=143,PreviousCategory=144,NextCategory=145,InstrumentArt=146,AboutText=147;
constexpr UINT BeginParameterLearn=WM_APP+40,LoadLibraryMessage=WM_APP+41;
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
 std::array<HWND,16> sliders{},labels{};
 HFONT font{}; HBRUSH background=CreateSolidBrush(lpui::ink);
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
 ~App(){closeMidi();engine.reset();for(auto f:{font,titleFont,signatureFont,largeFont,smallFont})if(f)DeleteObject(f);DeleteObject(background);}
 void closeMidi(){router.cancelLearn();if(engine)engine->panic();if(midiHandle){midiInStop(midiHandle);midiInReset(midiHandle);midiInClose(midiHandle);midiHandle=nullptr;}}
 void message(const std::wstring& text){SetWindowTextW(status,text.c_str());}
 void error(const std::exception& e){auto s=std::string(e.what());std::wstring text(s.begin(),s.end());MessageBoxW(window,text.c_str(),L"Live Performance",MB_OK|MB_ICONERROR);}
 HWND control(const wchar_t* kind,const wchar_t* text,DWORD style,int id){
  if(_wcsicmp(kind,L"BUTTON")==0)style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;
  auto h=CreateWindowExW(_wcsicmp(kind,L"LISTBOX")==0?WS_EX_CLIENTEDGE:0,kind,text,WS_CHILD|style,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
  SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;
 }
 void initialize(HWND w){
  window=w;wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);base=std::filesystem::path(exe).parent_path();
  font=CreateFontW(-24,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  loadingStart=GetTickCount64();
  titleFont=CreateFontW(-50,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  signatureFont=CreateFontW(-38,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe Script");
  largeFont=CreateFontW(-34,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  smallFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  photo=lpui::portrait(GetModuleHandleW(nullptr));
  control(L"BUTTON",L"Áudio / MIDI\nA",BS_PUSHBUTTON|WS_TABSTOP,AudioMidiButton);
  control(L"BUTTON",L"Configurações\nC",BS_PUSHBUTTON|WS_TABSTOP,SettingsButton);
  control(L"BUTTON",L"Sobre",BS_PUSHBUTTON,AboutButton);
  control(L"BUTTON",L"Escolher categoria",BS_PUSHBUTTON|WS_TABSTOP,CategoryTile);
  control(L"BUTTON",L"❮",BS_PUSHBUTTON|WS_TABSTOP,PreviousCategory);
  control(L"BUTTON",L"❯",BS_PUSHBUTTON|WS_TABSTOP,NextCategory);
  control(L"BUTTON",L"Categoria selecionada",BS_PUSHBUTTON,InstrumentArt);
  control(L"EDIT",L"Hugo Sodré é cantor, compositor, produtor e multi-instrumentista de Guarulhos, São Paulo. Iniciou seus estudos de piano aos seis anos e concluiu a formação em piano popular na Escola Proarte de Música aos treze. Mais tarde, estudou Composição e Regência na FIAM-FAAM.\r\n\r\nAo longo de sua trajetória, atuou com Os Originais do Samba, Eliana de Lima, Sandra de Sá, Jair Rodrigues, Tony Tornado e Netinho de Paula, entre outros artistas, além de desenvolver seu trabalho autoral.\r\n\r\nAos treze anos, uma degeneração da mácula comprometeu sua visão. A experiência pessoal com a baixa visão e as exigências do palco motivaram a concepção do Live Performance: um instrumento de trabalho com alto contraste, controles grandes e acesso pelo teclado e por MIDI.\r\n\r\nO projeto busca ampliar a autonomia de músicos com baixa visão, unindo acessibilidade, expressão musical e praticidade nas apresentações.\r\n\r\nAquilo que meus olhos não veem, minha música sente.",ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_TABSTOP,AboutText);
  control(L"BUTTON",L"Abrir perfil",BS_PUSHBUTTON|WS_TABSTOP,Open);
  control(L"BUTTON",L"Salvar perfil",BS_PUSHBUTTON|WS_TABSTOP,Save);
  control(L"BUTTON",L"Importar SFZ",BS_PUSHBUTTON|WS_TABSTOP,Import);
  control(L"BUTTON",L"Importar SF2",BS_PUSHBUTTON|WS_TABSTOP,ImportSf2);
  effectsButton=control(L"BUTTON",L"Parâmetros (E)",BS_PUSHBUTTON|WS_TABSTOP,Effects);
  category=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,Category);
  for(const auto* name:lp::instrumentCategories){auto text=std::filesystem::u8path(name).wstring();SendMessageW(category,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));}
  SendMessageW(category,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Favoritos"));
  SendMessageW(category,CB_SETCURSEL,0,0);
  catalog=control(L"LISTBOX",L"",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,Catalog);
  favorites=control(L"LISTBOX",L"",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,Favorites);
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
  control(L"BUTTON",L"Performance\nEditor (P)",BS_PUSHBUTTON|WS_TABSTOP,PerformanceButton);
  performanceMode=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,PerformanceModeControl);
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Camadas: quatro timbres em todo teclado"));
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dividir: duas partes (2 + 2)"));
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dividir: duas partes (3 + 1)"));
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dividir: três partes (2 + 1 + 1)"));
  SendMessageW(performanceMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dividir: quatro partes (1 + 1 + 1 + 1)"));
  SendMessageW(performanceMode,CB_SETCURSEL,0,0);
  for(int i=0;i<4;++i)performancePartControls[i]=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,PerformancePartBase+i);
  for(int i=0;i<4;++i)performanceClearButtons[i]=control(L"BUTTON",L"Limpar",BS_PUSHBUTTON|WS_TABSTOP,PerformanceClearBase+i);
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
  for(int i=0;i<16;++i){labels[i]=control(L"STATIC",L"",SS_LEFT|SS_NOTIFY,Knob+100+i);sliders[i]=control(TRACKBAR_CLASSW,L"",TBS_HORZ|WS_TABSTOP,Knob+i);SendMessageW(sliders[i],TBM_SETRANGE,TRUE,MAKELPARAM(0,1000));
   if(!SetWindowSubclass(sliders[i],parameterProc,i,0)||!SetWindowSubclass(labels[i],parameterProc,i,0))throw std::runtime_error("Não foi possível habilitar o clique direito dos parâmetros");}
  for(int i=0;i<4;++i){SendMessageW(performancePartControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Desativado"));SendMessageW(performancePartControls[i],CB_SETCURSEL,0,0);}
  layout();PostMessageW(window,LoadLibraryMessage,0,0);
 }
 void loadLibrary(){
  updateSplash(2,L"Localizando os timbres SFZ...");
  auto samples=base/L"samples";
  if(!std::filesystem::is_directory(samples))samples=base/L"recovery/extracted/live/samples";
  if(!std::filesystem::is_directory(samples))samples=std::filesystem::current_path()/L"recovery/extracted/live/samples";
  if(std::filesystem::exists(base/L"soundbank/catalog.txt"))instruments=lp::loadInstrumentBank(base/L"soundbank");
  else for(const auto& path:lp::catalog(samples)){lp::Instrument p;p.sfz=path;p.name=path.stem().u8string();p.category=1;instruments.push_back(p);}
  if(!instruments.empty()){categoryIndex=static_cast<int>(instruments.front().category);SendMessageW(category,CB_SETCURSEL,categoryIndex,0);}
  updateSplash(12,L"Catálogo SFZ carregado. Inicializando o motor...");
  refreshCatalog();refreshFavorites();
  try{auto dll=base/L"sfizz.dll";if(!std::filesystem::exists(dll))dll=base/L"recovery/extracted/live/sfizz.dll";engine=std::make_unique<lp::AudioHost>(dll);
   // Discover SF2 evaluation presets without changing the approved SFZ bank.
   // Keep SF2 entries in memory only; the original curated catalog remains intact.
   // Scan standard banks and a user-owned "Red Pianos" folder placed beside the EXE.
   // Never package or redistribute purchased banks with the public executable.
   const std::array<std::filesystem::path,2> sf2Folders={
    base/L"soundbank/sf2",base/L"Red Pianos SF2 Punch Pack - Vol 1"};
   std::vector<std::filesystem::path> bankFiles;
   for(const auto& folder:sf2Folders)if(std::filesystem::is_directory(folder)){
    for(auto it=std::filesystem::recursive_directory_iterator(folder,std::filesystem::directory_options::skip_permission_denied);it!=std::filesystem::recursive_directory_iterator();++it){
     if(it.depth()>5){it.disable_recursion_pending();continue;}
     auto extension=it->path().extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
     if(it->is_regular_file()&&extension==L".sf2")bankFiles.push_back(it->path());
    }
   }
   startupBanks=std::move(bankFiles);startupCompleted=0;
   PostMessageW(window,LoadLibraryMessage,1,0);
  }catch(const std::exception& e){error(e);finishLibrary();}
 }
 std::vector<std::filesystem::path> startupBanks;std::size_t startupCompleted=0;
 void loadNextBank(){
  if(startupCompleted>=startupBanks.size()){finishLibrary();return;}
  const auto& bankFile=startupBanks[startupCompleted];std::filesystem::directory_entry entry(bankFile);
  updateSplash(15+static_cast<int>(75*startupCompleted/startupBanks.size()),L"Carregando "+std::to_wstring(startupCompleted+1)+L" de "+std::to_wstring(startupBanks.size())+L": "+bankFile.filename().wstring());
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
  ++startupCompleted;PostMessageW(window,LoadLibraryMessage,1,0);
 }
 void finishLibrary(){
  try{if(!engine)throw std::runtime_error("Motor de áudio indisponível");
   updateSplash(92,L"Restaurando Áudio / MIDI...");
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
  updateSplash(100,L"Biblioteca pronta.");loading=false;layout();SetFocus(GetDlgItem(window,CategoryTile));SetTimer(window,1,250,nullptr);
 }
 #include "ui_app.inc"
 void cancelAudioRestore(){pendingRestore=false;KillTimer(window,2);}
 void restoreAudio(){
  if(!pendingRestore||!engine)return;
  try{lp::restoreRememberedAudio(*engine,settingsPath);cancelAudioRestore();audioStatus();}
  catch(const std::exception& e){if(++restoreAttempts>=3){cancelAudioRestore();error(e);message(L"Não foi possível reconectar o ASIO salvo. Confira seu driver e clique em Conectar ASIO.");}}
 }
 void audioStatus(){if(engine&&engine->running())message(L"ASIO conectado — "+std::to_wstring(static_cast<int>(engine->sampleRate()))+L" Hz. Latência de saída informada pelo driver: "+std::to_wstring(static_cast<int>(engine->latency()*10000)/10.0)+L" ms. "+(engine->loaded()?L"Timbre carregado.":L"Selecione um timbre e pressione Enter."));else message(L"ASIO desconectado. Selecione o driver e clique em Conectar ASIO.");}
 int driverIndex(){auto selected=SendMessageW(audioDriver,CB_GETCURSEL,0,0);if(selected<1||static_cast<std::size_t>(selected)>audioDevices.size())throw std::runtime_error("Select an ASIO driver");return static_cast<int>(selected-1);}
 void audition(){try{
  syncPerformance();
  if(categoryIndex==13){openCatalogPerformance();return;}
  auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=visibleInstruments.size())return;
  const auto& instrument=instruments[visibleInstruments[selected]];auto reference=lp::instrumentReference(instrument);
  for(unsigned i=0;i<20;++i)if(show.favorites[i]&&show.favorites[i]->sfz==reference){activate(i);return;}
  if(!engine)throw std::runtime_error("Motor de áudio indisponível");
  lp::Patch patch{};patch.sfz=reference;auto sf2=lp::parseSf2Reference(instrument.sfz.u8string());
  if(sf2)engine->loadSf2(sf2->path,sf2->bank,sf2->program,patch.parameters);else engine->load(instrument.sfz,patch.parameters);
  previewPatch=patch;previewActive=true;editingPerformanceValid=false;activePerformanceIndex=-1;router.cancelLearn();show.current.reset();cancelMidiLearn();refreshParameters();if(effects)layout();audioStatus();
 }catch(const std::exception& e){error(e);}}
 std::wstring parameterName(int index){return std::wstring(lp::parameterNames[index],lp::parameterNames[index]+std::char_traits<char>::length(lp::parameterNames[index]));}
 void beginParameterLearn(std::size_t index){
  touchParameters();if(!effects||index>=sliders.size())return;
  if(!midiHandle){message(L"Selecione a entrada MIDI em Áudio / MIDI antes de mapear.");return;}
  cancelMidiLearn();
  if(performanceControls()){if(index>=4||!editingPerformance.parts[index].enabled)return;learningVolume=static_cast<int>(index);}
  else if(index==15)learningMaster=true;
  else if(show.current)router.learn(index);
  else if(previewActive)learningPerformanceParameter=static_cast<int>(index);
  else{message(L"Selecione um timbre antes de mapear.");return;}
  refreshParameters();message(L"MIDI Learn: mova o fader ou knob físico. ESC cancela o aprendizado.");
 }
 void cancelMidiLearn(){bool wasLearning=router.learning()||learningPerformanceParameter>=0||learningVolume>=0||learningMaster;learningVolume=-1;learningMaster=false;learningPerformanceParameter=-1;router.cancelLearn();refreshParameters();if(wasLearning)message(L"MIDI Learn cancelado.");}
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
  auto index=visiblePerformances[static_cast<std::size_t>(selected)];loadPerformanceIndex(index);
 }
 void loadPerformanceIndex(std::size_t index){
  if(!engine)throw std::runtime_error("Motor de áudio indisponível");
  syncPerformance();if(index>=show.performanceLibrary.size())throw std::runtime_error("Performance ausente");
  auto loaded=lp::restorePerformance(show.performanceLibrary[index]);
  auto resolved=loaded;for(auto& part:resolved.parts)if(part.enabled)part.instrument=lp::resolveInstrument(part.instrument,instruments).u8string();
  engine->loadPerformance(resolved);previewActive=false;editingPerformance=std::move(loaded);
  editingPerformanceValid=true;activePerformanceIndex=static_cast<int>(index);
  editingPart=0;show.current.reset();router.cancelLearn();
  SetWindowTextW(performanceName,std::filesystem::u8path(editingPerformance.name).wstring().c_str());
  refreshParameters();if(effects)layout();message(L"Performance carregada do perfil de show.");
 }
 std::wstring favoriteName(const lp::Patch& patch){if(patch.sfz.rfind("performance://",0)==0){auto index=static_cast<std::size_t>(std::stoul(patch.sfz.substr(14)));return index<show.performanceLibrary.size()?std::filesystem::u8path(show.performanceLibrary[index].name).wstring():L"Performance ausente";}return std::filesystem::u8path(lp::instrumentName(patch.sfz,instruments)).wstring();}
 void refreshFavorites(){SendMessageW(favorites,LB_RESETCONTENT,0,0);for(int i=0;i<20;++i){std::wstring name=std::to_wstring(i+1)+L". ";name+=show.performances[i]?std::filesystem::u8path(show.performances[i]->name).wstring():(show.favorites[i]?favoriteName(*show.favorites[i]):L"Vazio");SendMessageW(favorites,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}refreshParameters();}
 void refreshParameters(){
  bool perf=performanceControls();
  for(int i=0;i<16;++i){float value=0;int cc=-1;bool enabled=false;std::wstring name;
   if(perf){if(i<4){const auto& part=editingPerformance.parts[i];name=L"Camada "+std::to_wstring(i+1)+L" · Volume";value=part.patch.parameters[0];cc=part.patch.cc[0];enabled=part.enabled;}}
   else if(i==15){name=L"Master";value=show.master;cc=show.masterCC;enabled=true;}
   else {name=parameterName(i);const lp::Patch* patch=show.current&&show.favorites[*show.current]?&*show.favorites[*show.current]:(previewActive?&previewPatch:nullptr);if(patch){value=patch->parameters[i];cc=patch->cc[i];enabled=true;}}
   auto label=name+L": "+std::to_wstring(static_cast<int>(value*100))+L"%";
   if(cc>=0)label+=L" · CC "+std::to_wstring(cc);
   if((perf&&learningVolume==i)||(!perf&&((i==15&&learningMaster)||learningPerformanceParameter==i||router.pendingParameter()==static_cast<std::size_t>(i))))label+=L" · Mova o controle";
   SetWindowTextW(labels[i],label.c_str());SetWindowTextW(sliders[i],name.c_str());SendMessageW(sliders[i],TBM_SETPOS,TRUE,static_cast<int>(value*1000));EnableWindow(sliders[i],enabled);
  }
 }
 void applyParameters(){
  if(!engine)return;
  if(editingPerformanceValid&&engine->performanceActive()){
   auto& part=editingPerformance.parts[editingPart];
   if(part.enabled)engine->setPerformancePartParameters(editingPart,part.patch.parameters,part.patch.adsrEnabled);
  }else if(show.current&&show.favorites[*show.current]&&!show.performances[*show.current])
   engine->parameters(show.favorites[*show.current]->parameters,show.favorites[*show.current]->adsrEnabled);
  else if(previewActive)engine->parameters(previewPatch.parameters,previewPatch.adsrEnabled);
 }
 void activate(unsigned slot){if(slot>=20||!show.favorites[slot])return;try{syncPerformance();
   if(show.favorites[slot]->sfz.rfind("performance://",0)==0){auto index=static_cast<std::size_t>(std::stoul(show.favorites[slot]->sfz.substr(14)));loadPerformanceIndex(index);show.programChange(slot);SendMessageW(favorites,LB_SETCURSEL,slot,0);return;}
   activePerformanceIndex=-1;if(!engine)throw std::runtime_error("Audio engine unavailable");if(show.performances[slot]){
    editingPerformance=lp::restorePerformance(*show.performances[slot]);editingPerformanceValid=true;editingPart=0;
    engine->loadPerformance(editingPerformance);router.cancelLearn();show.programChange(slot);SetWindowTextW(performanceName,std::filesystem::u8path(editingPerformance.name).wstring().c_str());
    SendMessageW(favorites,LB_SETCURSEL,slot,0);refreshParameters();message(L"Performance carregada dos Favoritos.");return;
   }
   editingPerformanceValid=false;previewActive=false;
   auto reference=show.favorites[slot]->sfz;auto sf2=lp::parseSf2Reference(reference);if(sf2)engine->loadSf2(sf2->path,sf2->bank,sf2->program,show.favorites[slot]->parameters,show.favorites[slot]->adsrEnabled);else engine->load(lp::resolveInstrument(reference,instruments),show.favorites[slot]->parameters,show.favorites[slot]->adsrEnabled);router.cancelLearn();show.programChange(slot);applyParameters();SendMessageW(favorites,LB_SETCURSEL,slot,0);refreshParameters();if(effects)layout();audioStatus();}catch(const std::exception& e){error(e);}}
 void favorite(){
  try{
   if(categoryIndex==15){auto slot=SendMessageW(favorites,LB_GETCURSEL,0,0);if(slot<0||!show.favorites[slot])return;
    if(show.current&&*show.current==static_cast<std::size_t>(slot)){previewPatch=*show.favorites[slot];previewActive=!performanceControls();show.current.reset();}
    show.favorites[slot].reset();refreshFavorites();SendMessageW(favorites,LB_SETCURSEL,slot,0);InvalidateRect(favorites,nullptr,TRUE);return;
   }
   auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);if(selected<0)return;
   std::string reference;
   if(categoryIndex==13){if(static_cast<std::size_t>(selected)>=visiblePerformances.size())return;reference="performance://"+std::to_string(visiblePerformances[selected]);}
   else{if(static_cast<std::size_t>(selected)>=visibleInstruments.size())return;reference=lp::instrumentReference(instruments[visibleInstruments[selected]]);}
   for(unsigned i=0;i<20;++i)if(show.favorites[i]&&show.favorites[i]->sfz==reference){
    if(show.current&&*show.current==i){previewPatch=*show.favorites[i];previewActive=!performanceControls();show.current.reset();}show.favorites[i].reset();refreshFavorites();InvalidateRect(catalog,nullptr,TRUE);return;
   }
   for(unsigned i=0;i<20;++i)if(!show.favorites[i]){show.favorites[i]=previewActive&&previewPatch.sfz==reference?previewPatch:lp::Patch{};show.favorites[i]->sfz=reference;show.performances[i].reset();if((previewActive&&previewPatch.sfz==reference)||(performanceControls()&&reference=="performance://"+std::to_string(activePerformanceIndex))){show.programChange(i);previewActive=false;}refreshFavorites();InvalidateRect(catalog,nullptr,TRUE);message(L"Favorito "+std::to_wstring(i+1)+L" adicionado. Use Salvar perfil para guardar.");return;}
   message(L"Os 20 favoritos estão ocupados.");
  }catch(const std::exception& e){error(e);}
 }
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
  if(id==CategoryTile){openCategory();return;}if(id==PreviousCategory||id==NextCategory){chooseCategory(id==PreviousCategory?-1:1);return;}
  if(id==AudioMidiButton){openScreen(lp::Screen::AudioMidi);return;}if(id==SettingsButton){openScreen(lp::Screen::Settings);return;}if(id==AboutButton){openScreen(lp::Screen::About);return;}

  if(id>=PerformanceClearBase&&id<PerformanceClearBase+4){clearPerformancePart(id-PerformanceClearBase);return;}
  if((id==AudioDriver||id==AudioBuffer)&&event==CBN_SELCHANGE&&SendMessageW(audioDriver,CB_GETCURSEL,0,0)<1)return;
  if(id==AudioStart||((id==AudioDriver||id==AudioBuffer)&&event==CBN_SELCHANGE)){cancelAudioRestore();if(!engine)throw std::runtime_error("Audio engine unavailable; check sfizz.dll and ASIO");auto index=driverIndex();auto selected=SendMessageW(audioBuffer,CB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=buffers.size())throw std::runtime_error("Select ASIO buffer");lp::connectAndRemember(*engine,audioDevices[index].id,buffers[selected],settingsPath);persistAudioAndMidi();audioStatus();}
  if(id==AudioStop&&engine){cancelAudioRestore();engine->stop();audioStatus();}
  if(id==AudioPanel){if(!engine)throw std::runtime_error("Audio engine unavailable");engine->controlPanel(audioDevices[driverIndex()].id,window);audioStatus();}
  if(id==MidiLearnKnob){int selected=-1;auto focus=GetFocus();for(int i=0;i<14;++i)if(focus==sliders[i]||focus==labels[i]){selected=i;break;}if(selected<0){message(L"Abra Parâmetros (E), selecione o knob desejado com TAB e clique MIDI Learn ou pressione Ctrl+L.");}else beginParameterLearn(static_cast<std::size_t>(selected));}
  if(id==Panic&&engine){engine->panic();message(L"Pânico: notas e caudas interrompidas.");}
  if(id==PerformanceButton){openScreen(lp::Screen::PerformanceEditor);syncPerformance();
   for(int i=0;i<4;++i){SendMessageW(performancePartControls[i],CB_RESETCONTENT,0,0);SendMessageW(performancePartControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Desativado"));for(const auto& instrument:instruments){auto name=std::filesystem::u8path(instrument.name).wstring();SendMessageW(performancePartControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}int selection=0;if(editingPerformanceValid&&editingPerformance.parts[i].enabled){for(std::size_t n=0;n<instruments.size();++n)if(lp::instrumentReference(instruments[n])==editingPerformance.parts[i].instrument){selection=static_cast<int>(n+1);break;}}SendMessageW(performancePartControls[i],CB_SETCURSEL,selection,0);}
   if(editingPerformanceValid){
    int mode=0;if(editingPerformance.mode==lp::PerformanceMode::Split){mode=4;if(editingPerformance.parts[0].low==editingPerformance.parts[1].low)mode=editingPerformance.parts[2].low==0?2:(editingPerformance.parts[2].high==127?1:3);}
    SendMessageW(performanceMode,CB_SETCURSEL,mode,0);SetWindowTextW(performanceName,std::filesystem::u8path(editingPerformance.name).wstring().c_str());SendMessageW(performanceEditPart,CB_SETCURSEL,editingPart,0);}
   layout();SetFocus(performanceVisible?performanceMode:catalog);}
  if(id==PerformanceEditPart&&event==CBN_SELCHANGE&&editingPerformanceValid){learningPerformanceParameter=-1;
   editingPart=static_cast<int>(SendMessageW(performanceEditPart,CB_GETCURSEL,0,0));
   if(editingPart>=0&&editingPart<4){
    const auto& patch=editingPerformance.parts[editingPart].patch;
    for(int i=0;i<15;++i)SendMessageW(sliders[i],TBM_SETPOS,TRUE,static_cast<int>(patch.parameters[i]*1000));
    refreshParameters();
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
   categoryIndex=13;SendMessageW(category,CB_SETCURSEL,13,0);refreshCatalog();
   SendMessageW(catalog,LB_SETCURSEL,activePerformanceIndex,0);
   refreshFavorites();
   message(L"Performance salva na categoria Performance deste perfil. Use Salvar perfil para gravar em disco.");
  }
  if(id==PerformancePlay){syncPerformance();learningPerformanceParameter=-1;if(!engine)throw std::runtime_error("Audio engine unavailable");lp::Performance selected;selected.name="Performance";int mode=static_cast<int>(SendMessageW(performanceMode,CB_GETCURSEL,0,0));selected.mode=mode==0?lp::PerformanceMode::Layer:lp::PerformanceMode::Split;
   for(int i=0;i<4;++i){int item=static_cast<int>(SendMessageW(performancePartControls[i],CB_GETCURSEL,0,0));if(item<=0)continue;auto& part=selected.parts[i];part.enabled=true;part.instrument=lp::instrumentReference(instruments[static_cast<std::size_t>(item-1)]);auto zone=lp::splitZone(mode,i);part.low=zone[0];part.high=zone[1];if(editingPerformanceValid&&editingPerformance.parts[i].instrument==part.instrument)part.patch=editingPerformance.parts[i].patch;}

   wchar_t name[256]{};GetWindowTextW(performanceName,name,256);selected.name=std::filesystem::path(name).u8string();
   auto resolved=selected;for(auto& part:resolved.parts)if(part.enabled)part.instrument=lp::resolveInstrument(part.instrument,instruments).u8string();
   engine->loadPerformance(resolved);router.cancelLearn();learningVolume=-1;previewActive=false;editingPerformance=selected;editingPerformanceValid=true;editingPart=0;show.current.reset();refreshParameters();
   message(L"Performance ativa: quatro partes editaveis. Selecione timbre e abra Parametros (E).");}
  if(id==Effects){toggleParameters();}
  if(id==Save||id==Open){std::filesystem::path path;if(fileDialog(id==Save,false,path)){if(id==Save){
    if(activePerformanceIndex>=0&&editingPerformanceValid&&static_cast<std::size_t>(activePerformanceIndex)<show.performanceLibrary.size())
     show.performanceLibrary[static_cast<std::size_t>(activePerformanceIndex)]=lp::savePerformance(editingPerformance);
    show.save(path);
   }else{show.load(path);if(engine)engine->masterVolume(show.master);router.cancelLearn();editingPerformanceValid=false;activePerformanceIndex=-1;previewActive=false;show.current.reset();if(engine)engine->unload();refreshFavorites();refreshCatalog();}message(L"Perfil processado. Formato novo .lphs; INI antigo ainda não é compatível.");}}
  if(id==Import){std::filesystem::path path;if(fileDialog(false,true,path)){lp::Instrument p;p.sfz=path;p.name=path.stem().u8string();p.category=14;if(addMySound(std::move(p)))saveMySounds();categoryIndex=14;SendMessageW(category,CB_SETCURSEL,14,0);openCategory();SendMessageW(catalog,LB_SETCURSEL,visibleInstruments.size()-1,0);}}
  if(id==ImportSf2){std::filesystem::path path;if(sf2Dialog(path)){if(!engine)throw std::runtime_error("Audio engine unavailable");auto presets=engine->sf2Presets(path);if(presets.empty())throw std::runtime_error("SF2 has no presets");for(const auto& preset:presets){lp::Instrument p;p.sfz=std::filesystem::u8path(lp::makeSf2Reference(path,preset.bank,preset.program));p.name=path.stem().u8string()+" / "+preset.name+" ("+std::to_string(preset.bank)+":"+std::to_string(preset.program)+")";p.category=14;addMySound(std::move(p));}saveMySounds();categoryIndex=14;SendMessageW(category,CB_SETCURSEL,14,0);openCategory();message(L"SF2 importado: escolha um preset na lista Meus Sons.");}}
  if(id==Category&&event==CBN_SELCHANGE)refreshCatalog();
  if(id==Catalog&&(event==LBN_DBLCLK||event==LBN_SELCHANGE))audition();
  if(id==Favorites&&event==LBN_SELCHANGE){auto slot=SendMessageW(favorites,LB_GETCURSEL,0,0);if(slot>=0)activate(static_cast<unsigned>(slot));}
  if(id==Midi&&event==CBN_SELCHANGE){cancelMidiLearn();openMidiSelection();persistAudioAndMidi();}
 }catch(const std::exception& e){error(e);}}
 void midiEvent(DWORD packed){try{
  auto m=lp::MidiMessage::packed(packed);if(!m.valid())return;
  auto binding=lp::knobBinding(m);bool consumed=false;
  if(binding>=0){
   if(learningMaster){
    for(auto& patch:show.favorites)if(patch)for(auto& cc:patch->cc)if(cc==binding)cc=-1;
    for(auto& cc:previewPatch.cc)if(cc==binding)cc=-1;
    for(auto& performance:show.performanceLibrary)for(auto& part:performance.parts)for(auto& cc:part.patch.cc)if(cc==binding)cc=-1;
    for(auto& part:editingPerformance.parts)for(auto& cc:part.patch.cc)if(cc==binding)cc=-1;
    show.masterCC=binding;learningMaster=false;consumed=true;
   }else if((learningVolume>=0||learningPerformanceParameter>=0||router.pendingParameter())&&show.masterCC==binding)show.masterCC=-1;
   if(show.masterCC==binding){show.master=lp::knobValue(m);if(engine)engine->masterVolume(show.master);consumed=true;}
   if(performanceControls()){
    if(learningVolume>=0){for(auto& part:editingPerformance.parts)if(part.patch.cc[0]==binding)part.patch.cc[0]=-1;editingPerformance.parts[learningVolume].patch.cc[0]=binding;learningVolume=-1;}
    for(int part=0;part<4;++part){auto& p=editingPerformance.parts[part];if(p.enabled&&p.patch.cc[0]==binding){p.patch.parameters[0]=lp::knobValue(m);engine->setPerformancePartParameters(part,p.patch.parameters,p.patch.adsrEnabled);consumed=true;}}
    if(consumed)syncPerformance();
   }else if(previewActive&&!show.current){
    if(learningPerformanceParameter>=0){for(auto& cc:previewPatch.cc)if(cc==binding)cc=-1;previewPatch.cc[learningPerformanceParameter]=binding;learningPerformanceParameter=-1;}
    for(int i=0;i<15;++i)if(previewPatch.cc[i]==binding){previewPatch.parameters[i]=lp::knobValue(m);if(i>=1&&i<=4)previewPatch.adsrEnabled=true;applyParameters();consumed=true;}
   }
  }
  if(consumed){touchParameters();refreshParameters();if(engine&&m.data1==1)engine->send(m);return;}
  auto result=router.route(m);
  if(result.action==lp::MidiAction::WrongProgram)message(L"Aguardando Program Change "+std::to_wstring(result.index+1)+L".");
  else if(result.action==lp::MidiAction::FavoriteLearned)message(L"Program Change aprendido. Use Salvar perfil para guardar.");
  else if(result.action==lp::MidiAction::Favorite)activate(result.index);
  else if(result.action==lp::MidiAction::Parameter||result.action==lp::MidiAction::Learned){touchParameters();applyParameters();refreshParameters();}
  if(engine&&lp::forwardsToEngine(result,m))engine->send(m);
 }catch(const std::exception& e){error(e);}}

};
LRESULT CALLBACK listProc(HWND control,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(control,&r);if(static_cast<short>(LOWORD(lp))>=r.right-60){auto hit=SendMessageW(control,LB_ITEMFROMPOINT,0,lp);if(!HIWORD(hit)){SendMessageW(control,LB_SETCURSEL,LOWORD(hit),0);SendMessageW(GetParent(control),WM_APP+42,id,0);return 0;}}}
 if(msg==WM_NCDESTROY)RemoveWindowSubclass(control,listProc,id);
 return DefSubclassProc(control,msg,wp,lp);
}
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 auto app=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));
 if(msg==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
 if(!app)return DefWindowProcW(w,msg,wp,lp);
 switch(msg){
 case WM_CREATE:try{app->initialize(w);SetWindowSubclass(app->catalog,listProc,Catalog,0);SetWindowSubclass(app->favorites,listProc,Favorites,0);}catch(const std::exception& e){app->error(e);return -1;}return 0;
 case LoadLibraryMessage:try{if(wp==1)app->loadNextBank();else app->loadLibrary();}catch(const std::exception& e){app->loading=false;app->layout();app->error(e);}return 0;
 case WM_TIMER:
  if(wp==2){app->restoreAudio();return 0;}
  if(app->effects&&!(GetKeyState(VK_LBUTTON)&0x8000)&&!app->loading&&app->parameterPanel.expired(GetTickCount64())){app->toggleParameters();}
  if(app->engine&&app->engine->takeFault()){try{app->engine->stop();}catch(const std::exception& e){app->error(e);}app->message(L"Falha no motor/driver. Áudio interrompido; reconecte ASIO.");}return 0;
 case WM_SIZE:if(app->catalog)app->layout();return 0;
 case WM_PAINT:{PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);auto buffer=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,std::max(1L,r.right),std::max(1L,r.bottom));auto previous=SelectObject(buffer,bitmap);app->paint(buffer);BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,previous);DeleteObject(bitmap);DeleteDC(buffer);EndPaint(w,&ps);return 0;}
 case WM_DRAWITEM:{auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item->CtlType==ODT_BUTTON)app->drawButton(item);else if(item->CtlType==ODT_LISTBOX)app->drawList(item);return TRUE;}
 case WM_MEASUREITEM:{auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);if(item->CtlType==ODT_LISTBOX){item->itemHeight=62;return TRUE;}break;}
 case WM_COMMAND:app->touchParameters();app->command(LOWORD(wp),HIWORD(wp));return 0;
 case MM_MIM_DATA:if(reinterpret_cast<HMIDIIN>(wp)==app->midiHandle)app->midiEvent(static_cast<DWORD>(lp));return 0;
 case WM_APP+42:app->favorite();return 0;
 case BeginParameterLearn:app->beginParameterLearn(static_cast<std::size_t>(wp));return 0;
 case WM_HSCROLL:for(int i=0;i<16;++i)if(reinterpret_cast<HWND>(lp)==app->sliders[i]){app->setPanelValue(i,static_cast<float>(SendMessageW(app->sliders[i],TBM_GETPOS,0,0))/1000);return 0;}break;
 case WM_LBUTTONDOWN:if(!app->loading&&!app->effects&&app->screen==lp::Screen::Home){POINT pt{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};if(PtInRect(&app->keyboardRect,pt)){SetCapture(w);app->virtualKey(pt);return 0;}}break;
 case WM_MOUSEMOVE:if(GetCapture()==w){app->virtualKey({static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))});return 0;}break;
 case WM_LBUTTONUP:if(GetCapture()==w){app->releaseVirtual();ReleaseCapture();return 0;}break;
 case WM_CAPTURECHANGED:app->releaseVirtual();return 0;
 case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE)app->releaseVirtual();break;
 case WM_CONTEXTMENU:if(reinterpret_cast<HWND>(wp)==app->favorites){
  auto slot=SendMessageW(app->favorites,LB_GETCURSEL,0,0);
  if(lp!=-1){POINT pt{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};ScreenToClient(app->favorites,&pt);auto hit=SendMessageW(app->favorites,LB_ITEMFROMPOINT,0,MAKELPARAM(pt.x,pt.y));slot=HIWORD(hit)?-1:LOWORD(hit);}
  if(slot>=0){try{app->router.learnFavorite(static_cast<std::size_t>(slot));app->message(L"MIDI Learn: pressione o Program Change correspondente ao favorito (1 a 20). ESC cancela.");}catch(const std::exception& e){app->error(e);}}return 0;
 }break;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,lpui::white);SetBkColor(dc,lpui::ink);return reinterpret_cast<LRESULT>(app->background);}
 case WM_ERASEBKGND:return 1;
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={1280,850};return 0;
 case WM_CLOSE:try{app->releaseVirtual();if(app->engine)app->persistAudioAndMidi();}catch(const std::exception& e){app->error(e);}break;
 case WM_DESTROY:app->closeMidi();PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(w,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&common);
 Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return 1;
 int result=0;{
 App app;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LPbyHSAccessible";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(3));RegisterClassW(&cls);
 auto w=CreateWindowW(cls.lpszClassName,L"Live Performance by Hugo Sodré",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1360,900,nullptr,nullptr,instance,&app);
 if(w){ShowWindow(w,show);UpdateWindow(w);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){
  if(app.effects&&(msg.message==WM_LBUTTONDOWN||msg.message==WM_RBUTTONDOWN||msg.message==WM_MOUSEWHEEL||msg.message==WM_LBUTTONUP))app.touchParameters();
  if(app.keyCommand(msg))continue;
  if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
 }result=static_cast<int>(msg.wParam);}else result=1;
 }
 Gdiplus::GdiplusShutdown(token);return result;
}
