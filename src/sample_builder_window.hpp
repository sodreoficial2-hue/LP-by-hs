#pragma once
#include "sample_builder.hpp"
#include <commdlg.h>
#include <future>
#include <memory>
#include <sstream>
namespace lp::sampling {
// Modeless, keyboard-accessible native assistant shared by both frontends.
class BuilderWindow {
 struct Result {Project project;std::filesystem::path saved;std::string error;};
 HWND hwnd{},owner{};HFONT font{};HBRUSH brush=CreateSolidBrush(RGB(12,17,25));
 std::filesystem::path library;Project project;std::future<Result> task;
 std::shared_ptr<std::atomic<bool>> cancel=std::make_shared<std::atomic<bool>>(false);
 std::function<void(const std::filesystem::path&)> imported;
 enum {Files=10,List=11,Root=12,Velocity=13,Apply=14,Loop=15,Name=16,Save=17,Status=18,Cancel=19};
 HWND item(int id){return GetDlgItem(hwnd,id);}
 static std::wstring wide(const std::string& s){return std::filesystem::u8path(s).wstring();}
 void text(int id,const std::wstring& s){SetWindowTextW(item(id),s.c_str());}
 std::wstring text(int id){int size=GetWindowTextLengthW(item(id));std::wstring value(size+1,L'\0');GetWindowTextW(item(id),value.data(),size+1);value.resize(size);return value;}
 bool busy()const{return task.valid();}
 void status(const std::wstring& value){text(Status,value);}
 void enable(){for(int id:{Files,List,Root,Velocity,Apply,Loop,Name,Save})EnableWindow(item(id),!busy());EnableWindow(item(Save),!busy()&&!project.samples.empty());}
 HWND control(const wchar_t* cls,const wchar_t* label,int style,int id){auto h=CreateWindowExW(0,cls,label,WS_CHILD|WS_VISIBLE|style,0,0,0,0,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;}
 void layout(){RECT r{};GetClientRect(hwnd,&r);int width=r.right,height=r.bottom;auto place=[&](int id,int x,int y,int w,int h){MoveWindow(item(id),x,y,w,h,TRUE);};
  place(Files,20,16,width-40,48);place(100,20,72,width-40,54);int bottom=std::max(270,height-290);place(List,20,134,width-40,std::max(90,bottom-144));
  place(101,20,bottom,195,30);place(Root,220,bottom,88,34);place(102,330,bottom,220,30);place(Velocity,555,bottom,88,34);
  place(Loop,20,bottom+43,34,36);place(104,60,bottom+43,350,36);place(Apply,430,bottom+43,std::max(180,width-450),40);
  place(103,20,bottom+96,180,32);place(Name,205,bottom+96,width-225,36);
  place(Save,20,bottom+143,width-240,48);place(Cancel,width-205,bottom+143,185,48);place(Status,20,bottom+204,width-40,76);
 }
 void refresh(){auto selected=SendMessageW(item(List),LB_GETCURSEL,0,0);SendMessageW(item(List),LB_RESETCONTENT,0,0);
  for(const auto& s:project.samples){auto label=wide(s.source)+L" | nota "+(s.root<0?std::wstring(L"? — revisar"):std::to_wstring(s.root))+L" | intensidade "+std::to_wstring(s.velocity)+(s.explicitVelocity?L" (informada)":L" (estimada)")+L" | teclas "+std::to_wstring(s.low)+L"–"+std::to_wstring(s.high)+L" | velocidades "+std::to_wstring(s.velLow)+L"–"+std::to_wstring(s.velHigh);SendMessageW(item(List),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
  SendMessageW(item(List),LB_SETHORIZONTALEXTENT,1900,0);if(!project.samples.empty())SendMessageW(item(List),LB_SETCURSEL,selected>=0&&selected<static_cast<LRESULT>(project.samples.size())?selected:0,0);selection();enable();
 }
 void selection(){auto row=SendMessageW(item(List),LB_GETCURSEL,0,0);if(row<0||static_cast<std::size_t>(row)>=project.samples.size())return;const auto& s=project.samples[row];text(Root,s.root<0?L"":std::to_wstring(s.root));text(Velocity,std::to_wstring(s.velocity));SendMessageW(item(Loop),BM_SETCHECK,s.loop?BST_CHECKED:BST_UNCHECKED,0);}
 int number(int id,int min,int max){try{auto value=text(id);std::size_t used=0;int n=std::stoi(value,&used);if(used!=value.size()||n<min||n>max)throw std::invalid_argument("range");return n;}catch(...){throw std::runtime_error("Use nota MIDI de 0 a 127 e intensidade de 1 a 127.");}}
 void apply(){auto row=SendMessageW(item(List),LB_GETCURSEL,0,0);if(row<0||static_cast<std::size_t>(row)>=project.samples.size())return;
  // Transactional edit: a duplicate velocity must not corrupt the current map.
  auto original=project.samples[row];auto& s=project.samples[row];try{int root=number(Root,0,127),velocity=number(Velocity,1,127);if(root!=s.root){s.root=root;s.cents=0;}s.velocity=velocity;s.explicitVelocity=true;bool loop=SendMessageW(item(Loop),BM_GETCHECK,0,0)==BST_CHECKED;if(loop&&s.loopEnd<=s.loopStart)throw std::runtime_error("Nenhum loop seguro encontrado nesta nota. Mantenha a sustentação desligada.");s.loop=loop;map(project);}catch(...){s=std::move(original);map(project);throw;}
  refresh();status(L"Ajustes aplicados; regiões do teclado e camadas reorganizadas.");
 }
 void files(){std::vector<wchar_t> buffer(65536);OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=hwnd;dialog.lpstrFile=buffer.data();dialog.nMaxFile=static_cast<DWORD>(buffer.size());dialog.lpstrFilter=L"Áudio (WAV, FLAC, OGG, AIFF)\0*.wav;*.flac;*.ogg;*.aif;*.aiff\0Todos os arquivos\0*.*\0\0";dialog.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
  if(!GetOpenFileNameW(&dialog)){if(CommDlgExtendedError())throw std::runtime_error("Falha ao selecionar os áudios; selecione um lote menor.");return;}
  std::vector<std::filesystem::path> paths;std::filesystem::path first=buffer.data();auto next=buffer.data()+wcslen(buffer.data())+1;if(!*next)paths.push_back(first);else while(*next){paths.push_back(first/next);next+=wcslen(next)+1;}
  auto lib=library;auto stop=cancel;stop->store(false);task=std::async(std::launch::async,[paths,lib,stop]{Result result;try{result.project=analyze(paths,lib,[stop]{return stop->load();});}catch(const std::exception& e){result.error=e.what();}return result;});enable();status(L"Analisando notas, silêncios, intensidades e sustentação…");
 }
 void save(){if(project.samples.empty())return;auto name=std::filesystem::path(text(Name)).u8string();if(name.empty()||name.size()>128)throw std::runtime_error("Digite um nome de até 128 caracteres.");
  // Validate on the worker too; the UI merely chooses the destination.
  wchar_t file[32768]=L"Meu timbre.sf2";OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=hwnd;dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"SoundFont SF2\0*.sf2\0\0";dialog.lpstrDefExt=L"sf2";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
  if(!GetSaveFileNameW(&dialog)){if(CommDlgExtendedError())throw std::runtime_error("Falha ao escolher onde salvar o timbre.");return;}
  auto path=std::filesystem::path(file);if(_wcsicmp(path.extension().c_str(),L".sf2")!=0)throw std::runtime_error("Salve com a extensão .sf2. Os arquivos de áudio originais devem ser preservados.");auto stop=cancel;stop->store(false);task=std::async(std::launch::async,[p=std::move(project),path,name,stop]()mutable{Result result;result.project=std::move(p);try{cancelled([stop]{return stop->load();});auto bytes=soundfont(result.project,name);cancelled([stop]{return stop->load();});atomicWrite(path,bytes);result.saved=path;}catch(const std::exception& e){result.error=e.what();}return result;});enable();status(L"Criando SF2 com as notas e camadas de intensidade…");
 }
 void poll(){if(!task.valid()||task.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;auto result=task.get();
  // A failed analysis keeps the last usable project; a failed export returns it.
  if(!result.project.samples.empty())project=std::move(result.project);refresh();
  if(!result.error.empty()){status(wide(result.error));return;}
  if(!result.saved.empty()){try{imported(result.saved);status(L"SF2 criado e adicionado a Meus Sons. Pronto para selecionar e tocar.");}catch(const std::exception& e){status(L"SF2 salvo. Falha ao adicionar a Meus Sons: "+wide(e.what()));}}
  else{bool unknown=false,estimated=false;for(const auto& s:project.samples){unknown|=s.root<0;estimated|=!s.explicitVelocity;}status(unknown?L"Análise concluída. Corrija as notas marcadas com ? antes de criar o SF2.":estimated?L"Análise concluída. Intensidades estimadas; revise se os áudios usaram ganhos diferentes. Você já pode criar o SF2.":L"Análise concluída. Notas e camadas organizadas. Você já pode criar o SF2.");}
 }
 static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){auto self=reinterpret_cast<BuilderWindow*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(m==WM_NCCREATE){self=static_cast<BuilderWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->hwnd=w;SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}if(!self)return DefWindowProcW(w,m,wp,lp);
  try{switch(m){
   case WM_CREATE:self->create();return 0;
   case WM_SIZE:self->layout();return 0;
   case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={760,660};return 0;}
   case WM_TIMER:self->poll();return 0;
   case WM_COMMAND:if(!self->busy()){if(LOWORD(wp)==Files)self->files();else if(LOWORD(wp)==List&&HIWORD(wp)==LBN_SELCHANGE)self->selection();else if(LOWORD(wp)==Apply)self->apply();else if(LOWORD(wp)==Save)self->save();}if(LOWORD(wp)==Cancel||LOWORD(wp)==IDCANCEL)SendMessageW(w,WM_CLOSE,0,0);return 0;
   case WM_CLOSE:self->cancel->store(true);if(self->busy()){self->status(L"Cancelando a operação. Aguarde a conclusão antes de fechar.");return 0;}DestroyWindow(w);return 0;
   case WM_DESTROY:KillTimer(w,1);self->hwnd=nullptr;return 0;
   case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,RGB(255,255,255));SetBkColor(dc,RGB(12,17,25));return reinterpret_cast<LRESULT>(self->brush);}
   case WM_ERASEBKGND:{RECT r{};GetClientRect(w,&r);FillRect(reinterpret_cast<HDC>(wp),&r,self->brush);return 1;}
  }}catch(const std::exception& e){self->enable();self->status(wide(e.what()));}return DefWindowProcW(w,m,wp,lp);
 }
 void create(){font=CreateFontW(-22,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  control(L"BUTTON",L"1. Selecionar áudios e analisar automaticamente",BS_PUSHBUTTON|WS_TABSTOP,Files);
  control(L"STATIC",L"Use notas isoladas ou separadas por silêncio. Nomes como C4_v40 e C4_v110 informam nota e intensidade (C4 = MIDI 60).",0,100);
  control(L"LISTBOX",L"Amostras, notas e camadas",LBS_NOTIFY|WS_VSCROLL|WS_HSCROLL|WS_BORDER|WS_TABSTOP,List);
  control(L"STATIC",L"Nota MIDI (0–127)",0,101);control(L"EDIT",L"",ES_NUMBER|WS_BORDER|WS_TABSTOP,Root);
  control(L"STATIC",L"Intensidade (1–127)",0,102);control(L"EDIT",L"",ES_NUMBER|WS_BORDER|WS_TABSTOP,Velocity);
  control(L"BUTTON",L"Sustentar com loop detectado",BS_AUTOCHECKBOX|WS_TABSTOP,Loop);control(L"STATIC",L"Sustentar com loop detectado",0,104);control(L"BUTTON",L"Aplicar edição da amostra",BS_PUSHBUTTON|WS_TABSTOP,Apply);
  control(L"STATIC",L"Nome do timbre",0,103);control(L"EDIT",L"Meu timbre",ES_AUTOHSCROLL|WS_BORDER|WS_TABSTOP,Name);
  SendMessageW(item(Name),EM_SETLIMITTEXT,128,0);
  control(L"BUTTON",L"2. Criar SF2 e adicionar a Meus Sons",BS_PUSHBUTTON|WS_TABSTOP,Save);control(L"BUTTON",L"Fechar / cancelar",BS_PUSHBUTTON|WS_TABSTOP,Cancel);
  control(L"STATIC",L"Selecione uma ou várias gravações do mesmo instrumento.",0,Status);layout();refresh();SetTimer(hwnd,1,100,nullptr);
 }
 public:
 BuilderWindow(HWND parent,std::filesystem::path decoder,std::function<void(const std::filesystem::path&)> callback):owner(parent),library(std::move(decoder)),imported(std::move(callback)){}
 ~BuilderWindow(){cancel->store(true);if(task.valid())task.wait();if(hwnd)DestroyWindow(hwnd);if(font)DeleteObject(font);DeleteObject(brush);}
 void show(){if(hwnd){ShowWindow(hwnd,SW_RESTORE);SetForegroundWindow(hwnd);return;}if(font){DeleteObject(font);font=nullptr;}WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"LPAutomaticSampler";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&cls);
  auto w=CreateWindowExW(WS_EX_CONTROLPARENT,cls.lpszClassName,L"Criar timbre de áudio — Live Performance",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,980,790,owner,nullptr,cls.hInstance,this);if(!w)throw std::runtime_error("Não foi possível abrir o criador de timbres.");ShowWindow(w,SW_SHOW);SetFocus(item(Files));}
 bool dialogMessage(MSG* msg){return hwnd&&IsDialogMessageW(hwnd,msg);}
 bool ownsMessage(const MSG* msg)const{return hwnd&&(msg->hwnd==hwnd||IsChild(hwnd,msg->hwnd));}
};
}
