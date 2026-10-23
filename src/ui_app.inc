 lp::Screen screen=lp::Screen::Home;
 lp::ParameterPanel parameterPanel;
 HWND page{};lp::PageScroll pageScroll;bool placing=false;int wheelRemainder=0;
 static constexpr int pageOrigin=180;
 HWND item(int id) const {auto child=GetDlgItem(window,id);return child?child:GetDlgItem(page,id);}
 int pageTop() const {RECT r{};GetClientRect(window,&r);return r.right<1280?260:180;}
 int pageMinimum() const {
  if(effects)return performanceControls()?300:740;
  switch(screen){case lp::Screen::Home:return 480;case lp::Screen::Instruments:return 120;case lp::Screen::AudioMidi:return 520;case lp::Screen::Settings:return 410;case lp::Screen::PerformanceEditor:return 580;case lp::Screen::About:return 140;}
  return 480;
 }
 void scrollPage(int delta){touchParameters();if(pageScroll.move(delta))layout();}
 void revealFocus(HWND child){
  if(loading||placing||GetParent(child)!=page||!IsWindowVisible(child)||GetFocus()!=child)return;
  RECT r{};GetWindowRect(child,&r);MapWindowPoints(nullptr,page,reinterpret_cast<POINT*>(&r),2);
  if(pageScroll.reveal(r.top+pageScroll.offset-8,r.bottom+pageScroll.offset+8))layout();
 }
 POINT pagePoint(LPARAM point) const {return {static_cast<short>(LOWORD(point)),static_cast<short>(HIWORD(point))+pageOrigin+pageScroll.offset};}
 bool wheelCommand(MSG& msg){
  if(loading||msg.message!=WM_MOUSEWHEEL)return false;
  wchar_t kind[32]{};GetClassNameW(msg.hwnd,kind,32);
  if(_wcsicmp(kind,L"LISTBOX")==0||_wcsicmp(kind,L"COMBOBOX")==0||_wcsicmp(kind,L"EDIT")==0||_wcsicmp(kind,TRACKBAR_CLASSW)==0)return false;
  if(!effects&&screen==lp::Screen::About){SendMessageW(item(AboutText),WM_MOUSEWHEEL,msg.wParam,msg.lParam);return true;}
  wheelRemainder+=GET_WHEEL_DELTA_WPARAM(msg.wParam);
  int notches=wheelRemainder/WHEEL_DELTA;wheelRemainder-=notches*WHEEL_DELTA;if(notches)scrollPage(-notches*144);return true;
 }
 HFONT titleFont{},signatureFont{},largeFont{},smallFont{};
 std::unique_ptr<Gdiplus::Image> photo;
 bool loading=true;int progress=0,categoryIndex=0,virtualNote=-1,learningVolume=-1;bool learningMaster=false;
 ULONGLONG loadingStart=0;
 RECT keyboardRect{};
 std::array<RECT,128> virtualKeys{};std::array<bool,128> keyDown{};
 bool performanceControls() const{return editingPerformanceValid&&engine&&engine->performanceActive();}
 void touchParameters(){if(effects)parameterPanel.touch(GetTickCount64());}
 std::wstring categoryName() const {return categoryIndex==15?L"Favoritos":std::filesystem::u8path(lp::instrumentCategories[categoryIndex]).wstring();}
 void syncPerformance(){if(activePerformanceIndex>=0&&editingPerformanceValid&&static_cast<std::size_t>(activePerformanceIndex)<show.performanceLibrary.size())show.performanceLibrary[activePerformanceIndex]=lp::savePerformance(editingPerformance);}
 void chooseCategory(int delta){categoryIndex=(categoryIndex+delta+16)%16;SendMessageW(category,CB_SETCURSEL,categoryIndex,0);refreshCatalog();layout();}
 void goHome(){pageScroll.seek(0);screen=lp::Screen::Home;performanceVisible=false;cancelMidiLearn();layout();SetFocus(item(CategoryTile));}
 void openCategory(){pageScroll.seek(0);screen=lp::Screen::Instruments;performanceVisible=false;refreshCatalog();layout();
  if(categoryIndex==15){if(SendMessageW(favorites,LB_GETCURSEL,0,0)<0)SendMessageW(favorites,LB_SETCURSEL,0,0);SetFocus(favorites);auto slot=SendMessageW(favorites,LB_GETCURSEL,0,0);if(slot>=0)activate(static_cast<unsigned>(slot));}
  else{SetFocus(catalog);audition();}
 }
 void toggleParameters(){
  if(loading)return;
  cancelMidiLearn();parameterPanel.toggle(GetTickCount64());effects=parameterPanel.open;
  if(effects){pageScroll.seek(0);performanceVisible=false;refreshParameters();layout();SetFocus(sliders[0]);}
  else goHome();
 }
 void openScreen(lp::Screen next){if(loading)return;pageScroll.seek(0);effects=false;parameterPanel.open=false;screen=next;performanceVisible=next==lp::Screen::PerformanceEditor;cancelMidiLearn();layout();SetFocus(window);}
 void updateSplash(int done,const std::wstring& label){
  progress=std::clamp(done,0,100);SetWindowTextW(status,label.c_str());layout();UpdateWindow(window);
 }
 void layout(){
  if(!catalog||placing)return;
  placing=true;
  RECT r{};GetClientRect(window,&r);int windowWidth=r.right,windowHeight=r.bottom;
  std::vector<HWND> visible;
  auto finish=[&]{
   for(auto parent:{window,page})for(HWND child=GetWindow(parent,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
    if(child==page)continue;
    bool show=std::find(visible.begin(),visible.end(),child)!=visible.end();
    if(show!=static_cast<bool>(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE))ShowWindow(child,show?SW_SHOW:SW_HIDE);
   }
   ShowWindow(page,loading?SW_HIDE:SW_SHOW);placing=false;
   InvalidateRect(window,nullptr,FALSE);InvalidateRect(page,nullptr,FALSE);
  };
  auto place=[&](int id,int x,int y,int ww,int hh){auto h=item(id);if(GetParent(h)==page)y-=pageOrigin+pageScroll.offset;MoveWindow(h,x,y,std::max(1,ww),std::max(1,hh),TRUE);visible.push_back(h);};
  if(loading){place(2,windowWidth/2,windowHeight-144,windowWidth/2-48,66);finish();return;}
  int topOfPage=pageTop();pageScroll.resize(windowHeight-topOfPage-92,pageMinimum());
  MoveWindow(page,0,topOfPage,windowWidth,pageScroll.viewport,TRUE);
  SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;info.nMin=0;info.nMax=pageScroll.content-1;info.nPage=pageScroll.viewport;info.nPos=pageScroll.offset;SetScrollInfo(page,SB_VERT,&info,TRUE);
  int navX=windowWidth-708,navY=windowWidth<1280?138:34;
  if(!effects){place(PerformanceButton,navX,navY,210,60);place(AudioMidiButton,navX+218,navY,166,60);place(SettingsButton,navX+392,navY,190,60);place(AboutButton,navX+590,navY,102,60);}
  place(2,28,windowHeight-72,windowWidth-56,60);
  int width=windowWidth-20,height=pageOrigin+pageScroll.content+92,top=148,bottom=height-92;
  if(effects){
   int count=performanceControls()?4:16,rows=performanceControls()?2:8;
   int cell=(bottom-192-12)/rows,half=(width-84)/2;
   for(int i=0;i<count;++i){int col=i/rows,row=i%rows,x=28+col*(half+28),y=192+row*cell;
    place(Knob+100+i,x,y,half,30);place(Knob+i,x,y+32,half,std::max(30,cell-37));}
  }else if(screen==lp::Screen::Home){
   int tile=std::max(200,std::min(width-360,height-242-(top+48)-90)),x=(width-tile)/2;
   place(CategoryTile,x,top+48,tile,tile+70);place(PreviousCategory,x-132,top+tile/2,96,96);place(NextCategory,x+tile+36,top+tile/2,96,96);
   keyboardRect={28,height-242,width-28,height-100};
  }else if(screen==lp::Screen::Instruments){
   int half=(width-84)/2;place(InstrumentArt,28,top+34,half,bottom-top-46);place(categoryIndex==15?Favorites:Catalog,56+half,top+34,half,bottom-top-46);
  }else if(screen==lp::Screen::AudioMidi){
   place(AudioDriver,40,top+68,width-80,300);place(AudioBuffer,40,top+172,width-80,300);place(Midi,40,top+276,width-80,300);
   place(AudioStart,40,top+354,260,64);place(AudioStop,320,top+354,220,64);place(AudioPanel,560,top+354,240,64);place(Panic,40,top+442,260,64);
  }else if(screen==lp::Screen::Settings){
   int half=(width-120)/2;place(Open,40,top+90,half,90);place(Save,80+half,top+90,half,90);place(Import,40,top+208,half,90);place(ImportSf2,80+half,top+208,half,90);place(Panic,40,top+326,half,76);
  }else if(screen==lp::Screen::PerformanceEditor){
   place(PerformanceModeControl,36,top+38,width-72,290);
   for(int i=0;i<4;++i){place(PerformancePartBase+i,36,top+112+i*70,width-190,290);place(PerformanceClearBase+i,width-132,top+112+i*70,96,54);}
   place(PerformanceName,36,top+414,width-72,56);place(PerformancePlay,36,top+494,(width-100)/2,68);place(PerformanceSave,64+(width-100)/2,top+494,(width-100)/2,68);
  }else if(screen==lp::Screen::About){place(AboutText,width/2,top+32,width/2-40,bottom-top-54);}
  finish();
 }
 void drawButton(DRAWITEMSTRUCT* item){
  HDC dc=item->hDC;RECT r=item->rcItem;bool selected=item->itemState&ODS_SELECTED;
  lpui::fill(dc,r,selected?RGB(49,61,78):lpui::panel);
  if(item->CtlID==CategoryTile||item->CtlID==InstrumentArt){
   RECT art{r.left+36,r.top+24,r.right-36,r.bottom-82};lpui::instrument(dc,art,categoryIndex);
   RECT label{r.left+12,r.bottom-78,r.right-12,r.bottom-14};lpui::text(dc,categoryName(),label,largeFont);
  }else{wchar_t label[128]{};GetWindowTextW(item->hwndItem,label,128);auto previousFont=SelectObject(dc,font);RECT measure=r;DrawTextW(dc,label,-1,&measure,DT_CALCRECT|DT_CENTER|DT_WORDBREAK);SelectObject(dc,previousFont);RECT target=r;target.top+=(r.bottom-r.top-(measure.bottom-measure.top))/2;lpui::text(dc,label,target,font,lpui::white,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);}
  if((item->itemState&ODS_FOCUS)||selected)lpui::box(dc,{r.left+3,r.top+3,r.right-3,r.bottom-3},lpui::gold,4);
  else lpui::box(dc,{r.left+1,r.top+1,r.right-1,r.bottom-1},RGB(89,106,128),1);
 }
 void drawList(DRAWITEMSTRUCT* item){
  if(item->itemID==static_cast<UINT>(-1))return;
  auto h=item->hwndItem;auto length=SendMessageW(h,LB_GETTEXTLEN,item->itemID,0);if(length<0)return;
  std::wstring name(static_cast<std::size_t>(length)+1,L'\0');SendMessageW(h,LB_GETTEXT,item->itemID,reinterpret_cast<LPARAM>(name.data()));
  RECT r=item->rcItem;lpui::fill(item->hDC,r,(item->itemState&ODS_SELECTED)?RGB(56,73,98):lpui::panel);
  RECT label{r.left+14,r.top+6,r.right-62,r.bottom-6};lpui::text(item->hDC,name,label,font,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
  bool star=false;
  if(h==favorites)star=show.favorites[item->itemID].has_value();
  else if(categoryIndex==13)star=isFavorite("performance://"+std::to_string(visiblePerformances[item->itemID]));
  else if(item->itemID<visibleInstruments.size())star=isFavorite(lp::instrumentReference(instruments[visibleInstruments[item->itemID]]));
  RECT mark{r.right-56,r.top,r.right-6,r.bottom};lpui::text(item->hDC,star?L"★":L"☆",mark,largeFont,lpui::gold);
  if(item->itemState&ODS_FOCUS)lpui::box(item->hDC,{r.left+2,r.top+2,r.right-2,r.bottom-2},lpui::gold,3);
 }
 bool isFavorite(const std::string& reference) const {for(const auto& item:show.favorites)if(item&&item->sfz==reference)return true;return false;}
 void paint(HDC dc,bool bodyOnly=false){
  RECT r{};GetClientRect(window,&r);int width=r.right,height=r.bottom;lpui::fill(dc,r,lpui::ink);
  if(loading){
   lpui::photo(dc,photo.get(),{32,32,width/2-28,height-32});
   lpui::text(dc,L"Live Performance",{width/2,140,width-30,218},titleFont);
   lpui::text(dc,L"by Hugo Sodré",{width/2,224,width-30,306},signatureFont,lpui::gold);
   lpui::text(dc,L"Carregando sua biblioteca",{width/2,350,width-40,400},font,lpui::muted);
   RECT bar{width/2+32,430,width-60,458};lpui::fill(dc,bar,lpui::panel);RECT fill=bar;fill.right=bar.left+(bar.right-bar.left)*progress/100;lpui::fill(dc,fill,lpui::gold);
   auto elapsed=(GetTickCount64()-loadingStart)/1000;
   std::wstring value=std::to_wstring(progress)+L"%";
   if(progress>0&&progress<100&&elapsed>=1)value+=L" · cerca de "+std::to_wstring(elapsed*(100-progress)/progress)+L" s restantes";
   lpui::text(dc,value,{width/2,480,width-40,526},font);
   lpui::text(dc,L"Versão 1.0",{width/2,height-66,width-40,height-28},font,lpui::gold);return;
  }
  if(!bodyOnly){
  lpui::photo(dc,photo.get(),{26,24,110,108},true);
  int brandRight=width<1280?width-28:width-728;
  lpui::text(dc,L"Live Performance",{128,20,brandRight,62},largeFont,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
  lpui::text(dc,L"by Hugo Sodré",{128,62,brandRight,110},signatureFont,lpui::gold,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
  lpui::line(dc,28,pageTop()-52,width-28,pageTop()-52,RGB(78,90,108),1);
  std::wstring heading;
  if(effects)heading=performanceControls()?L"Volumes da Performance · E para fechar":L"Parâmetros · E para fechar";
  else if(screen==lp::Screen::Home)heading=L"Escolha a categoria";
  else if(screen==lp::Screen::Instruments)heading=categoryName()+L" · ↑ ↓ para escolher · Espaço para favoritar";
  else if(screen==lp::Screen::PerformanceEditor)heading=L"Performance Editor";
  else if(screen==lp::Screen::AudioMidi)heading=L"Áudio / MIDI";
  else if(screen==lp::Screen::Settings)heading=L"Configurações";
  else heading=L"Sobre · Versão 1.0";
  lpui::text(dc,heading,{28,pageTop()-40,width-28,pageTop()-4},font,lpui::gold);
  return;
  }
  width-=20;height=pageOrigin+pageScroll.content+92;
  if(!effects&&screen==lp::Screen::Home)drawKeyboard(dc);
  if(!effects&&screen==lp::Screen::AudioMidi){
   lpui::text(dc,L"Driver ASIO",{40,184,width-40,212},font,lpui::muted,DT_LEFT);
   lpui::text(dc,L"Tamanho do buffer",{40,288,width-40,316},font,lpui::muted,DT_LEFT);
   lpui::text(dc,L"Entrada MIDI",{40,392,width-40,420},font,lpui::muted,DT_LEFT);
   auto rate=engine&&engine->running()?L"Sample rate automático: "+std::to_wstring(static_cast<int>(engine->sampleRate()))+L" Hz":L"Sample rate automático ao conectar o ASIO";
   lpui::text(dc,rate,{340,590,width-40,646},font,lpui::gold);
  }
  if(!effects&&screen==lp::Screen::About)lpui::photo(dc,photo.get(),{36,194,width/2-36,height-110});
 }
 void drawKeyboard(HDC dc){
  virtualKeys={};int whites=0;for(int note=36;note<=96;++note){int pc=note%12;if(pc!=1&&pc!=3&&pc!=6&&pc!=8&&pc!=10)++whites;}
  int index=0,w=keyboardRect.right-keyboardRect.left;
  for(int note=36;note<=96;++note){int pc=note%12;if(pc==1||pc==3||pc==6||pc==8||pc==10)continue;
   RECT key{keyboardRect.left+index*w/whites,keyboardRect.top,keyboardRect.left+(index+1)*w/whites,keyboardRect.bottom};virtualKeys[note]=key;
   lpui::fill(dc,key,keyDown[note]?lpui::gold:lpui::white);lpui::box(dc,key,lpui::ink,1);
   if(pc==0)lpui::text(dc,L"Dó"+std::to_wstring(note/12-1),{key.left,key.bottom-28,key.right,key.bottom},smallFont,lpui::ink);
   ++index;
  }
  index=0;
  for(int note=36;note<=96;++note){int pc=note%12;bool black=pc==1||pc==3||pc==6||pc==8||pc==10;if(!black){++index;continue;}
   int center=keyboardRect.left+index*w/whites;int half=w/whites/3;RECT key{center-half,keyboardRect.top,center+half,keyboardRect.top+(keyboardRect.bottom-keyboardRect.top)*3/5};virtualKeys[note]=key;lpui::fill(dc,key,keyDown[note]?lpui::gold:lpui::ink);lpui::box(dc,key,RGB(87,102,122),1);
  }
 }
 int keyAt(POINT point){for(int pass=0;pass<2;++pass)for(int note=36;note<=96;++note){int pc=note%12;bool black=pc==1||pc==3||pc==6||pc==8||pc==10;if(black!=(pass==0))continue;if(PtInRect(&virtualKeys[note],point))return note;}return -1;}
 void releaseVirtual(){if(virtualNote>=0&&engine)engine->send({0x80,static_cast<std::uint8_t>(virtualNote),0});if(virtualNote>=0)keyDown[virtualNote]=false;virtualNote=-1;InvalidateRect(page,nullptr,FALSE);}
 void virtualKey(POINT point){int note=keyAt(point);if(note==virtualNote)return;releaseVirtual();if(note>=0&&engine&&engine->loaded()){virtualNote=note;keyDown[note]=true;engine->send({0x90,static_cast<std::uint8_t>(note),100});InvalidateRect(page,nullptr,FALSE);}}
 void setPanelValue(int index,float value){
  touchParameters();
  if(performanceControls()){
   if(index>=4||!editingPerformance.parts[index].enabled)return;
   auto& part=editingPerformance.parts[index];part.patch.parameters[0]=value;engine->setPerformancePartParameters(index,part.patch.parameters,part.patch.adsrEnabled);syncPerformance();
  }else if(index==15){show.master=value;if(engine)engine->masterVolume(value);}
  else if(show.current){show.parameter(index,value);applyParameters();}
  else if(previewActive){previewPatch.parameters[index]=value;if(index>=1&&index<=4)previewPatch.adsrEnabled=true;applyParameters();}
  refreshParameters();
 }
 bool keyCommand(MSG& msg){
  if(msg.message!=WM_KEYDOWN||loading)return false;
  if(effects)touchParameters();
  auto key=msg.wParam;wchar_t kind[32]{};GetClassNameW(GetFocus(),kind,32);bool textControl=_wcsicmp(kind,L"EDIT")==0;bool typing=textControl&&!(GetWindowLongPtrW(GetFocus(),GWL_STYLE)&ES_READONLY);
  if(key==VK_F12){command(Panic,0);return true;}
  if(key==VK_ESCAPE){if(effects){cancelMidiLearn();return true;}goHome();return true;}
  if(!typing&&!(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)){
   if(key=='E'){toggleParameters();return true;}
   if(!effects){
   if(key=='P'){command(PerformanceButton,0);return true;}
   if(key=='C'){openScreen(lp::Screen::Settings);return true;}
   if(key=='A'){openScreen(lp::Screen::AudioMidi);return true;}
   }
  }
  if(key==VK_UP||key==VK_DOWN||key==VK_PRIOR||key==VK_NEXT){
   bool native=_wcsicmp(kind,L"LISTBOX")==0||_wcsicmp(kind,L"COMBOBOX")==0||_wcsicmp(kind,TRACKBAR_CLASSW)==0||(textControl&&(GetWindowLongPtrW(GetFocus(),GWL_STYLE)&ES_MULTILINE));
   if(native)return false;
   if(!effects&&screen==lp::Screen::About){auto text=item(AboutText);SetFocus(text);SendMessageW(text,WM_KEYDOWN,key,msg.lParam);return true;}
   if(!effects&&screen==lp::Screen::Instruments){auto list=categoryIndex==15?favorites:catalog;SetFocus(list);SendMessageW(list,WM_KEYDOWN,key,msg.lParam);return true;}
   int direction=key==VK_UP||key==VK_PRIOR?-1:1;scrollPage(direction*((key==VK_PRIOR||key==VK_NEXT)?std::max(48,pageScroll.viewport-48):48));return true;
  }
  if(!effects&&screen==lp::Screen::Home){if(key==VK_LEFT||key==VK_RIGHT){chooseCategory(key==VK_LEFT?-1:1);return true;}if(key==VK_RETURN){openCategory();return true;}}
  if(!effects&&screen==lp::Screen::Instruments){
   if(key==VK_SPACE){favorite();return true;}
   if(key==VK_RETURN){if(categoryIndex==15){auto slot=SendMessageW(favorites,LB_GETCURSEL,0,0);if(slot>=0)activate(static_cast<unsigned>(slot));}else audition();return true;}
  }
  if(key==VK_DELETE&&performanceVisible){for(int i=0;i<4;++i)if(GetFocus()==performancePartControls[i]){clearPerformancePart(i);return true;}}
  return false;
 }
