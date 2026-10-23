 void drawSlider(NMCUSTOMDRAW* item){
  HDC dc=item->hdc;RECT r{};GetClientRect(item->hdr.hwndFrom,&r);lpui::fill(dc,r,lpui::panel);
  RECT channel{};SendMessageW(item->hdr.hwndFrom,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&channel));
  int start=channel.left,end=channel.right,cy=(r.bottom+r.top)/2;
  int value=static_cast<int>(SendMessageW(item->hdr.hwndFrom,TBM_GETPOS,0,0));int x=start+(end-start)*value/1000;
  lpui::surface(dc,{start,cy-4,end,cy+5},RGB(58,69,86),RGB(58,69,86),RGB(58,69,86),4);
  if(x>start+2)lpui::surface(dc,{start,cy-4,x,cy+5},lpui::gold,RGB(170,126,64),lpui::gold,4);
  lpui::surface(dc,{x-10,cy-14,x+11,cy+15},RGB(255,227,169),lpui::gold,lpui::gold,6);
  if(GetFocus()==item->hdr.hwndFrom)lpui::box(dc,{1,1,r.right-2,r.bottom-2},lpui::gold,2);
 }
 void drawButton(DRAWITEMSTRUCT* item){
  HDC dc=item->hDC;RECT r=item->rcItem;bool pressed=item->itemState&ODS_SELECTED,focused=item->itemState&ODS_FOCUS;
  bool card=item->CtlID>=CategoryCardBase&&item->CtlID<CategoryCardBase+16;
  int artIndex=card?static_cast<int>(item->CtlID)-CategoryCardBase:categoryIndex;
  POINT cursor{};GetCursorPos(&cursor);ScreenToClient(item->hwndItem,&cursor);bool hovered=PtInRect(&r,cursor)!=FALSE;
  bool active=card&&artIndex==categoryIndex;
  if(item->CtlID==PerformanceButton)active=!effects&&screen==lp::Screen::PerformanceEditor;
  if(item->CtlID==AudioMidiButton)active=!effects&&screen==lp::Screen::AudioMidi;
  if(item->CtlID==SettingsButton)active=!effects&&screen==lp::Screen::Settings;
  if(item->CtlID==AboutButton)active=!effects&&screen==lp::Screen::About;
  bool primary=item->CtlID==AudioStart||item->CtlID==PerformancePlay||item->CtlID==PerformanceSave;
  active=active||primary;
  lpui::fill(dc,r,lpui::ink);
  lpui::surface(dc,{r.left+2,r.top+2,r.right-2,r.bottom-2},pressed?RGB(69,59,43):(active?RGB(48,43,34):(hovered?RGB(37,45,58):lpui::panel)),active?RGB(29,28,27):RGB(18,23,32),(focused||active)?lpui::gold:RGB(66,77,94),14);
  if(item->CtlID==Open||item->CtlID==Save||item->CtlID==Import||item->CtlID==ImportSf2){
   bool save=item->CtlID==Save,importing=item->CtlID==Import||item->CtlID==ImportSf2;
   auto tag=importing?L"BIBLIOTECA DE INSTRUMENTOS":L"PERFIL DE SHOW";
   lpui::text(dc,tag,{r.left+24,r.top+16,r.right-24,r.top+46},smallFont,lpui::gold,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   wchar_t title[128]{};GetWindowTextW(item->hwndItem,title,128);lpui::text(dc,title,{r.left+24,r.top+50,r.right-24,r.top+100},largeFont,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   auto description=save?L"Guarde favoritos, ajustes e controles.":(item->CtlID==Open?L"Retome o repertório de cada apresentação.":(item->CtlID==Import?L"Adicione um instrumento à sua biblioteca.":L"Adicione os timbres de um SoundFont."));
   lpui::text(dc,description,{r.left+24,r.top+112,r.right-24,r.bottom-16},smallFont,lpui::muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
  }else if(card){
   // Reserve the full card width for the accessible name; photos never compete with text.
   RECT picture{r.left+10,r.top+8,r.right-10,r.bottom-36};
   lpui::categoryPhoto(dc,categoryPhotos.get(),picture,artIndex);
   auto label=artIndex==15?std::wstring(L"Favoritos"):std::filesystem::u8path(lp::instrumentCategories[artIndex]).wstring();
   RECT caption{r.left+10,r.bottom-32,r.right-10,r.bottom-4};
   lpui::text(dc,label,caption,font,lpui::white,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
   if(artIndex>=13){RECT badge{picture.right-24,picture.top,picture.right,picture.top+24};lpui::fill(dc,badge,lpui::ink);lpui::text(dc,artIndex==15?L"★":(artIndex==14?L"+":L"P"),badge,smallFont,lpui::gold);}
  }else if(item->CtlID==CategoryTile||item->CtlID==InstrumentArt){
   RECT art{r.left+32,r.top+28,r.right-32,r.bottom-156};lpui::categoryPhoto(dc,categoryPhotos.get(),art,artIndex);
   lpui::text(dc,categoryName(),{r.left+16,r.bottom-152,r.right-16,r.bottom-102},largeFont);
   lpui::text(dc,L"Sua biblioteca de palco",{r.left+16,r.bottom-96,r.right-16,r.bottom-60},font,lpui::muted);
   lpui::text(dc,L"↑ ↓ Selecionar   ·   Espaço Favoritar",{r.left+16,r.bottom-56,r.right-16,r.bottom-18},smallFont,lpui::gold);
  }else{
   wchar_t label[128]{};GetWindowTextW(item->hwndItem,label,128);
   lpui::text(dc,label,{r.left+8,r.top+8,r.right-8,r.bottom-8},font,active?lpui::gold:lpui::white,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
  }
  if(focused)lpui::box(dc,{r.left+7,r.top+7,r.right-7,r.bottom-7},lpui::gold,2);
 }
 void drawList(DRAWITEMSTRUCT* item){
  if(item->itemID==static_cast<UINT>(-1))return;
  auto h=item->hwndItem;auto length=SendMessageW(h,LB_GETTEXTLEN,item->itemID,0);if(length<0)return;
  std::wstring name(static_cast<std::size_t>(length)+1,L'\0');SendMessageW(h,LB_GETTEXT,item->itemID,reinterpret_cast<LPARAM>(name.data()));
  RECT r=item->rcItem;bool selected=item->itemState&ODS_SELECTED;
  lpui::fill(item->hDC,r,lpui::ink);RECT row{r.left+4,r.top+3,r.right-4,r.bottom-3};
  lpui::surface(item->hDC,row,selected?RGB(55,47,35):lpui::panel,selected?RGB(37,34,28):RGB(20,25,34),selected?lpui::gold:RGB(48,58,73),10);
  if(selected)lpui::fill(item->hDC,{r.left+5,r.top+12,r.left+9,r.bottom-12},lpui::gold);
  RECT label{r.left+24,r.top+6,r.right-62,r.bottom-6};lpui::text(item->hDC,name,label,font,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
  bool star=false;
  if(h==favorites)star=show.favorites[item->itemID].has_value();
  else if(categoryIndex==13)star=isFavorite("performance://"+std::to_string(visiblePerformances[item->itemID]));
  else if(item->itemID<visibleInstruments.size())star=isFavorite(lp::instrumentReference(instruments[visibleInstruments[item->itemID]]));
  RECT mark{r.right-56,r.top,r.right-6,r.bottom};lpui::text(item->hDC,star?L"★":L"☆",mark,largeFont,lpui::gold);
  if(item->itemState&ODS_FOCUS)lpui::box(item->hDC,{r.left+2,r.top+2,r.right-2,r.bottom-2},lpui::gold,3);
 }
 bool isFavorite(const std::string& reference) const {for(const auto& item:show.favorites)if(item&&item->sfz==reference)return true;return false;}
 void drawCombo(DRAWITEMSTRUCT* item){
  HDC dc=item->hDC;RECT r=item->rcItem;bool selected=(item->itemState&ODS_SELECTED)!=0;
  lpui::fill(dc,r,selected?RGB(56,47,34):lpui::panel);
  int index=static_cast<int>(item->itemID);if(index<0)index=static_cast<int>(SendMessageW(item->hwndItem,CB_GETCURSEL,0,0));
  if(index>=0){auto length=SendMessageW(item->hwndItem,CB_GETLBTEXTLEN,index,0);if(length>=0){std::wstring label(length+1,L'\0');SendMessageW(item->hwndItem,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(label.data()));
   lpui::text(dc,label,{r.left+12,r.top,r.right-10,r.bottom},font,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);}}
  if(item->itemState&ODS_FOCUS)lpui::box(dc,{r.left+2,r.top+2,r.right-2,r.bottom-2},lpui::gold,1);
 }
 void paint(HDC dc,bool bodyOnly=false){
  RECT bounds{};GetClientRect(window,&bounds);int width=bounds.right,height=bounds.bottom,p=pageOrigin;
  lpui::fill(dc,bounds,lpui::ink);
  auto label=[&](const std::wstring& text,RECT r){lpui::text(dc,text,r,smallFont,lpui::muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);};
  auto panel=[&](RECT r){lpui::surface(dc,r,lpui::panel,lpui::panel,RGB(52,63,80),16);};
  if(loading){
   int split=width*44/100,x=width*48/100,y=std::max(56,height/2-190);
   lpui::surface(dc,{24,24,split,height-24},RGB(29,35,46),RGB(18,23,32),RGB(58,67,83),20);
   lpui::photo(dc,photo.get(),{38,38,split-14,height-38});
   label(L"INSTRUMENTOS  ·  PALCO  ·  EXPRESSÃO",{x,y,width-36,y+30});
   lpui::text(dc,L"Live Performance",{x,y+44,width-32,y+114},titleFont,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   lpui::text(dc,L"by Hugo Sodré",{x,y+118,width-32,y+178},signatureFont,lpui::gold,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   label(L"Seu repertório. Sua expressão.",{x,y+194,width-36,y+228});
   int by=y+252;RECT bar{x,by,width-48,by+12};lpui::surface(dc,bar,RGB(58,67,81),RGB(58,67,81),RGB(58,67,81),6);
   if(progress>0){RECT fill=bar;fill.right=bar.left+(bar.right-bar.left)*progress/100;if(fill.right>fill.left+3)lpui::surface(dc,fill,lpui::gold,RGB(184,139,68),lpui::gold,6);}
   auto elapsed=(GetTickCount64()-loadingStart)/1000;std::wstring estimate=L"Preparando seus instrumentos";if(progress>0&&progress<100&&elapsed>0)estimate=L"Cerca de "+std::to_wstring(elapsed*(100-progress)/progress)+L" s restantes";label(estimate,{x,by+24,width-164,by+58});
   lpui::text(dc,std::to_wstring(progress)+L"%",{width-148,by+24,width-48,by+58},font,lpui::gold,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
   label(L"LIVE PERFORMANCE  /  VERSÃO 1.0",{x,height-48,width-40,height-16});return;
  }
  if(!bodyOnly){
   lpui::surface(dc,{16,12,width-16,100},RGB(29,35,46),RGB(18,23,32),RGB(51,61,77),18);
   lpui::photo(dc,photo.get(),{28,24,88,84},true);
   int brandRight=width-672;
   lpui::text(dc,L"Live Performance",{104,22,brandRight,54},font,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   lpui::text(dc,L"by Hugo Sodré",{104,54,brandRight,86},font,lpui::gold,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   std::wstring title;
   if(effects)title=performanceControls()?L"Mix da Performance":L"Parâmetros do instrumento";
   else switch(screen){case lp::Screen::Home:title=L"Biblioteca de instrumentos";break;case lp::Screen::Instruments:title=categoryName();break;case lp::Screen::AudioMidi:title=L"Áudio e MIDI";break;case lp::Screen::Settings:title=L"Perfis de show e arquivos";break;case lp::Screen::PerformanceEditor:title=L"Performance";break;case lp::Screen::Keyboard:title=L"Teclado virtual";break;case lp::Screen::About:title=L"Música, visão e propósito";break;}
   lpui::text(dc,title,{32,108,width-230,148},largeFont,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   lpui::fill(dc,{0,height-70,width,height},lpui::panel);
   lpui::line(dc,24,height-71,width-24,height-71,RGB(55,66,83),1);
   Gdiplus::Graphics g(dc);Gdiplus::SolidBrush dot(lpui::color(engine&&engine->running()?RGB(110,211,169):lpui::gold));g.FillEllipse(&dot,28,height-44,10,10);
   return;
  }
  width-=20;
  if(effects){
   int count=performanceControls()?4:16,rows=performanceControls()?2:8,half=(width-84)/2;
   label(performanceControls()?L"PARTES 1 E 2":L"EXPRESSÃO E AMBIENTE",{32,p+8,half,p+40});
   label(performanceControls()?L"PARTES 3 E 4":L"TEXTURA E SAÍDA",{56+half,p+8,width-32,p+40});
   for(int i=0;i<count;++i){int x=28+(i/rows)*(half+28),y=p+56+(i%rows)*96;panel({x,y,x+half,y+88});}
   return;
  }
  switch(screen){
  case lp::Screen::Home:break;
  case lp::Screen::Instruments:{
   int left=width*2/5;label(categoryIndex==15?L"SEUS FAVORITOS  ·  PROGRAM CHANGE 1–20":L"SELECIONE UM TIMBRE PARA TOCAR",{left+16,p+8,width-32,p+48});
   break;
  }
  case lp::Screen::Keyboard:
   panel({32,p+16,width-32,p+pageScroll.content-24});
   lpui::text(dc,L"Toque o instrumento selecionado",{56,p+32,width-56,p+78},largeFont,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   label(L"Clique nas teclas. ESC retorna à sua biblioteca.",{56,p+80,width-56,p+116});drawKeyboard(dc);break;
  case lp::Screen::AudioMidi:{
   panel({32,p+8,width-32,p+82});
   lpui::text(dc,engine&&engine->running()?L"Áudio conectado":L"Prepare sua conexão de palco",{56,p+20,width-56,p+68},font,lpui::gold,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   panel({32,p+100,width-32,p+424});int driverWidth=width-408;
   label(L"DRIVER ASIO",{56,p+112,56+driverWidth,p+146});label(L"BUFFER",{80+driverWidth,p+112,width-56,p+146});
   label(L"CONTROLADOR MIDI",{56,p+222,width-56,p+256});
   panel({32,p+444,width-32,p+552});
   auto rate=engine&&engine->running()?std::to_wstring(static_cast<int>(engine->sampleRate()))+L" Hz  ·  Frequência do áudio":L"Frequência ajustada automaticamente pelo driver";
   lpui::text(dc,rate,{56,p+456,width-320,p+494},font,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   label(L"F12 interrompe todas as notas.",{56,p+500,width-320,p+534});break;
  }
  case lp::Screen::Settings:
   panel({32,p+416,width-32,p+548});
   lpui::text(dc,L"Seu show, pronto para voltar",{56,p+436,width-320,p+480},font,lpui::gold,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   label(L"Salve favoritos, ajustes e mapeamentos em um perfil.",{56,p+486,width-320,p+522});break;
  case lp::Screen::PerformanceEditor:
   panel({32,p+8,width-32,p+128});label(L"DISTRIBUIÇÃO DO TECLADO",{56,p+22,width-56,p+56});
   for(int i=0;i<4;++i){int y=p+152+i*96;panel({32,y,width-32,y+84});
    lpui::surface(dc,{48,y+20,100,y+66},RGB(49,43,33),RGB(49,43,33),RGB(112,92,61),12);lpui::text(dc,std::to_wstring(i+1),{48,y+20,100,y+66},font,lpui::gold);}
   panel({32,p+552,width-32,p+748});label(L"NOME DA PERFORMANCE",{56,p+564,width-56,p+596});break;
  case lp::Screen::About:{
   int left=width/3;panel({32,p+8,left+16,p+pageScroll.content-24});
   lpui::photo(dc,photo.get(),{48,p+24,left,p+pageScroll.content-152});
   lpui::text(dc,L"Hugo Sodré",{48,p+pageScroll.content-138,left,p+pageScroll.content-88},largeFont,lpui::white,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   label(L"ARTISTA E CRIADOR",{48,p+pageScroll.content-80,left,p+pageScroll.content-42});
   panel({left+24,p+8,width-32,p+pageScroll.content-24});label(L"TRAJETÓRIA E CRÉDITOS",{left+44,p+22,width-56,p+56});break;
  }
  }
 }
