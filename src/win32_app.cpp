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
#include "session.hpp"
#include "midi.hpp"
#include "sfizz_engine.hpp"
#include <memory>
#include <string>

namespace {
constexpr int Catalog=100,Favorites=101,Save=102,Open=103,Midi=104,Effects=105,Import=106,Knob=200;
struct App {
 HWND window{},catalog{},favorites{},status{},midi{},effectsButton{};
 std::array<HWND,14> sliders{},labels{};
 HFONT font{}; HBRUSH background=CreateSolidBrush(RGB(12,16,24));
 lp::Session show; lp::MidiRouter router{show};
 std::vector<std::filesystem::path> instruments;
 std::unique_ptr<lp::SfizzEngine> engine;
 HMIDIIN midiHandle{};bool effects=false;
 std::filesystem::path base;
 ~App(){closeMidi();engine.reset();if(font)DeleteObject(font);DeleteObject(background);}
 void closeMidi(){if(midiHandle){midiInStop(midiHandle);midiInReset(midiHandle);midiInClose(midiHandle);midiHandle=nullptr;}}
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
  catalog=control(L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP,Catalog);
  favorites=control(L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP,Favorites);
  midi=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,Midi);
  SendMessageW(midi,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"MIDI desligado"));
  for(UINT i=0;i<midiInGetNumDevs();++i){MIDIINCAPSW caps{};if(midiInGetDevCapsW(i,&caps,sizeof(caps))==MMSYSERR_NOERROR)SendMessageW(midi,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(caps.szPname));else SendMessageW(midi,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Dispositivo indisponível"));}
  SendMessageW(midi,CB_SETCURSEL,0,0);
  status=control(L"STATIC",L"Saída ASIO e DSP em reconstrução. Esta interface ainda não toca áudio.",SS_LEFT,2);
  for(int i=0;i<14;++i){labels[i]=control(L"STATIC",L"",SS_LEFT,Knob+100+i);sliders[i]=control(TRACKBAR_CLASSW,L"",TBS_HORZ|WS_TABSTOP,Knob+i);SendMessageW(sliders[i],TBM_SETRANGE,TRUE,MAKELPARAM(0,1000));}
  auto samples=base/L"samples";
  if(!std::filesystem::is_directory(samples))samples=base/L"recovery/extracted/live/samples";
  if(!std::filesystem::is_directory(samples))samples=std::filesystem::current_path()/L"recovery/extracted/live/samples";
  instruments=lp::catalog(samples);refreshCatalog();refreshFavorites();
  try{auto dll=base/L"sfizz.dll";if(!std::filesystem::exists(dll))dll=base/L"recovery/extracted/live/sfizz.dll";engine=std::make_unique<lp::SfizzEngine>(dll);}catch(const std::exception&){message(L"sfizz não encontrado. Catálogo e perfis disponíveis; áudio em reconstrução.");}
  layout();SetFocus(catalog);
 }
 void layout(){RECT r{};GetClientRect(window,&r);int width=r.right,height=r.bottom,half=(width-48)/2;
  MoveWindow(GetDlgItem(window,1),16,12,width-32,40,TRUE);
  MoveWindow(GetDlgItem(window,Open),16,60,190,44,TRUE);MoveWindow(GetDlgItem(window,Save),214,60,190,44,TRUE);
  MoveWindow(GetDlgItem(window,Import),412,60,190,44,TRUE);MoveWindow(effectsButton,610,60,240,44,TRUE);
  MoveWindow(midi,16,116,width-32,320,TRUE);
  MoveWindow(catalog,16,170,half,height-260,TRUE);MoveWindow(favorites,32+half,170,half,height-260,TRUE);
  ShowWindow(catalog,effects?SW_HIDE:SW_SHOW);ShowWindow(favorites,effects?SW_HIDE:SW_SHOW);
  for(int i=0;i<14;++i){int col=i/7,row=i%7;int x=16+col*(half+16),y=170+row*((height-270)/7);
   MoveWindow(labels[i],x,y,half,30,TRUE);MoveWindow(sliders[i],x,y+30,half,32,TRUE);
   ShowWindow(labels[i],effects?SW_SHOW:SW_HIDE);ShowWindow(sliders[i],effects?SW_SHOW:SW_HIDE);
  }
  MoveWindow(status,16,height-78,width-32,70,TRUE);InvalidateRect(window,nullptr,TRUE);
 }
 void refreshCatalog(){SendMessageW(catalog,LB_RESETCONTENT,0,0);for(auto& p:instruments)SendMessageW(catalog,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(p.stem().c_str()));}
 void refreshFavorites(){SendMessageW(favorites,LB_RESETCONTENT,0,0);for(int i=0;i<20;++i){std::wstring name=std::to_wstring(i+1)+L". ";name+=show.favorites[i]?std::filesystem::u8path(show.favorites[i]->sfz).stem().wstring():L"Vazio";SendMessageW(favorites,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}refreshParameters();}
 void refreshParameters(){for(int i=0;i<14;++i){float value=show.current&&show.favorites[*show.current]?show.favorites[*show.current]->parameters[i]:0;auto text=std::wstring(lp::parameterNames[i],lp::parameterNames[i]+std::char_traits<char>::length(lp::parameterNames[i]))+L": "+std::to_wstring(static_cast<int>(value*100))+L"%";SetWindowTextW(labels[i],text.c_str());SendMessageW(sliders[i],TBM_SETPOS,TRUE,static_cast<int>(value*1000));EnableWindow(sliders[i],show.current.has_value());}}
 void activate(unsigned slot){if(slot>=20||!show.favorites[slot])return;try{if(engine)engine->load(std::filesystem::u8path(show.favorites[slot]->sfz));router.cancelLearn();show.programChange(slot);SendMessageW(favorites,LB_SETCURSEL,slot,0);refreshParameters();message(L"Favorito carregado. Saída ASIO e efeitos ainda em reconstrução.");}catch(const std::exception& e){error(e);}}
 void favorite(){auto selected=SendMessageW(catalog,LB_GETCURSEL,0,0);if(selected<0||static_cast<std::size_t>(selected)>=instruments.size())return;auto path=instruments[selected].u8string();for(unsigned i=0;i<20;++i)if(show.favorites[i]&&show.favorites[i]->sfz==path){activate(i);return;}for(unsigned i=0;i<20;++i)if(!show.favorites[i]){show.favorites[i]=lp::Patch{};show.favorites[i]->sfz=path;refreshFavorites();activate(i);return;}message(L"Os 20 favoritos estão ocupados.");}
 bool fileDialog(bool saving,bool sfz,std::filesystem::path& path){wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=sfz?L"Instrumento SFZ\0*.sfz\0\0":L"Perfil Live Performance\0*.lphs\0\0";dialog.lpstrDefExt=sfz?L"sfz":L"lphs";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(saving?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);bool ok=saving?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog);if(ok)path=file;else if(CommDlgExtendedError())throw std::runtime_error("File dialog failed");return ok;}
 void command(int id,int event){try{
  if(id==Effects){effects=!effects;layout();SetFocus(effects?sliders[0]:catalog);}
  if(id==Save||id==Open){std::filesystem::path path;if(fileDialog(id==Save,false,path)){if(id==Save)show.save(path);else{show.load(path);router.cancelLearn();if(engine)engine->unload();refreshFavorites();}message(L"Perfil processado. Formato novo .lphs; INI antigo ainda não é compatível.");}}
  if(id==Import){std::filesystem::path path;if(fileDialog(false,true,path)){instruments.push_back(path);refreshCatalog();}}
  if(id==Catalog&&event==LBN_DBLCLK)favorite();
  if(id==Favorites&&event==LBN_SELCHANGE){auto slot=SendMessageW(favorites,LB_GETCURSEL,0,0);if(slot>=0)activate(static_cast<unsigned>(slot));}
  if(id==Midi&&event==CBN_SELCHANGE){closeMidi();auto index=SendMessageW(midi,CB_GETCURSEL,0,0);if(index>0){auto result=midiInOpen(&midiHandle,static_cast<UINT>(index-1),reinterpret_cast<DWORD_PTR>(window),0,CALLBACK_WINDOW);if(result!=MMSYSERR_NOERROR)throw std::runtime_error("Cannot open MIDI input");if(midiInStart(midiHandle)!=MMSYSERR_NOERROR){closeMidi();throw std::runtime_error("Cannot start MIDI input");}}}
 }catch(const std::exception& e){error(e);}}
 void midiEvent(DWORD packed){try{auto result=router.route(lp::MidiMessage::packed(packed));if(result.action==lp::MidiAction::Favorite)activate(result.index);else if(result.action==lp::MidiAction::Parameter||result.action==lp::MidiAction::Learned)refreshParameters();else if(engine){if(result.action==lp::MidiAction::NoteOn)engine->noteOn(result.index,result.value);if(result.action==lp::MidiAction::NoteOff)engine->noteOff(result.index,result.value);if(result.action==lp::MidiAction::Control)engine->cc(result.index,result.value);}}catch(const std::exception& e){error(e);}}
};
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){auto app=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(msg==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}if(!app)return DefWindowProcW(w,msg,wp,lp);
 switch(msg){case WM_CREATE:try{app->initialize(w);}catch(const std::exception& e){app->error(e);return -1;}return 0;
 case WM_SIZE:if(app->catalog)app->layout();return 0;
 case WM_COMMAND:app->command(LOWORD(wp),HIWORD(wp));return 0;
 case MM_MIM_DATA:app->midiEvent(static_cast<DWORD>(lp));return 0;
 case WM_HSCROLL:for(int i=0;i<14;++i)if(reinterpret_cast<HWND>(lp)==app->sliders[i]&&app->show.current){app->show.parameter(i,static_cast<float>(SendMessageW(app->sliders[i],TBM_GETPOS,0,0))/1000);app->refreshParameters();}return 0;
 case WM_CONTEXTMENU:for(int i=0;i<14;++i)if(reinterpret_cast<HWND>(wp)==app->sliders[i]){try{app->router.learn(i);app->message(L"MIDI Learn: mova um controle CC no teclado. ESC cancela.");}catch(const std::exception& e){app->error(e);}return 0;}break;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:SetTextColor(reinterpret_cast<HDC>(wp),RGB(255,255,255));SetBkColor(reinterpret_cast<HDC>(wp),RGB(12,16,24));return reinterpret_cast<LRESULT>(app->background);
 case WM_ERASEBKGND:{RECT r{};GetClientRect(w,&r);FillRect(reinterpret_cast<HDC>(wp),&r,app->background);return 1;}
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={1000,820};return 0;
 case WM_DESTROY:app->closeMidi();PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};InitCommonControlsEx(&common);App app;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LPbyHSReconstruction";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);auto w=CreateWindowW(cls.lpszClassName,L"Live Performance by Hugo Sodré",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1200,900,nullptr,nullptr,instance,&app);if(!w)return 1;ShowWindow(w,show);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_KEYDOWN){if(msg.wParam==VK_ESCAPE){app.router.cancelLearn();app.effects=false;app.layout();SetFocus(app.catalog);continue;}if(msg.wParam=='E'){app.command(Effects,0);continue;}if(msg.wParam==VK_SPACE&&GetFocus()==app.catalog){app.favorite();continue;}}if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}return static_cast<int>(msg.wParam);}
