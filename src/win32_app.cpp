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
#include "audio_preferences.hpp"
#include <memory>
#include <string>

namespace {
constexpr int Catalog=100,Favorites=101,Save=102,Open=103,Midi=104,Effects=105,Import=106,AudioDriver=107,AudioBuffer=108,AudioStart=109,AudioStop=110,AudioPanel=111,Panic=112,Category=113,Knob=200;
constexpr UINT BeginParameterLearn=WM_APP+40;
// Do not rely on a native trackbar forwarding its mouse context request.
LRESULT CALLBACK parameterProc(HWND control,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR parameter,DWORD_PTR){
 switch(msg){
 case WM_RBUTTONDOWN:return 0;
 case WM_RBUTTONUP:case WM_CONTEXTMENU:
  SendMessageW(GetParent(control),BeginParameterLearn,parameter,0);return 0;
 case WM_KEYDOWN:case WM_SYSKEYDOWN:
  if(wp==VK_APPS||(wp==VK_F10&&(GetKeyState(VK_SHIFT)&0x8000))){SendMessageW(GetParent(control),BeginParameterLearn,parameter,0);return 0;}break;
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
 std::unique_ptr<lp::AudioHost> engine;
 HWND audioDriver{},audioBuffer{};
 std::vector<lp::AudioDevice> audioDevices;
 const std::array<unsigned long,7> buffers{0,64,128,256,512,1024,2048};
 std::filesystem::path settingsPath;
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
  try{auto dll=base/L"sfizz.dll";if(!std::filesystem::exists(dll))dll=base/L"recovery/extracted/live/sfizz.dll";engine=std::make_unique<lp::AudioHost>(dll);
   audioDevices=engine->devices();for(const auto& device:audioDevices){auto text=std::filesystem::u8path(device.name).wstring();SendMessageW(audioDriver,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));}
   if(audioDevices.empty())message(L"Nenhum driver ASIO estéreo encontrado. Instale ou habilite seu driver ASIO. Não há fallback Windows.");
   PWSTR roaming=nullptr;auto folderResult=SHGetKnownFolderPath(FOLDERID_RoamingAppData,KF_FLAG_CREATE,nullptr,&roaming);
   if(FAILED(folderResult)||!roaming){if(roaming)CoTaskMemFree(roaming);throw std::runtime_error("Pasta permanente de configurações de áudio indisponível");}
   auto folder=std::filesystem::path(roaming)/L"Live Performance by Hugo Sodre";CoTaskMemFree(roaming);
   std::filesystem::create_directories(folder);settingsPath=folder/L"audio.settings";
   if(std::filesystem::exists(settingsPath)){
    auto saved=lp::AudioSettings::load(settingsPath);int device=lp::resolveAsioDriver(audioDevices,saved.driver);
    for(std::size_t i=0;i<audioDevices.size();++i)if(audioDevices[i].id==device)SendMessageW(audioDriver,CB_SETCURSEL,i+1,0);
    auto buffer=std::find(buffers.begin(),buffers.end(),saved.buffer);if(buffer==buffers.end())throw std::runtime_error("Buffer ASIO salvo inválido");
    SendMessageW(audioBuffer,CB_SETCURSEL,buffer-buffers.begin(),0);
    pendingRestore=true;SetTimer(window,2,750,nullptr);message(L"Reconectando automaticamente o último driver ASIO...");
   }
  }catch(const std::exception& e){error(e);message(L"Áudio não inicializado ou configurações não restauradas. Confira o driver ASIO e tente conectar.");}
  layout();SetFocus(catalog);SetTimer(window,1,500,nullptr);
 }
 void layout(){RECT r{};GetClientRect(window,&r);int width=r.right,height=r.bottom,half=(width-48)/2;
  MoveWindow(GetDlgItem(window,1),16,12,width-32,40,TRUE);
  MoveWindow(GetDlgItem(window,Open),16,60,190,44,TRUE);MoveWindow(GetDlgItem(window,Save),214,60,190,44,TRUE);
  MoveWindow(GetDlgItem(window,Import),412,60,190,44,TRUE);MoveWindow(effectsButton,610,60,240,44,TRUE);
  MoveWindow(midi,16,116,width-32,320,TRUE);
  MoveWindow(audioDriver,16,170,half,320,TRUE);MoveWindow(audioBuffer,32+half,170,half,320,TRUE);
  MoveWindow(GetDlgItem(window,AudioStart),16,224,210,44,TRUE);MoveWindow(GetDlgItem(window,AudioStop),234,224,190,44,TRUE);
  MoveWindow(GetDlgItem(window,AudioPanel),432,224,200,44,TRUE);MoveWindow(GetDlgItem(window,Panic),640,224,210,44,TRUE);
  MoveWindow(category,16,284,half,350,TRUE);
  MoveWindow(catalog,16,338,half,height-428,TRUE);MoveWindow(favorites,32+half,284,half,height-374,TRUE);
  ShowWindow(category,effects?SW_HIDE:SW_SHOW);
  ShowWindow(catalog,effects?SW_HIDE:SW_SHOW);ShowWindow(favorites,effects?SW_HIDE:SW_SHOW);
  for(int i=0;i<14;++i){int col=i/7,row=i%7;int x=16+col*(half+16),y=284+row*((height-384)/7);
   MoveWindow(labels[i],x,y,half,30,TRUE);MoveWindow(sliders[i],x,y+30,half,32,TRUE);
   ShowWindow(labels[i],effects?SW_SHOW:SW_HIDE);ShowWindow(sliders[i],effects?SW_SHOW:SW_HIDE);
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
 void audition(){try{auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=visibleInstruments.size())return;if(!engine)throw std::runtime_error("Audio engine unavailable");engine->load(instruments[visibleInstruments[selected]].sfz,lp::Patch{}.parameters);router.cancelLearn();show.current.reset();refreshParameters();audioStatus();}catch(const std::exception& e){error(e);}}
 std::wstring parameterName(int index){return std::wstring(lp::parameterNames[index],lp::parameterNames[index]+std::char_traits<char>::length(lp::parameterNames[index]));}
 void beginParameterLearn(std::size_t index){
  if(index>=sliders.size())return;
  if(!show.current||!show.favorites[*show.current]){message(L"Selecione um timbre nos Favoritos antes de mapear os parâmetros MIDI.");return;}
  if(!midiHandle){message(L"Selecione a entrada MIDI do seu teclado antes de iniciar o MIDI Learn.");return;}
  try{router.learn(index);refreshParameters();SetFocus(sliders[index]);message(L"MIDI Learn — "+parameterName(static_cast<int>(index))+L": mova o fader ou knob físico do teclado que envia CC. ESC cancela.");}catch(const std::exception& e){error(e);}
 }
 void cancelMidiLearn(){bool wasLearning=router.learning();router.cancelLearn();refreshParameters();if(wasLearning)message(L"MIDI Learn cancelado.");}
 void refreshCatalog(){SendMessageW(catalog,LB_RESETCONTENT,0,0);visibleInstruments.clear();auto selected=SendMessageW(category,CB_GETCURSEL,0,0);for(std::size_t i=0;i<instruments.size();++i){const auto& p=instruments[i];if(selected<0||p.category!=static_cast<unsigned>(selected))continue;visibleInstruments.push_back(i);auto name=std::filesystem::u8path(p.name).wstring();SendMessageW(catalog,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}if(!visibleInstruments.empty())SendMessageW(catalog,LB_SETCURSEL,0,0);}
 void refreshFavorites(){SendMessageW(favorites,LB_RESETCONTENT,0,0);for(int i=0;i<20;++i){std::wstring name=std::to_wstring(i+1)+L". ";name+=show.favorites[i]?std::filesystem::u8path(lp::instrumentName(show.favorites[i]->sfz,instruments)).wstring():L"Vazio";SendMessageW(favorites,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}refreshParameters();}
 void refreshParameters(){for(int i=0;i<14;++i){float value=show.current&&show.favorites[*show.current]?show.favorites[*show.current]->parameters[i]:0;auto text=std::wstring(lp::parameterNames[i],lp::parameterNames[i]+std::char_traits<char>::length(lp::parameterNames[i]))+L": "+std::to_wstring(static_cast<int>(value*100))+L"%";if(i>=1&&i<=4&&show.current&&show.favorites[*show.current]&&!show.favorites[*show.current]->adsrEnabled)text+=L" (SFZ original)";if(show.current&&show.favorites[*show.current]){int cc=show.favorites[*show.current]->cc[i];if(cc>=0)text+=L" — CC "+std::to_wstring(cc);}
  if(router.pendingParameter()==static_cast<std::size_t>(i))text+=L" — aguardando CC";
  SetWindowTextW(labels[i],text.c_str());SendMessageW(sliders[i],TBM_SETPOS,TRUE,static_cast<int>(value*1000));EnableWindow(sliders[i],show.current.has_value());}}
 void applyParameters(){if(engine&&show.current&&show.favorites[*show.current])engine->parameters(show.favorites[*show.current]->parameters,show.favorites[*show.current]->adsrEnabled);}
 void activate(unsigned slot){if(slot>=20||!show.favorites[slot])return;try{if(!engine)throw std::runtime_error("Audio engine unavailable");engine->load(lp::resolveInstrument(show.favorites[slot]->sfz,instruments),show.favorites[slot]->parameters,show.favorites[slot]->adsrEnabled);router.cancelLearn();show.programChange(slot);applyParameters();SendMessageW(favorites,LB_SETCURSEL,slot,0);refreshParameters();audioStatus();}catch(const std::exception& e){error(e);}}
 void favorite(){auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=visibleInstruments.size())return;auto path=lp::instrumentReference(instruments[visibleInstruments[selected]]);for(unsigned i=0;i<20;++i)if(show.favorites[i]&&show.favorites[i]->sfz==path){activate(i);return;}for(unsigned i=0;i<20;++i)if(!show.favorites[i]){show.favorites[i]=lp::Patch{};show.favorites[i]->sfz=path;refreshFavorites();activate(i);return;}message(L"Os 20 favoritos estão ocupados.");}
 bool fileDialog(bool saving,bool sfz,std::filesystem::path& path){wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=sfz?L"Instrumento SFZ\0*.sfz\0\0":L"Perfil Live Performance\0*.lphs\0\0";dialog.lpstrDefExt=sfz?L"sfz":L"lphs";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(saving?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);bool ok=saving?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog);if(ok)path=file;else if(CommDlgExtendedError())throw std::runtime_error("File dialog failed");return ok;}
 void command(int id,int event){try{
  if((id==AudioDriver||id==AudioBuffer)&&event==CBN_SELCHANGE&&SendMessageW(audioDriver,CB_GETCURSEL,0,0)<1)return;
  if(id==AudioStart||((id==AudioDriver||id==AudioBuffer)&&event==CBN_SELCHANGE)){cancelAudioRestore();if(!engine)throw std::runtime_error("Audio engine unavailable; check sfizz.dll and ASIO");auto index=driverIndex();auto selected=SendMessageW(audioBuffer,CB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=buffers.size())throw std::runtime_error("Select ASIO buffer");lp::connectAndRemember(*engine,audioDevices[index].id,buffers[selected],settingsPath);audioStatus();}
  if(id==AudioStop&&engine){cancelAudioRestore();engine->stop();audioStatus();}
  if(id==AudioPanel){if(!engine)throw std::runtime_error("Audio engine unavailable");engine->controlPanel(audioDevices[driverIndex()].id,window);audioStatus();}
  if(id==Panic&&engine){engine->panic();message(L"Pânico: notas e caudas interrompidas.");}
  if(id==Effects){effects=!effects;if(!effects)cancelMidiLearn();layout();SetFocus(effects?sliders[0]:catalog);}
  if(id==Save||id==Open){std::filesystem::path path;if(fileDialog(id==Save,false,path)){if(id==Save)show.save(path);else{show.load(path);router.cancelLearn();if(engine)engine->unload();refreshFavorites();}message(L"Perfil processado. Formato novo .lphs; INI antigo ainda não é compatível.");}}
  if(id==Import){std::filesystem::path path;if(fileDialog(false,true,path)){lp::Instrument p;p.sfz=path;p.name=path.stem().u8string();p.category=10;instruments.push_back(p);SendMessageW(category,CB_SETCURSEL,10,0);refreshCatalog();SendMessageW(catalog,LB_SETCURSEL,visibleInstruments.size()-1,0);}}
  if(id==Category&&event==CBN_SELCHANGE)refreshCatalog();
  if(id==Catalog&&event==LBN_DBLCLK)audition();
  if(id==Favorites&&event==LBN_SELCHANGE){auto slot=SendMessageW(favorites,LB_GETCURSEL,0,0);if(slot>=0)activate(static_cast<unsigned>(slot));}
  if(id==Midi&&event==CBN_SELCHANGE){cancelMidiLearn();closeMidi();auto index=SendMessageW(midi,CB_GETCURSEL,0,0);if(index>0){auto result=midiInOpen(&midiHandle,static_cast<UINT>(index-1),reinterpret_cast<DWORD_PTR>(window),0,CALLBACK_WINDOW);if(result!=MMSYSERR_NOERROR)throw std::runtime_error("Cannot open MIDI input");if(midiInStart(midiHandle)!=MMSYSERR_NOERROR){closeMidi();throw std::runtime_error("Cannot start MIDI input");}}}
 }catch(const std::exception& e){error(e);}}
 void midiEvent(DWORD packed){try{auto midiMessage=lp::MidiMessage::packed(packed);auto result=router.route(midiMessage);if(result.action==lp::MidiAction::WrongProgram){message(L"Aguardando PC "+std::to_wstring(result.index+1)+L". Recebido PC "+std::to_wstring(result.value+1)+L". Configure o botão do controlador para o número do favorito.");}
 else if(result.action==lp::MidiAction::FavoriteLearned){message(L"Program Change "+std::to_wstring(result.index+1)+L" aprendido. Use Salvar perfil para guardar o vínculo.");}
 else if(result.action==lp::MidiAction::Favorite)activate(result.index);else if(result.action==lp::MidiAction::Parameter||result.action==lp::MidiAction::Learned){applyParameters();refreshParameters();if(result.action==lp::MidiAction::Learned)message(L"CC "+std::to_wstring(result.value)+L" aprendido para "+parameterName(result.index)+L". O controle já está ativo. Use Salvar perfil para guardar o mapeamento.");}
 if(engine&&lp::forwardsToEngine(result,midiMessage))engine->send(midiMessage);}catch(const std::exception& e){error(e);}}
};
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){auto app=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(msg==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}if(!app)return DefWindowProcW(w,msg,wp,lp);
 switch(msg){case WM_CREATE:try{app->initialize(w);}catch(const std::exception& e){app->error(e);return -1;}return 0;
 case WM_TIMER:if(wp==2){app->restoreAudio();return 0;}if(app->engine&&app->engine->takeFault()){try{app->engine->stop();}catch(const std::exception& e){app->error(e);}app->message(L"Falha no motor/driver. Áudio interrompido; reconecte ASIO.");}return 0;
 case WM_SIZE:if(app->catalog)app->layout();return 0;
 case WM_COMMAND:app->command(LOWORD(wp),HIWORD(wp));return 0;
 case MM_MIM_DATA:if(reinterpret_cast<HMIDIIN>(wp)==app->midiHandle)app->midiEvent(static_cast<DWORD>(lp));return 0;
 case BeginParameterLearn:app->beginParameterLearn(static_cast<std::size_t>(wp));return 0;
 case WM_HSCROLL:for(int i=0;i<14;++i)if(reinterpret_cast<HWND>(lp)==app->sliders[i]&&app->show.current){app->show.parameter(i,static_cast<float>(SendMessageW(app->sliders[i],TBM_GETPOS,0,0))/1000);app->applyParameters();app->refreshParameters();}return 0;
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
 case WM_CLOSE:try{if(app->engine)lp::rememberConnectedAudio(*app->engine,app->settingsPath);}catch(const std::exception& e){app->error(e);}break;
 case WM_DESTROY:app->closeMidi();PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};InitCommonControlsEx(&common);App app;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LPbyHSReconstruction";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);auto w=CreateWindowW(cls.lpszClassName,L"Live Performance by Hugo Sodré",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1200,940,nullptr,nullptr,instance,&app);if(!w)return 1;ShowWindow(w,show);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_KEYDOWN){if(msg.wParam==VK_ESCAPE){app.cancelMidiLearn();app.effects=false;app.layout();SetFocus(app.catalog);continue;}if(msg.wParam==VK_F12){app.command(Panic,0);continue;}if(msg.wParam==VK_RETURN&&GetFocus()==app.catalog){app.audition();continue;}if(msg.wParam=='E'){app.command(Effects,0);continue;}if(msg.wParam==VK_SPACE&&GetFocus()==app.catalog){app.favorite();continue;}}if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}return static_cast<int>(msg.wParam);}
