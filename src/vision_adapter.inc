 // The Vision renderer uses the existing App commands and controls as its model.
 // It never creates an AudioHost or a second physical MIDI input.
#ifdef LP_VISION_UI
 std::unique_ptr<lp::vision::WebView> vision;
 bool visionDirty=true;std::string visionLastError;std::array<bool,128> visionNotes{};
 static std::string visionUtf8(const std::wstring& value){
  if(value.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
  if(!n)throw std::invalid_argument("Texto Unicode inválido");std::string result(n,'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),n,nullptr,nullptr);return result;
 }
 static std::wstring visionWide(const std::string& value){
  if(value.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
  if(!n)throw std::invalid_argument("Texto UTF-8 inválido");std::wstring result(n,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),n);return result;
 }
 static std::string visionText(HWND control){int n=GetWindowTextLengthW(control);if(n<0||n>1000000)return {};std::wstring value(n+1,L'\0');GetWindowTextW(control,value.data(),n+1);value.resize(n);return visionUtf8(value);}
 static std::string visionOption(HWND control,int index){auto n=SendMessageW(control,CB_GETLBTEXTLEN,index,0);if(n<0||n>1000000)return {};std::wstring value(static_cast<std::size_t>(n)+1,L'\0');SendMessageW(control,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(value.data()));value.resize(n);return visionUtf8(value);}
 static void visionSelect(HWND control,int index){auto count=SendMessageW(control,CB_GETCOUNT,0,0);if(index<0||index>=count)throw std::invalid_argument("Seleção indisponível");SendMessageW(control,CB_SETCURSEL,index,0);}
 bool visionVisible()const{return vision&&vision->active();}
 bool visionPending()const{return vision&&vision->pending();}
 void visionLoadTimeout(){KillTimer(window,3);if(visionPending())vision->useClassic();}
 HWND visionContainer()const{return vision?vision->container():nullptr;}
 void markVisionDirty(){visionDirty=true;}
 void recordVisionError(const std::exception& e){visionLastError=e.what();markVisionDirty();}
 void releaseVisionNotes(){for(unsigned n=0;n<128;++n)if(visionNotes[n]){if(engine)engine->send({0x80,static_cast<std::uint8_t>(n),0});visionNotes[n]=false;}}
 void closeVision(){KillTimer(window,3);releaseVisionNotes();vision.reset();MSG pending{};while(PeekMessageW(&pending,window,VisionCommandMessage,VisionCommandMessage,PM_REMOVE))delete reinterpret_cast<std::wstring*>(pending.lParam);}
 std::string visionState(unsigned request=0){
  using lp::vision::quote;std::ostringstream out;out.imbue(std::locale::classic());
  static const char* screens[]={"home","instruments","performance","audio","profiles","about","keyboard"};
  std::string reference,loadedName;
  if(engine&&engine->loaded()){
   if(show.current&&show.favorites[*show.current]){reference=show.favorites[*show.current]->sfz;loadedName=visionUtf8(favoriteName(*show.favorites[*show.current]));}
   else if(previewActive){reference=previewPatch.sfz;loadedName=lp::instrumentName(reference,instruments);}
   else if(editingPerformanceValid){reference="performance://"+std::to_string(activePerformanceIndex);loadedName=editingPerformance.name;}
  }
  out<<"{\"type\":\"state\",\"version\":1,\"request\":"<<request<<",\"error\":"<<quote(visionLastError)<<",\"screen\":"<<quote(screens[static_cast<int>(screen)])<<",\"effects\":"<<(effects?"true":"false")<<",\"loading\":"<<(loading?"true":"false")<<",\"progress\":"<<progress<<",\"status\":"<<quote(visionText(status))<<",\"loadedName\":"<<quote(loadedName)<<",\"category\":"<<categoryIndex<<",\"selected\":"<<SendMessageW(categoryIndex==15?favorites:catalog,LB_GETCURSEL,0,0)<<",\"categories\":[";
  for(unsigned i=0;i<16;++i){if(i)out<<',';out<<quote(i==15?"Favoritos":lp::instrumentCategories[i]);}out<<"],\"catalog\":[";
  if(categoryIndex!=15){auto count=SendMessageW(catalog,LB_GETCOUNT,0,0);for(int i=0;i<count;++i){if(i)out<<',';auto n=SendMessageW(catalog,LB_GETTEXTLEN,i,0);std::wstring label(static_cast<std::size_t>(n)+1,L'\0');SendMessageW(catalog,LB_GETTEXT,i,reinterpret_cast<LPARAM>(label.data()));label.resize(n);std::string ref;
    if(categoryIndex==13&&static_cast<std::size_t>(i)<visiblePerformances.size())ref="performance://"+std::to_string(visiblePerformances[i]);
    else if(static_cast<std::size_t>(i)<visibleInstruments.size())ref=lp::instrumentReference(instruments[visibleInstruments[i]]);
    out<<"{\"id\":"<<i<<",\"name\":"<<quote(visionUtf8(label))<<",\"favorite\":"<<(isFavorite(ref)?"true":"false")<<",\"loaded\":"<<(!ref.empty()&&ref==reference?"true":"false")<<'}';
   }}out<<"],\"favorites\":[";
  for(unsigned i=0;i<20;++i){if(i)out<<',';auto name=show.performances[i]?show.performances[i]->name:(show.favorites[i]?visionUtf8(favoriteName(*show.favorites[i])):"Vazio");out<<"{\"id\":"<<i<<",\"name\":"<<quote(name)<<",\"occupied\":"<<(show.favorites[i]?"true":"false")<<",\"loaded\":"<<(show.current&&*show.current==i&&engine&&engine->loaded()?"true":"false")<<'}';}out<<"],\"controls\":[";
  for(int i=0;i<16;++i){if(i)out<<',';out<<"{\"id\":"<<i<<",\"name\":"<<quote(visionText(sliders[i]))<<",\"label\":"<<quote(visionText(labels[i]))<<",\"value\":"<<SendMessageW(sliders[i],TBM_GETPOS,0,0)<<",\"enabled\":"<<(IsWindowEnabled(sliders[i])?"true":"false")<<'}';}out<<"],\"partMix\":"<<(performanceControls()?"true":"false")<<",\"audio\":{";
  auto options=[&](const char* name,HWND control){out<<quote(name)<<":[";auto count=SendMessageW(control,CB_GETCOUNT,0,0);for(int i=0;i<count;++i){if(i)out<<',';out<<quote(visionOption(control,i));}out<<']';};
  options("drivers",audioDriver);out<<",\"driver\":"<<SendMessageW(audioDriver,CB_GETCURSEL,0,0)<<',';options("midiInputs",midi);out<<",\"midi\":"<<SendMessageW(midi,CB_GETCURSEL,0,0)<<",\"connected\":"<<(engine&&engine->running()?"true":"false")<<",\"sampleRate\":"<<(engine?engine->sampleRate():0)<<",\"latencyMs\":"<<(engine&&engine->running()?engine->latency()*1000:0)<<"},\"performance\":{\"mode\":"<<SendMessageW(performanceMode,CB_GETCURSEL,0,0)<<",\"name\":"<<quote(visionText(performanceName))<<",\"parts\":[";
  for(int i=0;i<4;++i){if(i)out<<',';out<<SendMessageW(performancePartControls[i],CB_GETCURSEL,0,0);}out<<"],\"editingPart\":"<<editingPart<<",\"instrumentCategories\":[-1";for(const auto& instrument:instruments)out<<','<<instrument.category;out<<"],";options("modes",performanceMode);out<<',';options("instruments",performancePartControls[0]);out<<"},\"about\":"<<quote(visionText(item(AboutText)))<<'}';return out.str();
 }
 void flushVision(unsigned request=0){if(!visionVisible()||(!visionDirty&&!request))return;vision->post(visionWide(visionState(request)));visionDirty=false;}
 void startVision(){
#ifdef LP_VISION_TEST
  return; // Exercise the native dispatcher without starting a browser/runtime.
#endif
  try{
  vision=std::make_unique<lp::vision::WebView>();
  bool started=vision->start(window,base/L"vision-ui",[this](std::wstring wire){auto text=std::make_unique<std::wstring>(std::move(wire));if(PostMessageW(window,VisionCommandMessage,0,reinterpret_cast<LPARAM>(text.get())))text.release();},[this](bool active){KillTimer(window,3);if(!active)releaseVisionNotes();markVisionDirty();layout();if(!IsWindowVisible(window)){ShowWindow(window,SW_SHOW);UpdateWindow(window);}if(!active)SetFocus(item(CategoryCardBase+categoryIndex));});
  if(started)SetTimer(window,3,10000,nullptr);
  if(!started){vision.reset();message(L"Interface clássica ativa. Para o visual Vision, instale o Microsoft Edge WebView2 Runtime e mantenha vision-ui e WebView2Loader.dll ao lado do programa.");}
  }catch(const std::exception&){vision.reset();message(L"Interface clássica ativa. Não foi possível iniciar o visual Vision neste Windows.");}
 }
 void visionCommand(const std::wstring& wire){
  unsigned sequence=0;visionLastError.clear();
  try{
   auto c=lp::vision::parse(visionUtf8(wire));sequence=c.sequence;
   if(c.action=="ready"){if(vision&&!visionVisible())vision->reveal();}
   else if(loading){throw std::runtime_error("Aguarde o carregamento da biblioteca");}
   else if(c.action=="category"){releaseVisionNotes();effects=false;parameterPanel.open=false;command(CategoryCardBase+c.a,BN_CLICKED);}
   else if(c.action=="select"||c.action=="favorite"){
    if(screen!=lp::Screen::Instruments)throw std::invalid_argument("Abra uma categoria primeiro");auto list=categoryIndex==15?favorites:catalog;auto count=SendMessageW(list,LB_GETCOUNT,0,0);if(c.a>=count)throw std::invalid_argument("Timbre ausente");SendMessageW(list,LB_SETCURSEL,c.a,0);
    if(c.action=="favorite")favorite();else if(categoryIndex==15)activate(c.a);else audition();
   }
   else if(c.action=="activateFavorite"||c.action=="removeFavorite"||c.action=="learnFavorite"){
    if(!show.favorites[c.a])throw std::runtime_error("Este favorito está vazio");
    if(c.action=="activateFavorite")activate(c.a);
    else if(c.action=="learnFavorite"){if(!midiHandle)throw std::runtime_error("Selecione a entrada MIDI antes de mapear");router.learnFavorite(c.a);message(L"MIDI Learn: pressione Program Change "+std::to_wstring(c.a+1)+L". ESC cancela.");}
    else{categoryIndex=15;SendMessageW(category,CB_SETCURSEL,15,0);SendMessageW(favorites,LB_SETCURSEL,c.a,0);favorite();}
   }
   else if(c.action=="screen"){
    releaseVisionNotes();constexpr int ids[]={HomeButton,HomeButton,PerformanceButton,AudioMidiButton,SettingsButton,AboutButton,KeyboardButton};if(c.a==1){effects=false;parameterPanel.open=false;openCategory();}else command(ids[c.a],BN_CLICKED);
   }
   else if(c.action=="deleteSelected")deleteSelectedSound();
   else if(c.action=="effects"){releaseVisionNotes();toggleParameters();}
   else if(c.action=="escape"){cancelMidiLearn();releaseVisionNotes();if(!effects)command(HomeButton,BN_CLICKED);}
   else if(c.action=="parameter"){if(!effects||!IsWindowEnabled(sliders[c.a]))throw std::runtime_error("Controle indisponível");setPanelValue(c.a,c.b/1000.0f);}
   else if(c.action=="learnParameter")beginParameterLearn(c.a);
   else if(c.action=="driver"||c.action=="midi"){
    int id=c.action=="driver"?AudioDriver:Midi;visionSelect(item(id),c.a);command(id,CBN_SELCHANGE);
   }
   else if(c.action=="performanceMode")visionSelect(performanceMode,c.a);
   else if(c.action=="performancePart"){visionSelect(performancePartControls[c.a],c.b);editingPart=c.a;refreshParameters();}
   else if(c.action=="editPart"){
    if(screen!=lp::Screen::PerformanceEditor)throw std::invalid_argument("Abra o Performance Editor");
    cancelMidiLearn();editingPart=c.a;SendMessageW(performanceEditPart,CB_SETCURSEL,c.a,0);refreshParameters();
   }
   else if(c.action=="performanceName"){auto name=visionWide(c.text);if(name.size()>255)throw std::invalid_argument("Nome muito longo");SetWindowTextW(performanceName,name.c_str());}
   else if(c.action=="clearPart")clearPerformancePart(c.a);
   else if(c.action=="noteOn"){if(screen!=lp::Screen::Keyboard||!engine||!engine->loaded())throw std::runtime_error("Selecione um timbre antes de tocar");if(c.b&&!visionNotes[c.a]){engine->send({0x90,static_cast<std::uint8_t>(c.a),static_cast<std::uint8_t>(c.b)});visionNotes[c.a]=true;}}
   else if(c.action=="noteOff"){if(visionNotes[c.a]){if(engine)engine->send({0x80,static_cast<std::uint8_t>(c.a),0});visionNotes[c.a]=false;}}
   else if(c.action=="releaseNotes")releaseVisionNotes();
   else if(c.action=="touch"){/* Compatibility with earlier frontends; no inactivity timer. */}
   else{
    const std::pair<const char*,int> actions[]={{"connect",AudioStart},{"disconnect",AudioStop},{"controlPanel",AudioPanel},{"panic",Panic},{"save",Save},{"open",Open},{"importSfz",Import},{"importSf2",ImportSf2},{"performancePlay",PerformancePlay},{"performanceSave",PerformanceSave}};
    bool found=false;for(const auto& action:actions)if(c.action==action.first){if(c.action=="panic")releaseVisionNotes();command(action.second,BN_CLICKED);found=true;break;}if(!found)throw std::invalid_argument("Comando indisponível");
   }
  }catch(const std::exception& e){visionLastError=e.what();message(visionWide(visionLastError));}
  markVisionDirty();flushVision(sequence);
 }
#else
 bool visionVisible()const{return false;}bool visionPending()const{return false;}void visionLoadTimeout(){}HWND visionContainer()const{return nullptr;}
 void markVisionDirty(){}void recordVisionError(const std::exception&){}void closeVision(){}void flushVision(unsigned=0){}void startVision(){}void visionCommand(const std::wstring&){}
#endif
