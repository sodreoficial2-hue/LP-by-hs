 lp::Screen screen=lp::Screen::Home;
 lp::ParameterPanel parameterPanel;
 HWND page{};lp::PageScroll pageScroll;bool placing=false;int wheelRemainder=0;
 static constexpr int pageOrigin=156;
 HWND item(int id) const {auto child=GetDlgItem(window,id);return child?child:GetDlgItem(page,id);}
 int pageTop() const {return 156;}
 int pageMinimum() const {
  if(effects)return performanceControls()?360:864;
  switch(screen){case lp::Screen::Home:return 316;case lp::Screen::Keyboard:return 360;case lp::Screen::Instruments:return 400;case lp::Screen::AudioMidi:return 580;case lp::Screen::Settings:return 760;case lp::Screen::PerformanceEditor:return 1600;case lp::Screen::About:return 600;}
  return 580;
 }
 void scrollPage(int delta){if(pageScroll.move(delta))layout();}
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
 HFONT titleFont{},signatureFont{},largeFont{},smallFont{},statusFont{};
 std::unique_ptr<Gdiplus::Image> photo,categoryPhotos;
 bool loading=true;int progress=0,categoryIndex=0,virtualNote=-1,learningVolume=-1;bool learningMaster=false;
 ULONGLONG loadingStart=0;
 RECT keyboardRect{};
 std::array<RECT,128> virtualKeys{};std::array<bool,128> keyDown{};
 bool performanceActive() const{return editingPerformanceValid&&engine&&engine->performanceActive();}
 bool editingPerformanceControls() const{return screen==lp::Screen::PerformanceEditor&&performanceActive();}
 bool performanceControls() const{return screen!=lp::Screen::PerformanceEditor&&performanceActive();}
 std::wstring categoryName() const {return categoryIndex==15?L"Favoritos":std::filesystem::u8path(lp::instrumentCategories[categoryIndex]).wstring();}
 void syncPerformance(){
  if(performanceActive()&&show.current&&show.favorites[*show.current]&&show.favorites[*show.current]->sfz=="performance://"+std::to_string(activePerformanceIndex))
   show.favorites[*show.current]->performanceMix=lp::performanceMix(editingPerformance);
 }
 void goHome(){if(!preserveEditorDraft())return;restoringSounds=false;pageScroll.seek(0);screen=lp::Screen::Home;performanceVisible=false;cancelMidiLearn();layout();SetFocus(item(CategoryCardBase+categoryIndex));}
 void openCategory(){if(!preserveEditorDraft())return;restoringSounds=false;catalogSearch.clear();SetWindowTextW(item(Search),L"");pageScroll.seek(0);screen=lp::Screen::Instruments;performanceVisible=false;refreshCatalog();layout();
  // Browsing categories never loads a sample or interrupts the currently playing sound.
  if(categoryIndex==15){if(SendMessageW(favorites,LB_GETCURSEL,0,0)<0)SendMessageW(favorites,LB_SETCURSEL,0,0);SetFocus(favorites);}
  else SetFocus(catalog);
 }
 void toggleParameters(){
  if(loading)return;
  if(!effects&&screen==lp::Screen::PerformanceEditor){try{activateEditorPerformance();}catch(const std::exception& e){error(e);return;}}
  cancelMidiLearn();parameterPanel.toggle();effects=parameterPanel.open;
  if(effects){pageScroll.seek(0);performanceVisible=false;refreshParameters();layout();SetFocus(sliders[0]);}
  else if(screen==lp::Screen::PerformanceEditor){performanceVisible=true;pageScroll.seek(0);layout();SetFocus(performancePartControls[editingPart]);}
  else goHome();
 }
 void openScreen(lp::Screen next){if(loading)return;if(next!=lp::Screen::PerformanceEditor)if(!preserveEditorDraft())return;restoringSounds=false;pageScroll.seek(0);effects=false;parameterPanel.open=false;screen=next;performanceVisible=next==lp::Screen::PerformanceEditor;cancelMidiLearn();layout();SetFocus(window);}
 void updateSplash(int done,const std::wstring& label){
  progress=std::clamp(done,0,100);SetWindowTextW(status,label.c_str());layout();UpdateWindow(window);markVisionDirty();flushVision();
 }
 void layout(){
  if(!catalog||placing)return;
  placing=true;
  RECT r{};GetClientRect(window,&r);int windowWidth=r.right,windowHeight=r.bottom;
  std::vector<HWND> visible;
  auto finish=[&]{
   for(auto parent:{window,page})for(HWND child=GetWindow(parent,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
    if(child==page||child==visionContainer())continue;
    bool show=!visionVisible()&&std::find(visible.begin(),visible.end(),child)!=visible.end();
    if(show!=static_cast<bool>(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE))ShowWindow(child,show?SW_SHOW:SW_HIDE);
   }
   ShowWindow(page,(loading||visionVisible())?SW_HIDE:SW_SHOW);placing=false;
   markVisionDirty();
#ifdef LP_VISION_UI
   if(vision)vision->resize();
#endif
   InvalidateRect(window,nullptr,FALSE);InvalidateRect(page,nullptr,FALSE);
  };
  auto place=[&](int id,int x,int y,int ww,int hh){auto h=item(id);if(GetParent(h)==page)y-=pageOrigin+pageScroll.offset;MoveWindow(h,x,y,std::max(1,ww),std::max(1,hh),TRUE);visible.push_back(h);};
  if(loading){int y=std::max(56,windowHeight/2-190);place(2,windowWidth*48/100,y+332,windowWidth*52/100-48,72);finish();return;}
  int topOfPage=pageTop();pageScroll.resize(windowHeight-topOfPage-116,pageMinimum());
  MoveWindow(page,0,topOfPage,windowWidth,pageScroll.viewport,TRUE);
  SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;info.nMin=0;info.nMax=pageScroll.content-1;info.nPage=pageScroll.viewport;info.nPos=pageScroll.offset;SetScrollInfo(page,SB_VERT,&info,TRUE);
  int navX=windowWidth-660;
  place(PerformanceButton,navX,28,174,52);place(AudioMidiButton,navX+182,28,158,52);place(SettingsButton,navX+348,28,182,52);place(AboutButton,navX+538,28,106,52);
  place(!effects&&screen==lp::Screen::Home?KeyboardButton:HomeButton,windowWidth-214,108,182,40);
  place(2,54,windowHeight-60,windowWidth-86,48);
  place(ProfileState,54,windowHeight-102,(windowWidth-110)/2,36);place(OutputMeter,windowWidth/2,windowHeight-102,windowWidth/2-36,36);
  int width=windowWidth-20,p=pageOrigin;
  if(effects){
   int count=performanceControls()?4:16,rows=performanceControls()?2:8,half=(width-84)/2;
   for(int i=0;i<count;++i){int x=28+(i/rows)*(half+28),y=p+56+(i%rows)*96;
    place(Knob+100+i,x+18,y+12,half-36,32);place(Knob+i,x+18,y+48,half-36,34);}
  }else if(screen==lp::Screen::Home){
   lp::CategoryGrid grid(width,pageScroll.viewport);
   for(int i=0;i<16;++i){auto cell=grid.card(i);place(CategoryCardBase+i,cell[0],p+cell[1],cell[2],cell[3]);}
   keyboardRect={};virtualKeys={};
  }else if(screen==lp::Screen::Keyboard){
   keyboardRect={56,p+130,width-56,p+std::max(300,pageScroll.viewport-54)};
  }else if(screen==lp::Screen::Instruments){
   int left=width*2/5;place(InstrumentArt,32,p+20,left-48,pageScroll.content-42);if(categoryIndex!=15||restoringSounds)place(Search,left+8,p+12,width-left-40,44);place(categoryIndex==15&&!restoringSounds?Favorites:Catalog,left+8,p+68,width-left-40,pageScroll.content-(restoringSounds?160:90));
   if(restoringSounds){place(RestoreSelected,left+8,p+pageScroll.content-76,(width-left-56)/2,60);place(RestoreAll,left+24+(width-left-56)/2,p+pageScroll.content-76,(width-left-56)/2,60);}
  }else if(screen==lp::Screen::AudioMidi){
   place(AudioDriver,56,p+152,width-112,300);place(Midi,56,p+262,width-112,300);
   int actionWidth=(width-144)/3;place(AudioStart,56,p+340,actionWidth,60);place(AudioStop,72+actionWidth,p+340,actionWidth,60);place(AudioPanel,88+2*actionWidth,p+340,actionWidth,60);place(Panic,width-300,p+460,244,60);
  }else if(screen==lp::Screen::Settings){
   int half=(width-84)/2;
   place(Open,32,p+28,half,160);place(Save,52+half,p+28,half,160);place(Import,32,p+208,half,160);place(ImportSf2,52+half,p+208,half,160);place(RestoreSounds,32,p+388,width-64,68);place(LocateSounds,32,p+472,width-64,68);place(Panic,width-300,p+578,244,60);
  }else if(screen==lp::Screen::PerformanceEditor){
   place(PartSearch,56,p+10,width-112,44);p+=64;
   place(PerformanceModeControl,56,p+68,width-112,300);
   for(int i=0;i<4;++i){int y=p+152+i*300;place(PerformancePartBase+i,116,y+22,width-308,300);place(PerformanceClearBase+i,width-172,y+18,112,56);int half=(width-144)/2;
    place(PartVolumeBase+i,56,y+76,width-112,36);place(PartZoneBase+i,56,y+112,width-112,28);place(PartSoloBase+i,56,y+140,half,50);place(PartMuteBase+i,80+half,y+140,half,50);
    place(PartTargetBase+i,56,y+198,220,300);place(PartCopyBase+i,292,y+198,180,50);place(PartSwapBase+i,488,y+198,180,50);
    place(PartLowBase+i,56,y+252,260,44);place(PartHighBase+i,332,y+252,260,44);
   }
   place(PerformanceName,56,p+1390,width-112,50);place(PerformanceSave,56,p+1450,(width-128)/2,60);place(PerformanceSaveNew,72+(width-128)/2,p+1450,(width-128)/2,60);
  }else if(screen==lp::Screen::About){int left=width/3;place(AboutText,left+44,p+68,width-left-100,pageScroll.content-108);}
  finish();
 }
 #include "ui_studio.inc"
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

  if(performanceControls()){
   if(index>=4||!editingPerformance.parts[index].enabled)return;
   auto& part=editingPerformance.parts[index];part.patch.parameters[0]=value;engine->setPerformancePartParameters(index,part.patch.parameters,part.patch.adsrEnabled);syncPerformance();
  }else if(index==15){show.master=value;if(engine)engine->masterVolume(value);}
  else if(editingPerformanceControls()){
   auto& part=editingPerformance.parts[editingPart];if(!part.enabled)return;
   part.patch.parameters[index]=value;if(index>=1&&index<=4)part.patch.adsrEnabled=true;applyParameters();
  }
  else if(show.current){show.parameter(index,value);applyParameters();}
  else if(previewActive){previewPatch.parameters[index]=value;if(index>=1&&index<=4)previewPatch.adsrEnabled=true;applyParameters();}
  if(screen==lp::Screen::PerformanceEditor)touchEditor();refreshParameters();
 }
 bool keyCommand(MSG& msg){
  if(msg.message!=WM_KEYDOWN||loading)return false;
  auto key=msg.wParam;wchar_t kind[32]{};GetClassNameW(GetFocus(),kind,32);bool textControl=_wcsicmp(kind,L"EDIT")==0;bool typing=textControl&&!(GetWindowLongPtrW(GetFocus(),GWL_STYLE)&ES_READONLY);
  if(key=='D'&&!typing&&(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)&&!(GetKeyState(VK_SHIFT)&0x8000)){if(!(msg.lParam&(1L<<30)))deleteSelectedSound();return true;}
  if(key=='F'&&(GetKeyState(VK_CONTROL)&0x8000)&&!effects&&(screen==lp::Screen::Instruments||screen==lp::Screen::PerformanceEditor)){auto search=item(screen==lp::Screen::PerformanceEditor?PartSearch:Search);SetFocus(search);SendMessageW(search,EM_SETSEL,0,-1);return true;}
  if(GetFocus()==item(Search)&&key==VK_RETURN){SetFocus(catalog);if(restoringSounds)restoreSelectedSound();else audition();return true;}
  if(key==VK_F12){command(Panic,0);return true;}
  if(key==VK_ESCAPE){if(effects){cancelMidiLearn();return true;}goHome();return true;}
  if(!typing&&!(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)){
   if(key=='E'){toggleParameters();return true;}
   if(!effects){
   if(key=='P'){command(PerformanceButton,0);return true;}
   if(key=='C'){openScreen(lp::Screen::Settings);return true;}
   if(key=='A'){openScreen(lp::Screen::AudioMidi);return true;}
   if(key=='K'){openScreen(lp::Screen::Keyboard);return true;}
   }
  }
  if(!effects&&!typing&&screen==lp::Screen::Home&&!(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)&&
     (key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN)){
   int id=GetDlgCtrlID(GetFocus()),index=id>=CategoryCardBase&&id<CategoryCardBase+16?id-CategoryCardBase:categoryIndex;
   index=lp::CategoryGrid::neighbor(index,key==VK_LEFT?-1:(key==VK_RIGHT?1:0),key==VK_UP?-1:(key==VK_DOWN?1:0));
   SetFocus(item(CategoryCardBase+index));return true;
  }
  if(key==VK_UP||key==VK_DOWN||key==VK_PRIOR||key==VK_NEXT){
   bool native=_wcsicmp(kind,L"LISTBOX")==0||_wcsicmp(kind,L"COMBOBOX")==0||_wcsicmp(kind,TRACKBAR_CLASSW)==0||(textControl&&(GetWindowLongPtrW(GetFocus(),GWL_STYLE)&ES_MULTILINE));
   if(native)return false;
   if(!effects&&screen==lp::Screen::About){auto text=item(AboutText);SetFocus(text);SendMessageW(text,WM_KEYDOWN,key,msg.lParam);return true;}
   if(!effects&&screen==lp::Screen::Instruments){auto list=categoryIndex==15?favorites:catalog;SetFocus(list);SendMessageW(list,WM_KEYDOWN,key,msg.lParam);return true;}
   int direction=key==VK_UP||key==VK_PRIOR?-1:1;scrollPage(direction*((key==VK_PRIOR||key==VK_NEXT)?std::max(48,pageScroll.viewport-48):48));return true;
  }
  if(!effects&&screen==lp::Screen::Home){if(key==VK_RETURN){int focused=GetDlgCtrlID(GetFocus());if(focused>=CategoryCardBase&&focused<CategoryCardBase+16){categoryIndex=focused-CategoryCardBase;openCategory();return true;}if(GetFocus()==window){openCategory();return true;}}}
  if(!effects&&screen==lp::Screen::Instruments){
   if(key==VK_SPACE){favorite();return true;}
   if(key==VK_RETURN){if(restoringSounds){restoreSelectedSound();return true;}if(categoryIndex==15){auto slot=SendMessageW(favorites,LB_GETCURSEL,0,0);if(slot>=0)activate(static_cast<unsigned>(slot));}else audition();return true;}
  }
  if(key==VK_DELETE&&performanceVisible){for(int i=0;i<4;++i)if(GetFocus()==performancePartControls[i]){clearPerformancePart(i);return true;}}
  return false;
 }
