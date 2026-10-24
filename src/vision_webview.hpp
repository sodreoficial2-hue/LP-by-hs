#pragma once
#include <windows.h>
#include <shlobj.h>
#include <WebView2.h>
#include "webview2_compat/uuids.hpp"
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace lp::vision {
template<class T> class Com {
 T* value=nullptr;
public:
 Com()=default;Com(const Com&)=delete;Com& operator=(const Com&)=delete;
 ~Com(){reset();}void reset(T* p=nullptr){if(value)value->Release();value=p;}
 T* get() const{return value;}T* operator->()const{return value;}explicit operator bool()const{return value!=nullptr;}
 T** put(){reset();return &value;}
};
template<class Interface,class... Args> class Handler final: public Interface {
 std::atomic<ULONG> refs{1};std::function<HRESULT(Args...)> function;
public:
 explicit Handler(std::function<HRESULT(Args...)> f):function(std::move(f)){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{
  if(!out)return E_POINTER;*out=nullptr;
  if(IsEqualIID(iid,IID_IUnknown)||IsEqualIID(iid,__uuidof(Interface))){*out=static_cast<Interface*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
 }
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE Invoke(Args... args)override{try{return function(args...);}catch(...){return E_FAIL;}}
};
template<class I,class... A,class F> Handler<I,A...>* handler(F&& f){return new Handler<I,A...>(std::forward<F>(f));}

class WebView {
 struct Life {WebView* self=nullptr;};std::shared_ptr<Life> life=std::make_shared<Life>();
 Com<ICoreWebView2Controller> controller;Com<ICoreWebView2> web;
 HMODULE loader=nullptr;HWND child=nullptr,owner=nullptr;bool shown=false,starting=false;
 std::function<void(std::wstring)> receive;std::function<void(bool)> visibility;
 std::filesystem::path assets;
 static constexpr const wchar_t* origin=L"https://lp-vision.local/index.html";
 static bool trusted(const wchar_t* uri){return uri&&std::wstring(uri)==origin;}
 void fallback(){starting=false;shown=false;if(child)ShowWindow(child,SW_HIDE);if(visibility)visibility(false);}
 HRESULT setup(ICoreWebView2Controller* value){
  if(!value)return E_FAIL;value->AddRef();controller.reset(value);HRESULT hr=controller->get_CoreWebView2(web.put());if(FAILED(hr))return hr;
  Com<ICoreWebView2_3> map;if(FAILED(web->QueryInterface(__uuidof(ICoreWebView2_3),reinterpret_cast<void**>(map.put())))){fallback();return E_NOINTERFACE;}
  hr=map->SetVirtualHostNameToFolderMapping(L"lp-vision.local",assets.c_str(),COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);if(FAILED(hr)){fallback();return hr;}
  Com<ICoreWebView2Settings> settings;web->get_Settings(settings.put());
  if(settings){settings->put_IsScriptEnabled(TRUE);settings->put_IsWebMessageEnabled(TRUE);settings->put_AreDevToolsEnabled(FALSE);settings->put_AreDefaultContextMenusEnabled(FALSE);settings->put_IsStatusBarEnabled(FALSE);settings->put_AreHostObjectsAllowed(FALSE);settings->put_IsZoomControlEnabled(TRUE);}
  const std::weak_ptr<Life> weak=life;EventRegistrationToken token{};
  Com<ICoreWebView2WebMessageReceivedEventHandler> messages;
  messages.reset(handler<ICoreWebView2WebMessageReceivedEventHandler,ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs*>([weak](ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs* args){
   auto guard=weak.lock();if(!guard||!guard->self)return S_OK;auto& self=*guard->self;
   LPWSTR source=nullptr,text=nullptr;args->get_Source(&source);bool allowed=trusted(source);CoTaskMemFree(source);
   if(allowed&&SUCCEEDED(args->TryGetWebMessageAsString(&text))&&text){if(wcslen(text)<=4096&&self.receive)self.receive(text);CoTaskMemFree(text);}return S_OK;
  }));web->add_WebMessageReceived(messages.get(),&token);
  Com<ICoreWebView2NavigationStartingEventHandler> navigation;
  navigation.reset(handler<ICoreWebView2NavigationStartingEventHandler,ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs*>([](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs* args){LPWSTR uri=nullptr;args->get_Uri(&uri);args->put_Cancel(!trusted(uri));CoTaskMemFree(uri);return S_OK;}));web->add_NavigationStarting(navigation.get(),&token);
  Com<ICoreWebView2NewWindowRequestedEventHandler> windows;
  windows.reset(handler<ICoreWebView2NewWindowRequestedEventHandler,ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs*>([](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs* args){args->put_Handled(TRUE);return S_OK;}));web->add_NewWindowRequested(windows.get(),&token);
  Com<ICoreWebView2PermissionRequestedEventHandler> permissions;
  permissions.reset(handler<ICoreWebView2PermissionRequestedEventHandler,ICoreWebView2*,ICoreWebView2PermissionRequestedEventArgs*>([](ICoreWebView2*,ICoreWebView2PermissionRequestedEventArgs* args){args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);return S_OK;}));web->add_PermissionRequested(permissions.get(),&token);
  Com<ICoreWebView2NavigationCompletedEventHandler> completed;
  completed.reset(handler<ICoreWebView2NavigationCompletedEventHandler,ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs*>([weak](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* args){auto guard=weak.lock();if(!guard||!guard->self)return S_OK;BOOL ok=FALSE;args->get_IsSuccess(&ok);if(!ok)guard->self->fallback();return S_OK;}));web->add_NavigationCompleted(completed.get(),&token);
  Com<ICoreWebView2ProcessFailedEventHandler> failure;
  failure.reset(handler<ICoreWebView2ProcessFailedEventHandler,ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs*>([weak](ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs*){auto guard=weak.lock();if(guard&&guard->self)guard->self->fallback();return S_OK;}));web->add_ProcessFailed(failure.get(),&token);
  resize();return web->Navigate(origin);
 }
public:
 WebView(){life->self=this;}WebView(const WebView&)=delete;WebView& operator=(const WebView&)=delete;
 ~WebView(){close();}
 HWND container()const{return child;}bool active()const{return shown;}bool pending()const{return starting;}void useClassic(){fallback();}
 void close(){starting=false;shown=false;life->self=nullptr;if(controller)controller->Close();web.reset();controller.reset();if(child)DestroyWindow(child);child=nullptr;
  // WebView2 environment creation can still be pending; retain loader until process exit.
 }
 bool start(HWND window,const std::filesystem::path& folder,std::function<void(std::wstring)> callback,std::function<void(bool)> changed){
  owner=window;assets=std::filesystem::absolute(folder);receive=std::move(callback);visibility=std::move(changed);
  if(!std::filesystem::is_regular_file(assets/L"index.html"))return false;
  auto path=assets.parent_path()/L"WebView2Loader.dll";
  loader=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!loader)return false;
  using Create=HRESULT(STDMETHODCALLTYPE*)(PCWSTR,PCWSTR,ICoreWebView2EnvironmentOptions*,ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
  auto create=reinterpret_cast<Create>(GetProcAddress(loader,"CreateCoreWebView2EnvironmentWithOptions"));if(!create)return false;
  child=CreateWindowExW(0,L"STATIC",L"Interface Vision",WS_CHILD|WS_CLIPCHILDREN,0,0,1,1,window,nullptr,GetModuleHandleW(nullptr),nullptr);if(!child)return false;
  PWSTR local=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&local)))return false;
  auto profile=std::filesystem::path(local)/L"Live Performance by Hugo Sodre"/L"VisionWebView";CoTaskMemFree(local);std::filesystem::create_directories(profile);
  std::weak_ptr<Life> weak=life;
  Com<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> environment;
  environment.reset(handler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,HRESULT,ICoreWebView2Environment*>([weak](HRESULT result,ICoreWebView2Environment* env){
   auto guard=weak.lock();if(!guard||!guard->self)return S_OK;if(FAILED(result)||!env){guard->self->fallback();return S_OK;}
   Com<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> finished;
   finished.reset(handler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,HRESULT,ICoreWebView2Controller*>([weak](HRESULT result,ICoreWebView2Controller* controller){auto guard=weak.lock();if(!guard||!guard->self)return S_OK;if(FAILED(result)||FAILED(guard->self->setup(controller)))guard->self->fallback();return S_OK;}));
   return env->CreateCoreWebView2Controller(guard->self->child,finished.get());
  }));starting=true;HRESULT result=create(nullptr,profile.c_str(),nullptr,environment.get());if(FAILED(result))starting=false;return SUCCEEDED(result);
 }
 void reveal(){starting=false;shown=true;ShowWindow(child,SW_SHOW);if(controller)controller->put_IsVisible(TRUE);resize();if(visibility)visibility(true);if(controller)controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);}
 void resize(){if(!child)return;RECT bounds{};GetClientRect(owner,&bounds);SetWindowPos(child,HWND_TOP,0,0,bounds.right,bounds.bottom,SWP_NOACTIVATE);if(controller){RECT area{0,0,bounds.right,bounds.bottom};controller->put_Bounds(area);}}
 void post(const std::wstring& json){if(web&&shown)web->PostWebMessageAsJson(json.c_str());}
};
}
