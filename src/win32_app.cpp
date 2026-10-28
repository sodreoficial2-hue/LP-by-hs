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
#include "hidden_sounds.hpp"
#include "profile_paths.hpp"
#include "midi.hpp"
#include "audio_host.hpp"
#include "level_calibration.hpp"
#include "sf2_reference.hpp"
#include "sf2_expansion.hpp"
#include "sf2_headers.hpp"
#include "audio_preferences.hpp"
#include <memory>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "ui_state.hpp"
#include "ui_art.hpp"
#ifdef LP_VISION_UI
#include "vision_protocol.hpp"
#include "vision_webview.hpp"
#endif

namespace {
constexpr int Search=160,RestoreSounds=161,RestoreSelected=162,RestoreAll=163,LocateSounds=164,ProfileState=165,OutputMeter=166,PartSearch=167;
constexpr int Catalog=100,Favorites=101,Save=102,Open=103,Midi=104,Effects=105,Import=106,ImportSf2=114,AudioDriver=107,AudioStart=109,AudioStop=110,AudioPanel=111,Panic=112,Category=113,Knob=200,MidiLearnKnob=115,PerformanceButton=116,PerformanceModeControl=117,PerformancePartBase=118,PerformancePlay=122,PerformanceSave=123,PerformanceName=124,PerformanceEditPart=125,PerformanceClearBase=126,AudioMidiButton=140,SettingsButton=141,AboutButton=142,CategoryTile=143,PreviousCategory=144,NextCategory=145,InstrumentArt=146,AboutText=147,HomeButton=148,KeyboardButton=149,CategoryCardBase=400;
constexpr UINT BeginParameterLearn=WM_APP+40,LoadLibraryMessage=WM_APP+41,ToggleFavoriteMessage=WM_APP+42,SelectSoundMessage=WM_APP+43,RevealFocusMessage=WM_APP+44,VisionCommandMessage=WM_APP+45;
LRESULT CALLBACK pageProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
LRESULT CALLBACK focusProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
LRESULT CALLBACK comboProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
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
 HFONT font{}; HBRUSH background=CreateSolidBrush(lpui::ink),panelBackground=CreateSolidBrush(lpui::panel);
 lp::Session show; lp::MidiRouter router{show};
 std::filesystem::path currentProfile;
 std::string savedProfile=show.serialize();
 bool editorDraft=false,restoringSounds=false,internalEdit=false;
 std::vector<std::string> restoreKeys;
 std::wstring catalogSearch;
 float displayedPeak=0;ULONGLONG clipUntil=0;
 bool profileDirty(){syncPerformance();return editorDraft||show.serialize()!=savedProfile;}
 void updateIndicators(){
  if(loading)return;
  bool dirty=profileDirty();auto label=dirty?L"Alterações não salvas":L"Perfil salvo / sem alterações";
  SetWindowTextW(item(ProfileState),label);
  auto title=std::wstring(L"Live Performance")+(dirty?L" * — Alterações não salvas":L"");SetWindowTextW(window,title.c_str());
  if(engine){engine->collectRetired();displayedPeak=engine->outputPeak();if(engine->takeOutputClip())clipUntil=GetTickCount64()+2000;}
  auto peak=displayedPeak>0?20*std::log10(displayedPeak):-90.f;
  auto meter=GetTickCount64()<clipUntil?std::wstring(L"PICO ALTO — reduza volumes"):L"Saída: "+std::to_wstring(static_cast<int>(peak))+L" dBFS";
  SetWindowTextW(item(OutputMeter),meter.c_str());markVisionDirty();
 }
 bool saveProfile(){
  std::filesystem::path path=currentProfile;if(!fileDialog(true,false,path))return false;
  // An unfinished editor draft must first become a named library entry.
  if(editorDraft){
   activateEditorPerformance();storeEditorPerformance();
  }
  syncPerformance();auto portable=show;
  lp::mapProfilePaths(portable,[&](const std::string& ref){return lp::portableReference(ref,base,path.parent_path());});
  portable.save(path);currentProfile=path;savedProfile=show.serialize();editorDraft=false;
  updateIndicators();message(L"Perfil gravado no computador.");return true;
 }
 bool confirmProfileChange(){
  if(!profileDirty())return true;
  int answer=MessageBoxW(window,L"Existem alterações não salvas. Deseja salvar antes de continuar?\n\nSim: salvar. Não: descartar. Cancelar: voltar ao programa.",L"Salvar alterações do perfil",MB_YESNOCANCEL|MB_ICONQUESTION|MB_DEFBUTTON3);
  if(answer==IDCANCEL)return false;if(answer==IDYES)return saveProfile();return answer==IDNO;
 }

 std::vector<lp::Instrument> instruments;
 std::vector<std::size_t> visibleInstruments;
 std::vector<std::size_t> visiblePerformances;
 int activePerformanceIndex=-1;
 lp::Patch previewPatch{};
 bool previewActive=false;
 std::unique_ptr<lp::AudioHost> engine;
 std::unique_ptr<lp::LevelCalibrationJob> levelCalibration;
 std::filesystem::path engineLibrary;
 std::wstring levelWarning;
 HWND audioDriver{},performanceMode{},performancePlay{},performanceName{},performanceEditPart{},performanceSave{};
 lp::Performance editingPerformance{};
 bool editingPerformanceValid=false;
 int editingPart=0;
 int learningPerformanceParameter=-1;
 std::array<HWND,4> performancePartControls{};
 std::array<HWND,4> performanceClearButtons{};
 bool performanceVisible=false;
 std::vector<lp::AudioDevice> audioDevices;
 std::filesystem::path settingsPath;
 lp::HiddenSounds hiddenSounds;bool deleteDialogActive=false,hiddenCatalogReady=true;
 std::filesystem::path hiddenSoundsPath() const {return settingsPath.parent_path()/L"hidden_sounds.catalog";}
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
 ~App(){levelCalibration.reset();closeVision();closeMidi();engine.reset();for(auto f:{font,titleFont,signatureFont,largeFont,smallFont,statusFont})if(f)DeleteObject(f);DeleteObject(background);DeleteObject(panelBackground);}
 void closeMidi(){router.cancelLearn();if(engine)engine->panic();if(midiHandle){midiInStop(midiHandle);midiInReset(midiHandle);midiInClose(midiHandle);midiHandle=nullptr;}}
 void message(const std::wstring& text){auto label=text;if(!levelWarning.empty())label+=L" · "+levelWarning;SetWindowTextW(status,label.c_str());markVisionDirty();}
 void error(const std::exception& e){recordVisionError(e);auto s=std::string(e.what());std::wstring text(s.begin(),s.end());MessageBoxW(window,text.c_str(),L"Live Performance",MB_OK|MB_ICONERROR);}
 HWND control(const wchar_t* kind,const wchar_t* text,DWORD style,int id){
  if(_wcsicmp(kind,L"BUTTON")==0)style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;
  bool combo=_wcsicmp(kind,L"COMBOBOX")==0;if(combo)style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
  bool header=id==ProfileState||id==OutputMeter||id==2||id==PerformanceButton||id==AudioMidiButton||id==SettingsButton||id==AboutButton||id==HomeButton||id==KeyboardButton;
  auto parent=header?window:page;
  auto h=CreateWindowExW(0,kind,text,WS_CHILD|style,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
  SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
  if(combo){SendMessageW(h,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),40);SendMessageW(h,CB_SETITEMHEIGHT,0,40);SetWindowSubclass(h,comboProc,0,0);}
  if(!header)SetWindowSubclass(h,focusProc,0,reinterpret_cast<DWORD_PTR>(window));
  return h;
 }
 void initialize(HWND w){
  internalEdit=true;window=w;wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);base=std::filesystem::path(exe).parent_path();
  font=CreateFontW(-24,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  page=CreateWindowExW(WS_EX_CONTROLPARENT,L"STATIC",L"Área de conteúdo",WS_CHILD|WS_VSCROLL|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,0,0,window,nullptr,GetModuleHandleW(nullptr),nullptr);
  if(!page||!SetWindowSubclass(page,pageProc,0,reinterpret_cast<DWORD_PTR>(this)))throw std::runtime_error("Não foi possível criar a área de rolagem");
  loadingStart=GetTickCount64();
  titleFont=CreateFontW(-50,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  signatureFont=CreateFontW(-38,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe Script");
  largeFont=CreateFontW(-34,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  smallFont=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  statusFont=CreateFontW(-20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  photo=lpui::portrait(GetModuleHandleW(nullptr));
  categoryPhotos=lpui::resourceImage(GetModuleHandleW(nullptr),4);
  if(!categoryPhotos)throw std::runtime_error("Não foi possível carregar as fotografias das categorias");
  control(L"BUTTON",L"Áudio / MIDI",BS_PUSHBUTTON|WS_TABSTOP,AudioMidiButton);
  control(L"BUTTON",L"Perfis",BS_PUSHBUTTON|WS_TABSTOP,SettingsButton);
  control(L"BUTTON",L"Sobre",BS_PUSHBUTTON|WS_TABSTOP,AboutButton);
  control(L"BUTTON",L"Teclado · K",BS_PUSHBUTTON|WS_TABSTOP,KeyboardButton);
  control(L"BUTTON",L"‹ Biblioteca",BS_PUSHBUTTON|WS_TABSTOP,HomeButton);
  for(int i=0;i<16;++i){auto label=i==15?std::wstring(L"Favoritos"):std::filesystem::u8path(lp::instrumentCategories[i]).wstring();control(L"BUTTON",label.c_str(),BS_PUSHBUTTON|WS_TABSTOP,CategoryCardBase+i);}
  control(L"BUTTON",L"Escolher categoria",BS_PUSHBUTTON|WS_TABSTOP,CategoryTile);
  control(L"BUTTON",L"Categoria selecionada",BS_PUSHBUTTON,InstrumentArt);
  control(L"EDIT",L"Hugo Sodré é cantor, compositor, produtor e multi-instrumentista de Guarulhos, São Paulo. Iniciou seus estudos de piano aos seis anos e concluiu a formação em piano popular na Escola Proarte de Música aos treze. Mais tarde, estudou Composição e Regência na FIAM-FAAM.\r\n\r\nAo longo de sua trajetória, atuou com Os Originais do Samba, Eliana de Lima, Sandra de Sá, Jair Rodrigues, Tony Tornado e Netinho de Paula, entre outros artistas, além de desenvolver seu trabalho autoral.\r\n\r\nAos treze anos, uma degeneração da mácula comprometeu sua visão. A experiência pessoal com a baixa visão e as exigências do palco motivaram a concepção do Live Performance: um instrumento de trabalho com alto contraste, controles grandes e acesso pelo teclado e por MIDI.\r\n\r\nO projeto busca ampliar a autonomia de músicos com baixa visão, unindo acessibilidade, expressão musical e praticidade nas apresentações.\r\n\r\nAquilo que meus olhos não veem, minha música sente.",ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_TABSTOP,AboutText);
  auto about=item(AboutText);int aboutLength=GetWindowTextLengthW(about);std::wstring aboutText(aboutLength+1,L'\0');GetWindowTextW(about,aboutText.data(),aboutLength+1);aboutText.resize(aboutLength);
  aboutText+=L"\r\n\r\nFOTOGRAFIAS DOS INSTRUMENTOS\r\n"+lpui::resourceText(GetModuleHandleW(nullptr),5);SetWindowTextW(about,aboutText.c_str());
  control(L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,PartSearch);
  SendMessageW(item(PartSearch),EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Buscar timbre das partes — Ctrl+F"));
  SendMessageW(item(PartSearch),EM_SETLIMITTEXT,256,0);
  control(L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,Search);
  SendMessageW(item(Search),EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Buscar timbre — Ctrl+F"));
  SendMessageW(item(Search),EM_SETLIMITTEXT,256,0);
  control(L"BUTTON",L"Restaurar timbres removidos",BS_PUSHBUTTON|WS_TABSTOP,RestoreSounds);
  control(L"BUTTON",L"Restaurar selecionado",BS_PUSHBUTTON|WS_TABSTOP,RestoreSelected);
  control(L"BUTTON",L"Restaurar todos",BS_PUSHBUTTON|WS_TABSTOP,RestoreAll);
  control(L"BUTTON",L"Localizar timbres ausentes",BS_PUSHBUTTON|WS_TABSTOP,LocateSounds);
  control(L"STATIC",L"Perfil sem alterações",SS_LEFT|SS_NOPREFIX,ProfileState);
  control(L"STATIC",L"Saída: silêncio",SS_LEFT|SS_NOPREFIX,OutputMeter);
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
  status=control(L"STATIC",L"Selecione seu driver ASIO, clique em Conectar ASIO e escolha um timbre.",SS_LEFT|SS_NOPREFIX,2);
  SendMessageW(status,WM_SETFONT,reinterpret_cast<WPARAM>(statusFont),TRUE);
  audioDriver=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,AudioDriver);
  control(L"BUTTON",L"Conectar ASIO",BS_PUSHBUTTON|WS_TABSTOP,AudioStart);
  control(L"BUTTON",L"Desconectar",BS_PUSHBUTTON|WS_TABSTOP,AudioStop);
  control(L"BUTTON",L"ASIO Control",BS_PUSHBUTTON|WS_TABSTOP,AudioPanel);
  control(L"BUTTON",L"MIDI Learn knob (Ctrl+L)",BS_PUSHBUTTON|WS_TABSTOP,MidiLearnKnob);
  control(L"BUTTON",L"Performance",BS_PUSHBUTTON|WS_TABSTOP,PerformanceButton);
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
  for(int i=0;i<16;++i){labels[i]=control(L"STATIC",L"",SS_LEFT|SS_NOTIFY,Knob+100+i);sliders[i]=control(TRACKBAR_CLASSW,L"",TBS_HORZ|WS_TABSTOP,Knob+i);SendMessageW(sliders[i],TBM_SETRANGE,TRUE,MAKELPARAM(0,1000));
   if(!SetWindowSubclass(sliders[i],parameterProc,i,0)||!SetWindowSubclass(labels[i],parameterProc,i,0))throw std::runtime_error("Não foi possível habilitar o clique direito dos parâmetros");}
  for(int i=0;i<4;++i){SendMessageW(performancePartControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Desativado"));SendMessageW(performancePartControls[i],CB_SETCURSEL,0,0);}
  internalEdit=false;editorDraft=false;layout();PostMessageW(window,LoadLibraryMessage,0,0);
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
  try{auto dll=base/L"sfizz.dll";if(!std::filesystem::exists(dll))dll=base/L"recovery/extracted/live/sfizz.dll";engineLibrary=dll;engine=std::make_unique<lp::AudioHost>(dll);
   // Discover SF2 evaluation presets without changing the approved SFZ bank.
   // Keep SF2 entries in memory only; the original curated catalog remains intact.
   // Scan standard banks and a user-owned "Red Pianos" folder placed beside the EXE.
   // Never package or redistribute purchased banks with the public executable.
   const std::array<std::filesystem::path,3> sf2Folders={
    base/L"soundbank/sf2",base/L"Red Pianos SF2 Punch Pack - Vol 1",base/lp::sf2ExpansionFolder};
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
  updateSplash(15+static_cast<int>(75*startupCompleted/startupBanks.size()),L"Carregando "+std::to_wstring(startupCompleted+1)+L" de "+std::to_wstring(startupBanks.size())+L": "+std::filesystem::u8path(lp::soundDisplayName(bankFile.stem().u8string())).wstring());
     try{
      auto presets=lp::expansionSf2Category(base,entry.path(),0,0)
       ?lp::readSf2PresetHeaders(entry.path()):engine->sf2Presets(entry.path());
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
       p.category=lp::expansionSf2Category(base,entry.path(),preset.bank,preset.program).value_or(category);
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
   try{hiddenSounds.load(hiddenSoundsPath());}catch(const std::exception& e){hiddenCatalogReady=false;error(e);}
   try{loadMySounds();refreshCatalog();}catch(const std::exception& e){error(e);}
   if(std::filesystem::exists(settingsPath)){
    auto saved=lp::AudioSettings::load(settingsPath);int device=lp::resolveAsioDriver(audioDevices,saved.driver);
    for(std::size_t i=0;i<audioDevices.size();++i)if(audioDevices[i].id==device)SendMessageW(audioDriver,CB_SETCURSEL,i+1,0);
    if(!saved.midi.empty()){for(UINT i=0;i<midiInGetNumDevs();++i){MIDIINCAPSW caps{};if(midiInGetDevCapsW(i,&caps,sizeof(caps))==MMSYSERR_NOERROR&&std::filesystem::path(caps.szPname).u8string()==saved.midi){SendMessageW(midi,CB_SETCURSEL,i+1,0);openMidiSelection();break;}}}

    pendingRestore=true; // Reconnect only after offline volume calibration is complete.
   }
  }catch(const std::exception& e){error(e);message(L"Áudio não inicializado ou configurações não restauradas. Confira o driver ASIO e tente conectar.");}
  if(engine&&!settingsPath.empty()){
   try{
    updateSplash(92,L"Verificando os volumes salvos dos timbres...");
    levelCalibration=std::make_unique<lp::LevelCalibrationJob>(engineLibrary,settingsPath.parent_path()/L"volume_calibration.cache",instruments);
    if(!SetTimer(window,4,100,nullptr))throw std::runtime_error("Nao foi possivel acompanhar o equilibrio de volumes");
    return;
   }catch(const std::exception& e){levelCalibration.reset();levelWarning=L"Equilíbrio automático indisponível.";error(e);completeLibrary(L"Equilíbrio automático indisponível. Volumes originais mantidos.");return;}
  }
  completeLibrary(L"Biblioteca aberta sem calibracao: motor ou pasta de configuracoes indisponivel.");
 }
 void pollLevelCalibration(){
  if(!levelCalibration)return;
  auto result=levelCalibration->snapshot();
  auto count=std::to_wstring(result.completed)+L" de "+std::to_wstring(result.total);
  updateSplash(92+(result.total?static_cast<int>(7*result.completed/result.total):7),L"Equilibrando volumes: "+count+L" · "+std::filesystem::u8path(result.name).wstring());
  if(!result.done)return;
  KillTimer(window,4);levelCalibration.reset();
  if(engine)engine->setLevelCompensations(result.gains);
  if(result.failed||result.silent||!result.error.empty()){
   levelWarning=L"Equilíbrio de volumes com avisos: "+std::to_wstring(result.failed)+L" falhas, "+std::to_wstring(result.silent)+L" sem sinal de teste.";
   auto warning=L"Equilibrio de volumes: "+std::to_wstring(result.gains.size())+L" timbres preparados; "+std::to_wstring(result.failed)+L" falhas; "+std::to_wstring(result.silent)+L" sem sinal de teste. Confira os avisos.";
   if(!result.error.empty())warning+=L" "+std::filesystem::u8path(result.error).wstring();
   completeLibrary(warning);
  }else completeLibrary(L"Biblioteca pronta. Volumes equilibrados: "+std::to_wstring(result.analyzed)+L" calibrados, "+std::to_wstring(result.reused)+L" recuperados da memoria.");
 }
 void completeLibrary(const std::wstring& statusText){
  updateSplash(100,statusText);loading=false;layout();SetFocus(item(CategoryCardBase+categoryIndex));SetTimer(window,1,250,nullptr);
  if(pendingRestore)SetTimer(window,2,750,nullptr);
  markVisionDirty();flushVision();
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
  if(restoringSounds)return;if(!preserveEditorDraft())return;
  syncPerformance();
  if(categoryIndex==13){openCatalogPerformance();return;}
  auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=visibleInstruments.size())return;
  const auto& instrument=instruments[visibleInstruments[selected]];auto reference=lp::instrumentReference(instrument);
  if(previewActive&&previewPatch.sfz==reference&&engine&&engine->loaded())return;
  for(unsigned i=0;i<20;++i)if(show.favorites[i]&&show.favorites[i]->sfz==reference){activate(i);return;}
  if(!engine)throw std::runtime_error("Motor de áudio indisponível");
  lp::Patch patch{};patch.sfz=reference;auto sf2=lp::parseSf2Reference(instrument.sfz.u8string());
  if(sf2)engine->loadSf2(sf2->path,sf2->bank,sf2->program,patch.parameters);else engine->load(instrument.sfz,patch.parameters);
  previewPatch=patch;previewActive=true;editingPerformanceValid=false;activePerformanceIndex=-1;router.cancelLearn();show.current.reset();cancelMidiLearn();refreshParameters();if(effects)layout();audioStatus();
 }catch(const std::exception& e){error(e);}}
 std::wstring parameterName(int index){return std::wstring(lp::parameterNames[index],lp::parameterNames[index]+std::char_traits<char>::length(lp::parameterNames[index]));}
 void beginParameterLearn(std::size_t index){
  if(!effects||index>=sliders.size())return;
  if(!midiHandle){message(L"Selecione a entrada MIDI em Áudio / MIDI antes de mapear.");return;}
  cancelMidiLearn();
  if(performanceControls()){if(index>=4||!editingPerformance.parts[index].enabled)return;learningVolume=static_cast<int>(index);}
  else if(index==15)learningMaster=true;
  else if(editingPerformanceControls())learningPerformanceParameter=static_cast<int>(index);
  else if(show.current)router.learn(index);
  else if(previewActive)learningPerformanceParameter=static_cast<int>(index);
  else{message(L"Selecione um timbre antes de mapear.");return;}
  refreshParameters();message(L"MIDI Learn: mova o fader ou knob físico. ESC cancela o aprendizado.");
 }
 void cancelMidiLearn(){bool wasLearning=router.learning()||learningPerformanceParameter>=0||learningVolume>=0||learningMaster;learningVolume=-1;learningMaster=false;learningPerformanceParameter=-1;router.cancelLearn();refreshParameters();if(wasLearning)message(L"MIDI Learn cancelado.");}
 void refreshCatalog(){markVisionDirty();
  SendMessageW(catalog,LB_RESETCONTENT,0,0);visibleInstruments.clear();visiblePerformances.clear();
  restoreKeys.clear();
  if(restoringSounds){
   for(const auto& key:hiddenSounds.entries()){
    std::wstring label=std::filesystem::u8path(key).wstring();
    for(const auto& instrument:instruments)if(lp::soundExclusionKey(lp::instrumentReference(instrument),base)==key)label=std::filesystem::u8path(lp::instrumentDisplayName(instrument)).wstring();
    for(const auto& performance:show.performanceLibrary)if(lp::performanceExclusionKey(performance,base)==key)label=std::filesystem::u8path(performance.name).wstring();
    if(!lp::matchesSearch(label,catalogSearch))continue;
    restoreKeys.push_back(key);SendMessageW(catalog,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
   }
   if(!restoreKeys.empty())SendMessageW(catalog,LB_SETCURSEL,0,0);return;
  }
  auto selected=SendMessageW(category,CB_GETCURSEL,0,0);
  if(selected==13){
   for(std::size_t i=0;i<show.performanceLibrary.size();++i){
    if(hiddenSounds.contains(lp::performanceExclusionKey(show.performanceLibrary[i],base)))continue;
    if(!lp::matchesSearch(std::filesystem::u8path(show.performanceLibrary[i].name).wstring(),catalogSearch))continue;
    visiblePerformances.push_back(i);
    auto name=std::filesystem::u8path(show.performanceLibrary[i].name).wstring();
    SendMessageW(catalog,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
   }
  }else for(std::size_t i=0;i<instruments.size();++i){
   const auto& p=instruments[i];if(selected<0||p.category!=static_cast<unsigned>(selected)||hiddenSounds.contains(lp::soundExclusionKey(lp::instrumentReference(p),base)))continue;
   if(!lp::matchesSearch(std::filesystem::u8path(lp::instrumentDisplayName(p)).wstring(),catalogSearch))continue;
   visibleInstruments.push_back(i);auto name=std::filesystem::u8path(lp::instrumentDisplayName(p)).wstring();
   SendMessageW(catalog,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
  }
  if(SendMessageW(catalog,LB_GETCOUNT,0,0)>0)SendMessageW(catalog,LB_SETCURSEL,0,0);
 }
 void deleteSelectedSound(){try{
  if(loading||effects||restoringSounds||screen!=lp::Screen::Instruments||deleteDialogActive)return;
  if(categoryIndex==15){message(L"Para excluir da categoria, selecione o timbre na categoria original. Espaço retira dos Favoritos.");return;}
  auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);if(selected<0)return;
  std::string key;std::wstring name;
  if(categoryIndex==13){
   if(static_cast<std::size_t>(selected)>=visiblePerformances.size())return;
   const auto& p=show.performanceLibrary[visiblePerformances[selected]];key=lp::performanceExclusionKey(p,base);name=std::filesystem::u8path(p.name).wstring();
  }else{
   if(static_cast<std::size_t>(selected)>=visibleInstruments.size())return;
   const auto& p=instruments[visibleInstruments[selected]];key=lp::soundExclusionKey(lp::instrumentReference(p),base);name=std::filesystem::u8path(lp::instrumentDisplayName(p)).wstring();
  }
  if(settingsPath.empty()||!hiddenCatalogReady)throw std::runtime_error("Lista de exclusoes indisponivel; nenhuma alteracao foi feita");
  // Capture identity before the modal loop: incoming MIDI cannot change the target.
  deleteDialogActive=true;auto prompt=L"Tem certeza de que deseja excluir o timbre \""+name+L"\" do Live Performance?\n\nEle será retirado da categoria. O arquivo original, os Favoritos e as performances salvas serão preservados.";
  int answer=MessageBoxW(window,prompt.c_str(),L"Excluir timbre do Live Performance",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
  deleteDialogActive=false;if(answer!=IDYES)return;
  hiddenSounds.exclude(key,hiddenSoundsPath());
  // Keep the running synth intact and do not auto-load a neighboring sound.
  refreshCatalog();auto count=SendMessageW(catalog,LB_GETCOUNT,0,0);
  if(count>0)SendMessageW(catalog,LB_SETCURSEL,std::min<LRESULT>(selected,count-1),0);
  message(L"Timbre retirado da categoria. Exclusão memorizada automaticamente.");layout();
  if(!visionVisible())SetFocus(catalog);
 }catch(const std::exception& e){deleteDialogActive=false;error(e);}}
 void openCatalogPerformance(){
  auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);
  if(selected<0||static_cast<std::size_t>(selected)>=visiblePerformances.size())return;
  if(!engine)throw std::runtime_error("Audio engine unavailable");
  auto index=visiblePerformances[static_cast<std::size_t>(selected)];
  for(unsigned slot=0;slot<20;++slot)if(show.favorites[slot]&&show.favorites[slot]->sfz=="performance://"+std::to_string(index)){activate(slot);return;}
  loadPerformanceIndex(index);
 }
 void loadPerformanceIndex(std::size_t index,const lp::PerformanceMix* mix=nullptr){
  if(!preserveEditorDraft())return;
  if(!engine)throw std::runtime_error("Motor de áudio indisponível");

  syncPerformance();if(index>=show.performanceLibrary.size())throw std::runtime_error("Performance ausente");
  auto loaded=lp::restorePerformance(show.performanceLibrary[index]);if(mix)lp::applyPerformanceMix(loaded,*mix);
  auto resolved=loaded;for(auto& part:resolved.parts)if(part.enabled)part.instrument=lp::resolveInstrument(part.instrument,instruments).u8string();
  engine->loadPerformance(resolved);previewActive=false;editingPerformance=std::move(loaded);
  editingPerformanceValid=true;activePerformanceIndex=static_cast<int>(index);
  editingPart=0;show.current.reset();router.cancelLearn();
  internalEdit=true;SetWindowTextW(performanceName,std::filesystem::u8path(editingPerformance.name).wstring().c_str());internalEdit=false;
  refreshParameters();if(effects)layout();message(L"Performance carregada do perfil de show.");
 }
 std::wstring favoriteName(const lp::Patch& patch){if(patch.sfz.rfind("performance://",0)==0){auto index=static_cast<std::size_t>(std::stoul(patch.sfz.substr(14)));return index<show.performanceLibrary.size()?std::filesystem::u8path(show.performanceLibrary[index].name).wstring():L"Performance ausente";}return std::filesystem::u8path(lp::instrumentName(patch.sfz,instruments)).wstring();}
 void refreshFavorites(){markVisionDirty();SendMessageW(favorites,LB_RESETCONTENT,0,0);for(int i=0;i<20;++i){std::wstring name=std::to_wstring(i+1)+L". ";name+=show.performances[i]?std::filesystem::u8path(show.performances[i]->name).wstring():(show.favorites[i]?favoriteName(*show.favorites[i]):L"Vazio");SendMessageW(favorites,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}refreshParameters();}
 void refreshParameters(){markVisionDirty();
  bool perf=performanceControls();
  for(int i=0;i<16;++i){float value=0;int cc=-1;bool enabled=false;std::wstring name;
   if(perf){if(i<4){const auto& part=editingPerformance.parts[i];name=L"Camada "+std::to_wstring(i+1)+L" · Volume";value=part.patch.parameters[0];cc=part.patch.cc[0];enabled=part.enabled;}}
   else if(i==15){name=L"Master";value=show.master;cc=show.masterCC;enabled=true;}
   else {name=parameterName(i);const lp::Patch* patch=editingPerformanceControls()?(editingPerformance.parts[editingPart].enabled?&editingPerformance.parts[editingPart].patch:nullptr):(show.current&&show.favorites[*show.current]?&*show.favorites[*show.current]:(previewActive?&previewPatch:nullptr));if(patch){value=patch->parameters[i];cc=patch->cc[i];enabled=true;}}
   auto label=name+L": "+std::to_wstring(static_cast<int>(value*100))+L"%";
   if(cc>=0)label+=L" · CC "+std::to_wstring(cc);
   if((perf&&learningVolume==i)||(!perf&&((i==15&&learningMaster)||learningPerformanceParameter==i||router.pendingParameter()==static_cast<std::size_t>(i))))label+=L" · Mova o controle";
   SetWindowTextW(labels[i],label.c_str());SetWindowTextW(sliders[i],name.c_str());SendMessageW(sliders[i],TBM_SETPOS,TRUE,static_cast<int>(value*1000));EnableWindow(sliders[i],enabled);
  }
 }
 void applyParameters(){
  if(!engine)return;
  if(editingPerformanceControls())editorDraft=true;
  if(editingPerformanceValid&&engine->performanceActive()){
   auto& part=editingPerformance.parts[editingPart];
   if(part.enabled)engine->setPerformancePartParameters(editingPart,part.patch.parameters,part.patch.adsrEnabled);
  }else if(show.current&&show.favorites[*show.current]&&!show.performances[*show.current])
   engine->parameters(show.favorites[*show.current]->parameters,show.favorites[*show.current]->adsrEnabled);
  else if(previewActive)engine->parameters(previewPatch.parameters,previewPatch.adsrEnabled);
 }
 void activate(unsigned slot){if(slot>=20||!show.favorites[slot])return;try{if(show.current&&*show.current==slot&&engine&&engine->loaded())return;if(!preserveEditorDraft())return;syncPerformance();
   if(show.favorites[slot]->sfz.rfind("performance://",0)==0){auto index=static_cast<std::size_t>(std::stoul(show.favorites[slot]->sfz.substr(14)));auto mix=show.favorites[slot]->performanceMix;loadPerformanceIndex(index,mix?&*mix:nullptr);show.programChange(slot);SendMessageW(favorites,LB_SETCURSEL,slot,0);refreshParameters();return;}
   activePerformanceIndex=-1;if(!engine)throw std::runtime_error("Audio engine unavailable");if(show.performances[slot]){
    editingPerformance=lp::restorePerformance(*show.performances[slot]);editingPerformanceValid=true;editingPart=0;
    engine->loadPerformance(editingPerformance);router.cancelLearn();show.programChange(slot);internalEdit=true;SetWindowTextW(performanceName,std::filesystem::u8path(editingPerformance.name).wstring().c_str());internalEdit=false;
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
   for(unsigned i=0;i<20;++i)if(!show.favorites[i]){show.favorites[i]=previewActive&&previewPatch.sfz==reference?previewPatch:lp::Patch{};show.favorites[i]->sfz=reference;show.performances[i].reset();if(performanceActive()&&reference=="performance://"+std::to_string(activePerformanceIndex))show.favorites[i]->performanceMix=lp::performanceMix(editingPerformance);if((previewActive&&previewPatch.sfz==reference)||(performanceControls()&&reference=="performance://"+std::to_string(activePerformanceIndex))){show.programChange(i);previewActive=false;}refreshFavorites();InvalidateRect(catalog,nullptr,TRUE);message(L"Favorito "+std::to_wstring(i+1)+L" adicionado. Use Salvar perfil para guardar.");return;}
   message(L"Os 20 favoritos estão ocupados.");
  }catch(const std::exception& e){error(e);}
 }
 bool fileDialog(bool saving,bool sfz,std::filesystem::path& path){wchar_t file[32768]{};if(saving&&!path.empty())wcsncpy(file,path.c_str(),32767);OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=sfz?L"Instrumento SFZ\0*.sfz\0\0":L"Perfil Live Performance\0*.lphs\0\0";dialog.lpstrDefExt=sfz?L"sfz":L"lphs";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(saving?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);bool ok=saving?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog);if(ok)path=file;else if(CommDlgExtendedError())throw std::runtime_error("File dialog failed");return ok;}
 bool sf2Dialog(std::filesystem::path& path){wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"SoundFont SF2\0*.sf2\0\0";dialog.lpstrDefExt=L"sf2";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;bool ok=GetOpenFileNameW(&dialog);if(ok)path=file;else if(CommDlgExtendedError())throw std::runtime_error("SF2 file dialog failed");return ok;}
 void filterPerformanceChoices(const std::wstring& query){
  std::array<int,4> selected{};
  for(int i=0;i<4;++i){auto row=SendMessageW(performancePartControls[i],CB_GETCURSEL,0,0);selected[i]=row>0?static_cast<int>(SendMessageW(performancePartControls[i],CB_GETITEMDATA,row,0)):0;}
  for(int i=0;i<4;++i){auto control=performancePartControls[i];SendMessageW(control,CB_RESETCONTENT,0,0);SendMessageW(control,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Desativado"));SendMessageW(control,CB_SETITEMDATA,0,0);int chosen=0;
   for(std::size_t n=0;n<instruments.size();++n){auto name=std::filesystem::u8path(lp::instrumentDisplayName(instruments[n])).wstring();bool keep=lp::matchesSearch(name,query);for(auto current:selected)keep|=current==static_cast<int>(n+1);if(!keep)continue;
    auto row=SendMessageW(control,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));SendMessageW(control,CB_SETITEMDATA,row,n+1);if(selected[i]==static_cast<int>(n+1))chosen=static_cast<int>(row);
   }SendMessageW(control,CB_SETCURSEL,chosen,0);
  }markVisionDirty();
 }
 void activateEditorPerformance(){
  learningPerformanceParameter=-1;if(!engine)throw std::runtime_error("Audio engine unavailable");lp::Performance selected;selected.name="Performance";int mode=static_cast<int>(SendMessageW(performanceMode,CB_GETCURSEL,0,0));selected.mode=mode==0?lp::PerformanceMode::Layer:lp::PerformanceMode::Split;
   for(int i=0;i<4;++i){int item=static_cast<int>(SendMessageW(performancePartControls[i],CB_GETCURSEL,0,0));if(item<=0)continue;item=static_cast<int>(SendMessageW(performancePartControls[i],CB_GETITEMDATA,item,0));if(item<=0||static_cast<std::size_t>(item)>instruments.size())continue;auto& part=selected.parts[i];part.enabled=true;part.instrument=lp::instrumentReference(instruments[static_cast<std::size_t>(item-1)]);auto zone=lp::splitZone(mode,i);part.low=zone[0];part.high=zone[1];if(editingPerformanceValid&&editingPerformance.parts[i].instrument==part.instrument)part.patch=editingPerformance.parts[i].patch;}

   wchar_t name[256]{};GetWindowTextW(performanceName,name,256);selected.name=std::filesystem::path(name).u8string();
   auto resolved=selected;for(auto& part:resolved.parts)if(part.enabled)part.instrument=lp::resolveInstrument(part.instrument,instruments).u8string();
   bool same=performanceActive()&&selected.mode==editingPerformance.mode;
   for(int i=0;i<4&&same;++i){const auto& a=selected.parts[i];const auto& b=editingPerformance.parts[i];same=a.enabled==b.enabled&&a.instrument==b.instrument&&a.low==b.low&&a.high==b.high&&a.transpose==b.transpose;}
   if(!selected.valid())throw std::runtime_error("Escolha ao menos uma parte e um nome válido para a Performance");
   if(!same)engine->loadPerformance(resolved);
   router.cancelLearn();learningVolume=-1;previewActive=false;editingPerformance=selected;editingPerformanceValid=true;show.current.reset();refreshParameters();
   message(L"Performance ativa: quatro partes editaveis. Selecione timbre e abra Parametros (E).");
 }
 bool preserveEditorDraft(){
  if(!editorDraft)return true;
  try{activateEditorPerformance();storeEditorPerformance();return true;}
  catch(const std::exception& e){
   auto text=L"Não foi possível guardar o rascunho da Performance.\nDeseja descartar esse rascunho e continuar?\n\nNão mantém o editor para você corrigir.";
   if(MessageBoxW(window,text,L"Performance incompleta",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES){editorDraft=false;return true;}
   error(e);return false;
  }
 }
 void storeEditorPerformance(){
  if(!editingPerformance.valid())throw std::runtime_error("Nome ou partes inválidos");
  auto saved=lp::savePerformance(editingPerformance);
  if(activePerformanceIndex>=0&&static_cast<std::size_t>(activePerformanceIndex)<show.performanceLibrary.size())show.performanceLibrary[activePerformanceIndex]=std::move(saved);
  else{if(show.performanceLibrary.size()>=256)throw std::runtime_error("Limite de 256 Performances por perfil");activePerformanceIndex=static_cast<int>(show.performanceLibrary.size());show.performanceLibrary.push_back(std::move(saved));}
  editorDraft=false;refreshCatalog();refreshFavorites();updateIndicators();message(L"Performance guardada no perfil em memória. Salve o perfil para gravar no computador.");
 }
 void restoreSelectedSound(){
  if(!restoringSounds)return;
  auto row=SendMessageW(catalog,LB_GETCURSEL,0,0);if(row<0||static_cast<std::size_t>(row)>=restoreKeys.size())return;
  if(!hiddenCatalogReady||settingsPath.empty())throw std::runtime_error("Lista de exclusões indisponível");
  auto key=restoreKeys[row];hiddenSounds.restore(key,hiddenSoundsPath());refreshCatalog();layout();message(L"Timbre restaurado na categoria original.");
 }
 bool repairMissing(lp::Session& candidate){
  std::vector<std::pair<std::string,std::string>> replacements;
  bool cancelled=false;
  lp::mapProfilePaths(candidate,[&](const std::string& reference){
   auto path=lp::referencePath(reference);
   if(reference.rfind("bank://",0)==0){try{path=lp::resolveInstrument(reference,instruments);}catch(...){path=std::filesystem::u8path(reference.substr(7)+".sfz");}}
   if(path.empty()||std::filesystem::is_regular_file(path))return reference;
   for(const auto& pair:replacements)if(pair.first==path.u8string())return pair.first==pair.second?reference:lp::withReferencePath(reference,std::filesystem::u8path(pair.second));
   if(cancelled)return reference;
   auto text=L"Não foi encontrado o arquivo:\n"+path.wstring()+L"\n\nDeseja localizá-lo? Sim: escolher arquivo. Não: manter ausente. Cancelar: interromper.";
   int answer=MessageBoxW(window,text.c_str(),L"Localizar timbre ausente",MB_YESNOCANCEL|MB_DEFBUTTON3|MB_ICONQUESTION);
   if(answer==IDCANCEL){cancelled=true;return reference;}if(answer!=IDYES){replacements.push_back({path.u8string(),path.u8string()});return reference;}
   std::filesystem::path selected;bool sf2=lp::parseSf2Reference(reference).has_value();
   if(!(sf2?sf2Dialog(selected):fileDialog(false,true,selected))){cancelled=true;return reference;}
   if(sf2){auto old=lp::parseSf2Reference(reference);auto presets=engine->sf2Presets(selected);bool found=false;for(const auto& preset:presets)found|=preset.bank==old->bank&&preset.program==old->program;if(!found)throw std::runtime_error("O SF2 escolhido não contém o banco/preset necessário");}
   replacements.push_back({path.u8string(),selected.u8string()});return lp::withReferencePath(reference,selected);
  });return !cancelled;
 }
 void clearPerformancePart(int index){
  if(index<0||index>=4)return;editorDraft=true;
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
  if(id==PartSearch&&event==EN_CHANGE){wchar_t text[257]{};GetWindowTextW(item(PartSearch),text,257);filterPerformanceChoices(text);return;}
  if(id==Search&&event==EN_CHANGE){
   wchar_t text[257]{};GetWindowTextW(item(Search),text,257);catalogSearch=text;refreshCatalog();return;
  }
  if(id==RestoreSounds){restoringSounds=true;screen=lp::Screen::Instruments;effects=false;parameterPanel.open=false;categoryIndex=14;SendMessageW(category,CB_SETCURSEL,14,0);catalogSearch.clear();SetWindowTextW(item(Search),L"");refreshCatalog();layout();SetFocus(catalog);message(L"Escolha o timbre e pressione Enter para restaurar.");return;}
  if(id==RestoreSelected){restoreSelectedSound();return;}
  if(id==RestoreAll){
   if(!hiddenCatalogReady||settingsPath.empty())throw std::runtime_error("Lista de exclusões indisponível");
   if(MessageBoxW(window,L"Restaurar todos os timbres removidos das categorias?",L"Restaurar timbres",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES){hiddenSounds.restore("",hiddenSoundsPath());refreshCatalog();layout();message(L"Todos os timbres foram restaurados.");}return;
  }
  if(id==LocateSounds){auto candidate=show;if(repairMissing(candidate)){show=std::move(candidate);refreshFavorites();refreshCatalog();updateIndicators();message(L"Verificação concluída. Salve o perfil para guardar os caminhos atualizados.");}return;}
  if(!internalEdit&&((id==PerformanceName&&event==EN_CHANGE)||(id==PerformanceModeControl&&event==CBN_SELCHANGE)||(id>=PerformancePartBase&&id<PerformancePartBase+4&&event==CBN_SELCHANGE)))editorDraft=true;

  if(id==KeyboardButton&&event==BN_CLICKED){openScreen(lp::Screen::Keyboard);return;}
  if(id==HomeButton){effects=false;parameterPanel.open=false;goHome();return;}
  if(id>=CategoryCardBase&&id<CategoryCardBase+16&&event==BN_CLICKED){categoryIndex=id-CategoryCardBase;SendMessageW(category,CB_SETCURSEL,categoryIndex,0);openCategory();return;}
  if(id==CategoryTile&&event==BN_CLICKED){openCategory();return;}
  if(id==AudioMidiButton){openScreen(lp::Screen::AudioMidi);return;}if(id==SettingsButton){openScreen(lp::Screen::Settings);return;}if(id==AboutButton){openScreen(lp::Screen::About);return;}

  if(id>=PerformanceClearBase&&id<PerformanceClearBase+4){clearPerformancePart(id-PerformanceClearBase);return;}
  if(id==AudioDriver&&event==CBN_SELCHANGE&&SendMessageW(audioDriver,CB_GETCURSEL,0,0)<1)return;
  if(id==AudioStart||(id==AudioDriver&&event==CBN_SELCHANGE)){cancelAudioRestore();if(!engine)throw std::runtime_error("Audio engine unavailable; check sfizz.dll and ASIO");auto index=driverIndex();lp::connectAndRemember(*engine,audioDevices[index].id,0,settingsPath);persistAudioAndMidi();audioStatus();}
  if(id==AudioStop&&engine){cancelAudioRestore();engine->stop();audioStatus();}
  if(id==AudioPanel){if(!engine)throw std::runtime_error("Audio engine unavailable");engine->controlPanel(audioDevices[driverIndex()].id,window);audioStatus();}
  if(id==MidiLearnKnob){int selected=-1;auto focus=GetFocus();for(int i=0;i<14;++i)if(focus==sliders[i]||focus==labels[i]){selected=i;break;}if(selected<0){message(L"Abra Parâmetros (E), selecione o knob desejado com TAB e clique MIDI Learn ou pressione Ctrl+L.");}else beginParameterLearn(static_cast<std::size_t>(selected));}
  if(id==Panic&&engine){engine->panic();message(L"Pânico: notas e caudas interrompidas.");}
  if(id==PerformanceButton){if(!preserveEditorDraft())return;syncPerformance();openScreen(lp::Screen::PerformanceEditor);
   for(int i=0;i<4;++i){SendMessageW(performancePartControls[i],CB_RESETCONTENT,0,0);SendMessageW(performancePartControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Desativado"));for(const auto& instrument:instruments){auto name=std::filesystem::u8path(lp::instrumentDisplayName(instrument)).wstring();auto row=SendMessageW(performancePartControls[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));SendMessageW(performancePartControls[i],CB_SETITEMDATA,row,&instrument-instruments.data()+1);}int selection=0;if(editingPerformanceValid&&editingPerformance.parts[i].enabled){for(std::size_t n=0;n<instruments.size();++n)if(lp::instrumentReference(instruments[n])==editingPerformance.parts[i].instrument){selection=static_cast<int>(n+1);break;}}SendMessageW(performancePartControls[i],CB_SETCURSEL,selection,0);}
   if(editingPerformanceValid){
    int mode=0;if(editingPerformance.mode==lp::PerformanceMode::Split){mode=4;if(editingPerformance.parts[0].low==editingPerformance.parts[1].low)mode=editingPerformance.parts[2].low==0?2:(editingPerformance.parts[2].high==127?1:3);}
    SendMessageW(performanceMode,CB_SETCURSEL,mode,0);internalEdit=true;SetWindowTextW(performanceName,std::filesystem::u8path(editingPerformance.name).wstring().c_str());internalEdit=false;SendMessageW(performanceEditPart,CB_SETCURSEL,editingPart,0);}
   refreshParameters();layout();SetFocus(performanceVisible?performanceMode:catalog);}
  if(id==PerformanceEditPart&&event==CBN_SELCHANGE&&editingPerformanceValid){learningPerformanceParameter=-1;
   editingPart=static_cast<int>(SendMessageW(performanceEditPart,CB_GETCURSEL,0,0));
   if(editingPart>=0&&editingPart<4){
    const auto& patch=editingPerformance.parts[editingPart].patch;
    for(int i=0;i<15;++i)SendMessageW(sliders[i],TBM_SETPOS,TRUE,static_cast<int>(patch.parameters[i]*1000));
    refreshParameters();
   }
  }
  if(id==PerformanceSave){activateEditorPerformance();storeEditorPerformance();}
  if(id==PerformancePlay)activateEditorPerformance();
  if(id==Effects){toggleParameters();}
  if(id==Save){saveProfile();}
  if(id==Open){
   if(!confirmProfileChange())return;
   std::filesystem::path path;if(fileDialog(false,false,path)){
    lp::Session candidate;candidate.load(path);
    lp::mapProfilePaths(candidate,[&](const std::string& ref){return lp::expandedReference(ref,base,path.parent_path());});
    // Resolve/cancel against the candidate, never replace the live show on failure.
    if(!repairMissing(candidate))return;
    show=std::move(candidate);currentProfile=path;savedProfile=show.serialize();editorDraft=false;
    if(engine)engine->masterVolume(show.master);router.cancelLearn();editingPerformanceValid=false;activePerformanceIndex=-1;previewActive=false;show.current.reset();if(engine)engine->unload();refreshFavorites();refreshCatalog();updateIndicators();message(L"Perfil aberto. Use Salvar perfil após novos ajustes.");
   }
  }
  if(id==Import){std::filesystem::path path;if(fileDialog(false,true,path)){lp::Instrument p;p.sfz=path;p.name=path.stem().u8string();p.category=14;if(addMySound(std::move(p)))saveMySounds();categoryIndex=14;SendMessageW(category,CB_SETCURSEL,14,0);openCategory();SendMessageW(catalog,LB_SETCURSEL,visibleInstruments.size()-1,0);audition();}}
  if(id==ImportSf2){std::filesystem::path path;if(sf2Dialog(path)){if(!engine)throw std::runtime_error("Audio engine unavailable");auto presets=engine->sf2Presets(path);if(presets.empty())throw std::runtime_error("SF2 has no presets");for(const auto& preset:presets){lp::Instrument p;p.sfz=std::filesystem::u8path(lp::makeSf2Reference(path,preset.bank,preset.program));p.name=path.stem().u8string()+" / "+preset.name+" ("+std::to_string(preset.bank)+":"+std::to_string(preset.program)+")";p.category=14;addMySound(std::move(p));}saveMySounds();categoryIndex=14;SendMessageW(category,CB_SETCURSEL,14,0);openCategory();message(L"SF2 importado: escolha um preset na lista Meus Sons.");}}
  if(id==Category&&event==CBN_SELCHANGE)refreshCatalog();
  if(id==Catalog&&event==LBN_SELCHANGE)audition();
  if(id==Favorites&&event==LBN_SELCHANGE){auto slot=SendMessageW(favorites,LB_GETCURSEL,0,0);if(slot>=0)activate(static_cast<unsigned>(slot));}
  if(id==Midi&&event==CBN_SELCHANGE){cancelMidiLearn();openMidiSelection();persistAudioAndMidi();}
 }catch(const std::exception& e){error(e);}}
 void midiEvent(DWORD packed){try{
  auto m=lp::MidiMessage::packed(packed);if(!m.valid())return;
  auto binding=lp::knobBinding(m);bool consumed=false;
  if(binding>=0){
   if(learningMaster){
    for(auto& patch:show.favorites)if(patch){for(auto& cc:patch->cc)if(cc==binding)cc=-1;if(patch->performanceMix)for(auto& cc:patch->performanceMix->cc)if(cc==binding)cc=-1;}
    for(auto& cc:previewPatch.cc)if(cc==binding)cc=-1;
    for(auto& performance:show.performanceLibrary)for(auto& part:performance.parts)for(auto& cc:part.patch.cc)if(cc==binding)cc=-1;
    for(auto& part:editingPerformance.parts)for(auto& cc:part.patch.cc)if(cc==binding)cc=-1;
    show.masterCC=binding;learningMaster=false;consumed=true;
   }else if((learningVolume>=0||learningPerformanceParameter>=0||router.pendingParameter())&&show.masterCC==binding)show.masterCC=-1;
   if(show.masterCC==binding){show.master=lp::knobValue(m);if(engine)engine->masterVolume(show.master);consumed=true;}
   if(editingPerformanceControls()){
    auto& p=editingPerformance.parts[editingPart];
    if(p.enabled){
     if(learningPerformanceParameter>=0){for(auto& cc:p.patch.cc)if(cc==binding)cc=-1;p.patch.cc[learningPerformanceParameter]=binding;learningPerformanceParameter=-1;}
     for(int i=0;i<15;++i)if(p.patch.cc[i]==binding){p.patch.parameters[i]=lp::knobValue(m);if(i>=1&&i<=4)p.patch.adsrEnabled=true;applyParameters();consumed=true;}
    }
   }else if(performanceControls()){
    if(learningVolume>=0){for(auto& part:editingPerformance.parts)if(part.patch.cc[0]==binding)part.patch.cc[0]=-1;editingPerformance.parts[learningVolume].patch.cc[0]=binding;learningVolume=-1;}
    for(int part=0;part<4;++part){auto& p=editingPerformance.parts[part];if(p.enabled&&p.patch.cc[0]==binding){p.patch.parameters[0]=lp::knobValue(m);engine->setPerformancePartParameters(part,p.patch.parameters,p.patch.adsrEnabled);consumed=true;}}
    if(consumed)syncPerformance();
   }else if(previewActive&&!show.current){
    if(learningPerformanceParameter>=0){for(auto& cc:previewPatch.cc)if(cc==binding)cc=-1;previewPatch.cc[learningPerformanceParameter]=binding;learningPerformanceParameter=-1;}
    for(int i=0;i<15;++i)if(previewPatch.cc[i]==binding){previewPatch.parameters[i]=lp::knobValue(m);if(i>=1&&i<=4)previewPatch.adsrEnabled=true;applyParameters();consumed=true;}
   }
  }
  if(consumed){refreshParameters();if(engine&&m.data1==1)engine->send(m);return;}
  auto result=router.route(m);
  if(result.action==lp::MidiAction::WrongProgram)message(L"Aguardando Program Change "+std::to_wstring(result.index+1)+L".");
  else if(result.action==lp::MidiAction::FavoriteLearned)message(L"Program Change aprendido. Use Salvar perfil para guardar.");
  else if(result.action==lp::MidiAction::Favorite)activate(result.index);
  else if(result.action==lp::MidiAction::Parameter||result.action==lp::MidiAction::Learned){applyParameters();refreshParameters();}
  if(engine&&lp::forwardsToEngine(result,m))engine->send(m);
 }catch(const std::exception& e){error(e);}}

 #include "vision_adapter.inc"
};
// Keep native combo interaction and accessibility while finishing the painted arrow.
LRESULT CALLBACK comboProc(HWND control,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 auto result=DefSubclassProc(control,msg,wp,lp);
 if(msg==WM_PAINT){auto dc=GetDC(control);COMBOBOXINFO info{};info.cbSize=sizeof(info);GetComboBoxInfo(control,&info);RECT r{};GetClientRect(control,&r);
  lpui::fill(dc,info.rcButton,lpui::panel);int cx=(info.rcButton.left+info.rcButton.right)/2,cy=(info.rcButton.top+info.rcButton.bottom)/2;lpui::line(dc,cx-5,cy-2,cx,cy+3,lpui::gold,2);lpui::line(dc,cx,cy+3,cx+5,cy-2,lpui::gold,2);
  lpui::box(dc,{0,0,r.right-1,r.bottom-1},GetFocus()==control?lpui::gold:RGB(78,91,110),GetFocus()==control?2:1);ReleaseDC(control,dc);
 }
 if(msg==WM_SETFOCUS||msg==WM_KILLFOCUS)InvalidateRect(control,nullptr,FALSE);
 if(msg==WM_NCDESTROY)RemoveWindowSubclass(control,comboProc,id);
 return result;
}
LRESULT CALLBACK focusProc(HWND control,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR root){
 if(msg==WM_PAINT&&GetDlgCtrlID(control)==PerformanceName){auto result=DefSubclassProc(control,msg,wp,lp);auto dc=GetDC(control);RECT r{};GetClientRect(control,&r);lpui::box(dc,{0,0,r.right-1,r.bottom-1},GetFocus()==control?lpui::gold:RGB(78,91,110),1);ReleaseDC(control,dc);return result;}
 if(msg==WM_MOUSEMOVE){if(!GetPropW(control,L"LPHover")){SetPropW(control,L"LPHover",reinterpret_cast<HANDLE>(1));TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,control,0};TrackMouseEvent(&track);InvalidateRect(control,nullptr,FALSE);}}
 if(msg==WM_MOUSELEAVE){RemovePropW(control,L"LPHover");InvalidateRect(control,nullptr,FALSE);}
 if(msg==WM_NCDESTROY)RemovePropW(control,L"LPHover");
 if(msg==WM_SETFOCUS)PostMessageW(reinterpret_cast<HWND>(root),RevealFocusMessage,0,reinterpret_cast<LPARAM>(control));
 if(msg==WM_NCDESTROY)RemoveWindowSubclass(control,focusProc,id);
 return DefSubclassProc(control,msg,wp,lp);
}
LRESULT CALLBACK pageProc(HWND page,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR context){
 auto& app=*reinterpret_cast<App*>(context);
 switch(msg){
 case WM_PAINT:{
  PAINTSTRUCT ps{};auto dc=BeginPaint(page,&ps);RECT r{};GetClientRect(page,&r);auto buffer=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,std::max(1L,r.right),std::max(1L,r.bottom));auto previous=SelectObject(buffer,bitmap);
  lpui::fill(buffer,r,lpui::ink);SetViewportOrgEx(buffer,0,-App::pageOrigin-app.pageScroll.offset,nullptr);app.paint(buffer,true);SetViewportOrgEx(buffer,0,0,nullptr);
  BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,previous);DeleteObject(bitmap);DeleteDC(buffer);EndPaint(page,&ps);return 0;
 }
 case WM_ERASEBKGND:return 1;
 case WM_VSCROLL:{
  SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_TRACKPOS;GetScrollInfo(page,SB_VERT,&info);
  int target=app.pageScroll.offset;switch(LOWORD(wp)){
   case SB_LINEUP:target-=48;break;case SB_LINEDOWN:target+=48;break;
   case SB_PAGEUP:target-=app.pageScroll.viewport-48;break;case SB_PAGEDOWN:target+=app.pageScroll.viewport-48;break;
   case SB_THUMBTRACK:case SB_THUMBPOSITION:target=info.nTrackPos;break;
   case SB_TOP:target=0;break;case SB_BOTTOM:target=app.pageScroll.maximum();break;default:return 0;
  }
  if(app.pageScroll.seek(target))app.layout();return 0;
 }
 case WM_LBUTTONDOWN:if(!app.effects&&app.screen==lp::Screen::Keyboard){auto point=app.pagePoint(lp);if(PtInRect(&app.keyboardRect,point)){SetCapture(page);app.virtualKey(point);return 0;}}SetFocus(app.window);return 0;
 case WM_MOUSEMOVE:if(GetCapture()==page){app.virtualKey(app.pagePoint(lp));return 0;}break;
 case WM_LBUTTONUP:if(GetCapture()==page){app.releaseVirtual();ReleaseCapture();return 0;}break;
 case WM_CAPTURECHANGED:app.releaseVirtual();return 0;
 case WM_NOTIFY:case WM_COMMAND:case WM_DRAWITEM:case WM_MEASUREITEM:case WM_HSCROLL:case WM_CONTEXTMENU:
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:
 case BeginParameterLearn:case ToggleFavoriteMessage:case SelectSoundMessage:
  return SendMessageW(app.window,msg,wp,lp);
 case WM_NCDESTROY:RemoveWindowSubclass(page,pageProc,id);break;
 }
 return DefSubclassProc(page,msg,wp,lp);
}
LRESULT CALLBACK listProc(HWND control,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(msg==WM_PAINT&&SendMessageW(control,LB_GETCOUNT,0,0)==0){auto result=DefSubclassProc(control,msg,wp,lp);auto dc=GetDC(control);RECT r{};GetClientRect(control,&r);auto font=reinterpret_cast<HFONT>(SendMessageW(control,WM_GETFONT,0,0));
  lpui::fill(dc,r,lpui::ink);lpui::surface(dc,{8,8,r.right-8,180},lpui::panel,lpui::panel,RGB(52,63,80),14);
  lpui::text(dc,L"Nenhum timbre nesta categoria",{24,28,r.right-24,110},font,lpui::white,DT_CENTER|DT_VCENTER|DT_WORDBREAK|DT_NOPREFIX);ReleaseDC(control,dc);return result;}
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(control,&r);if(static_cast<short>(LOWORD(lp))>=r.right-60){auto hit=SendMessageW(control,LB_ITEMFROMPOINT,0,lp);if(!HIWORD(hit)){SendMessageW(control,LB_SETCURSEL,LOWORD(hit),0);SendMessageW(GetParent(control),ToggleFavoriteMessage,id,0);return 0;}}}
 if(msg==WM_LBUTTONUP){auto result=DefSubclassProc(control,msg,wp,lp);SendMessageW(GetParent(control),SelectSoundMessage,reinterpret_cast<WPARAM>(control),0);return result;}
 if(msg==WM_NCDESTROY)RemoveWindowSubclass(control,listProc,id);
 return DefSubclassProc(control,msg,wp,lp);
}
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 auto app=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));
 if(msg==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
 if(!app)return DefWindowProcW(w,msg,wp,lp);
 switch(msg){
 case WM_CREATE:try{app->initialize(w);SetWindowSubclass(app->catalog,listProc,Catalog,0);SetWindowSubclass(app->favorites,listProc,Favorites,0);app->startVision();}catch(const std::exception& e){app->error(e);return -1;}return 0;
 case LoadLibraryMessage:try{if(wp==1)app->loadNextBank();else app->loadLibrary();}catch(const std::exception& e){app->loading=false;app->layout();app->error(e);}return 0;
 case VisionCommandMessage:{std::unique_ptr<std::wstring> wire(reinterpret_cast<std::wstring*>(lp));if(wire)app->visionCommand(*wire);return 0;}
 case WM_TIMER:
  if(wp==4){try{app->pollLevelCalibration();}catch(const std::exception& e){KillTimer(w,4);app->levelCalibration.reset();app->error(e);app->completeLibrary(L"Falha ao aplicar equilibrio de volumes. Confira os avisos.");}return 0;}
  if(wp==3){app->visionLoadTimeout();return 0;}
  if(wp==2){app->restoreAudio();app->flushVision();return 0;}
  if(app->engine&&app->engine->takeFault()){try{app->engine->stop();}catch(const std::exception& e){app->error(e);}app->message(L"Falha no motor/driver. Áudio interrompido; reconecte ASIO.");}app->updateIndicators();app->flushVision();return 0;
 case WM_SIZE:if(app->catalog)app->layout();return 0;
 case WM_PAINT:{PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);auto buffer=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,std::max(1L,r.right),std::max(1L,r.bottom));auto previous=SelectObject(buffer,bitmap);app->paint(buffer);BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,previous);DeleteObject(bitmap);DeleteDC(buffer);EndPaint(w,&ps);return 0;}
 case WM_NOTIFY:{auto custom=reinterpret_cast<NMCUSTOMDRAW*>(lp);if(custom->hdr.code==NM_CUSTOMDRAW){
  for(auto slider:app->sliders)if(custom->hdr.hwndFrom==slider){if(custom->dwDrawStage==CDDS_PREPAINT){app->drawSlider(custom);return CDRF_SKIPDEFAULT;}}
 }break;}
 case WM_DRAWITEM:{auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item->CtlType==ODT_BUTTON)app->drawButton(item);else if(item->CtlType==ODT_LISTBOX)app->drawList(item);else if(item->CtlType==ODT_COMBOBOX)app->drawCombo(item);return TRUE;}
 case WM_MEASUREITEM:{auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);if(item->CtlType==ODT_LISTBOX){item->itemHeight=68;return TRUE;}if(item->CtlType==ODT_COMBOBOX){item->itemHeight=40;return TRUE;}break;}
 case WM_COMMAND:app->command(LOWORD(wp),HIWORD(wp));return 0;
 case MM_MIM_DATA:if(reinterpret_cast<HMIDIIN>(wp)==app->midiHandle)app->midiEvent(static_cast<DWORD>(lp));return 0;
 case ToggleFavoriteMessage:app->favorite();return 0;
 case SelectSoundMessage:if(reinterpret_cast<HWND>(wp)==app->catalog)app->audition();else {auto slot=SendMessageW(app->favorites,LB_GETCURSEL,0,0);if(slot>=0)app->activate(static_cast<unsigned>(slot));}return 0;
 case RevealFocusMessage:app->revealFocus(reinterpret_cast<HWND>(lp));return 0;
 case BeginParameterLearn:app->beginParameterLearn(static_cast<std::size_t>(wp));return 0;
 case WM_HSCROLL:for(int i=0;i<16;++i)if(reinterpret_cast<HWND>(lp)==app->sliders[i]){app->setPanelValue(i,static_cast<float>(SendMessageW(app->sliders[i],TBM_GETPOS,0,0))/1000);return 0;}break;
 case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE)app->releaseVirtual();break;
 case WM_CONTEXTMENU:if(reinterpret_cast<HWND>(wp)==app->favorites){
  auto slot=SendMessageW(app->favorites,LB_GETCURSEL,0,0);
  if(lp!=-1){POINT pt{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};ScreenToClient(app->favorites,&pt);auto hit=SendMessageW(app->favorites,LB_ITEMFROMPOINT,0,MAKELPARAM(pt.x,pt.y));slot=HIWORD(hit)?-1:LOWORD(hit);}
  if(slot>=0){try{app->router.learnFavorite(static_cast<std::size_t>(slot));app->message(L"MIDI Learn: pressione o Program Change correspondente ao favorito (1 a 20). ESC cancela.");}catch(const std::exception& e){app->error(e);}}return 0;
 }break;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,lpui::white);int id=GetDlgCtrlID(reinterpret_cast<HWND>(lp));bool panel=(id>=Knob+100&&id<Knob+116)||id==PerformanceName||id==AboutText||id==2;SetBkColor(dc,panel?lpui::panel:lpui::ink);return reinterpret_cast<LRESULT>(panel?app->panelBackground:app->background);}
 case WM_ERASEBKGND:return 1;
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={1024,600};return 0;
 case WM_CLOSE:try{if(!app->confirmProfileChange())return 0;app->releaseVirtual();if(app->engine)app->persistAudioAndMidi();}catch(const std::exception& e){app->error(e);return 0;}break;
 case WM_DESTROY:app->levelCalibration.reset();app->closeVision();app->closeMidi();PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(w,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
 #ifdef LP_VISION_UI
 HRESULT comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
#endif
 INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&common);
 Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return 1;
 int result=0;{
 App app;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LPbyHSAccessible";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(3));RegisterClassW(&cls);
 auto w=CreateWindowW(cls.lpszClassName,L"Live Performance by Hugo Sodré",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1360,900,nullptr,nullptr,instance,&app);
 if(w){if(!app.visionPending())ShowWindow(w,show);UpdateWindow(w);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){
  if(app.visionVisible()){TranslateMessage(&msg);DispatchMessageW(&msg);continue;}
  if(app.keyCommand(msg))continue;
  if(app.wheelCommand(msg))continue;
  if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
 }result=static_cast<int>(msg.wParam);}else result=1;
 }
 Gdiplus::GdiplusShutdown(token);
#ifdef LP_VISION_UI
 if(SUCCEEDED(comResult))CoUninitialize();
#endif
 return result;
}
