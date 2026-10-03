// Compile with MSVC /std:c++17 /EHsc. Isolated native widget contract fixture;
// never loads or modifies a running game or its equipment.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <cmath>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#ifdef VERIFY_NATIVE_WARDROBE_UI
#include <winhttp.h>
#include <future>
#include <chrono>
#include <sstream>
#endif
using Ptr=void*;
uint8_t* fixture_game=nullptr;
HMODULE fixture_module(const wchar_t*){return reinterpret_cast<HMODULE>(fixture_game);}
void log(const char* message){std::cout<<message<<'\n';}
template<typename T>T symbol(HMODULE,const char*){return nullptr;}
void* trampoline(void* p,size_t){return p;}
void detour(void*,void*,size_t){}
size_t to_utf8(Ptr p,char* out,size_t n){auto& s=*static_cast<std::string*>(p);size_t copy=(std::min)(s.size(),n-1);std::memcpy(out,s.data(),copy);out[copy]=0;return copy;}
Ptr from_wide(const wchar_t* value,size_t count){return new std::string(value,value+count);}
void destroy_string(Ptr value){delete static_cast<std::string*>(value);}
#define GetModuleHandleW fixture_module
#include "wardrobe_preview.h"
#undef GetModuleHandleW
#ifdef VERIFY_NATIVE_WARDROBE_UI
#include "wardrobe_native_ui.h"
#endif

struct Widget { std::array<uint8_t,0xa00> bytes{}; std::string name; uint32_t type; std::wstring native_text; };
std::unordered_map<Ptr,Widget*> widgets;
std::array<Ptr,0x500/8> vtable{};
std::unordered_map<Ptr,std::vector<Ptr>> children;
Widget *host,*browser,*paper,*normal,*zoom,*robot;
int resets=0;std::vector<uint32_t> appearances;
const char* __cdecl name(Ptr p){return widgets.at(p)->name.c_str();}
const wchar_t* __cdecl edit_text(Ptr p){return widgets.at(p)->native_text.c_str();}
void __cdecl set_edit_text(Ptr p,const wchar_t* value){widgets.at(p)->native_text=value;}
Ptr __cdecl lookup(Ptr p,const char* n,uint32_t type){for(Ptr c:children[p])if(widgets.at(c)->name==n&&widgets.at(c)->type==type)return c;return nullptr;}
void __cdecl set_rect(Ptr p,const wardrobe::Rect* r){wardrobe::field<wardrobe::Rect>(p,0x50)=*r;}
void __cdecl absolute_rect(Ptr p,wardrobe::Rect* r){*r=wardrobe::rect(p);}
void __cdecl add(Ptr p,Ptr c,int){children[p].push_back(c);}
void __cdecl remove_child(Ptr p,Ptr c){auto& a=children[p];a.erase(std::remove(a.begin(),a.end(),c),a.end());}
void __cdecl set_flag(Ptr p,uint64_t bits,int){
 wardrobe::field<uint64_t>(p,0x30)|=bits;
 // Stock OnVisible restores the saved popup rectangle.
 if(p==paper->bytes.data()&&(bits&1))wardrobe::field<wardrobe::Rect>(p,0x50)={20,140,320,490};
}
void __cdecl clear_flag(Ptr p,uint64_t bits,int){wardrobe::field<uint64_t>(p,0x30)&=~bits;}
void __cdecl reset(Ptr p,bool){assert(p==paper->bytes.data());resets++;appearances.clear();wardrobe::field<uint64_t>(normal->bytes.data(),0x30)|=1;wardrobe::field<int>(p,0x598)=1;}
void __cdecl appearance(Ptr p,const uint32_t* data){assert(p==paper->bytes.data());appearances.push_back(data[0]);}
void __cdecl zoom_change(Ptr p,int value){wardrobe::field<int>(p,0x57c)=value;}
void __cdecl noop(Ptr){}
#ifdef VERIFY_NATIVE_WARDROBE_UI
std::string tooltip_query;
void __cdecl item_tooltip(Ptr,uint32_t,const char* title){tooltip_query=title;}
Ptr fixture_image=nullptr;
Ptr __cdecl state_image(Ptr,int){return fixture_image;}
int uv_updates=0;
void __cdecl uv_refresh(Ptr){++uv_updates;}
#endif
void __cdecl model_reload(Ptr p){assert(p==paper->bytes.data());for(auto w:{normal,zoom,robot})wardrobe::field<Ptr>(w->bytes.data(),0x470)=w;}
void __cdecl model_camera(Ptr p){
 auto& camera=wardrobe::field<std::array<float,3>>(p,0x6e8);
 camera=p==zoom->bytes.data()?std::array<float,3>{-.02f,.8f,.55f}:std::array<float,3>{0,5,0};
 wardrobe::field<std::array<float,3>>(p,0x8bc)=camera;
}
double __cdecl fixture_fmod(double x,double y){return std::fmod(x,y);}
double __cdecl fixture_sqrt(double x){return std::sqrt(x);}
struct Value {std::string text;int number=0;};
size_t __cdecl count(Ptr a){return static_cast<std::vector<Value>*>(a)->size();}
Ptr __cdecl element(Ptr a,size_t i){return &static_cast<std::vector<Value>*>(a)->at(i);}
Ptr __cdecl text(Ptr v){return new std::string(static_cast<Value*>(v)->text);}
int __cdecl integer(Ptr v){return static_cast<Value*>(v)->number;}
std::string callback_url="http://127.0.0.1:8091/market/wardrobe?session_id=fixture",callback_script;
Ptr __cdecl view_url(Ptr){return new std::string(callback_url);}
void __cdecl execute_js(Ptr,Ptr script,Ptr){callback_script=*static_cast<std::string*>(script);}
int journey_key_requests=0;
void __cdecl request_journey_key(){++journey_key_requests;}
void emit(size_t offset,Ptr function){uint8_t jump[12]={0x48,0xb8};std::memcpy(jump+2,&function,8);jump[10]=0xff;jump[11]=0xe0;std::memcpy(fixture_game+offset,jump,sizeof(jump));}
Widget make(const char* n,uint32_t type,uint32_t id,wardrobe::Rect rect,uint64_t flags){Widget w;w.name=n;w.type=type;Ptr vt=vtable.data();std::memcpy(w.bytes.data(),&vt,8);wardrobe::field<uint32_t>(w.bytes.data(),0x340)=id;wardrobe::field<wardrobe::Rect>(w.bytes.data(),0x50)=rect;wardrobe::field<uint64_t>(w.bytes.data(),0x30)=flags;return w;}
int main(int argc,char** argv){
 std::cout<<std::unitbuf;
 AddVectoredExceptionHandler(1,[](EXCEPTION_POINTERS* e)->LONG{
  if(e->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION){std::cerr<<"Native CPU fixture fault at RVA "<<std::hex<<(e->ContextRecord->Rip-reinterpret_cast<uintptr_t>(fixture_game))<<", address "<<e->ExceptionRecord->ExceptionInformation[1]<<"; stack";for(int i=0;i<12;++i)std::cerr<<' '<<(reinterpret_cast<uintptr_t*>(e->ContextRecord->Rsp)[i]-reinterpret_cast<uintptr_t>(fixture_game));std::cerr<<std::dec<<'\n';}
  return EXCEPTION_CONTINUE_SEARCH;
 });
 if(argc!=2){std::cerr<<"Pass the matching original Game.dll path. The camera/rotation checks execute its real CPU math.\n";return 2;}
 std::ifstream binary(argv[1],std::ios::binary);std::vector<uint8_t> original((std::istreambuf_iterator<char>(binary)),{});
 assert(original.size()>0x1400000);
 auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(original.data()+reinterpret_cast<const IMAGE_DOS_HEADER*>(original.data())->e_lfanew);
 assert(nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64);
 fixture_game=static_cast<uint8_t*>(VirtualAlloc(nullptr,nt->OptionalHeader.SizeOfImage,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));assert(fixture_game);
 auto section=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){assert(section[i].PointerToRawData+section[i].SizeOfRawData<=original.size());std::memcpy(fixture_game+section[i].VirtualAddress,original.data()+section[i].PointerToRawData,section[i].SizeOfRawData);}
 assert(fixture_game[0x487480]==0x40 && fixture_game[0x487481]==0x53 && fixture_game[0x1bf0d0]==0x44);
 std::cout<<"Original native CPU functions mapped\n";
 vtable[0xa8/8]=reinterpret_cast<Ptr>(name);vtable[0x338/8]=reinterpret_cast<Ptr>(lookup);vtable[0x1a8/8]=reinterpret_cast<Ptr>(set_rect);
 vtable[0x58/8]=reinterpret_cast<Ptr>(absolute_rect);
 vtable[0x2f8/8]=reinterpret_cast<Ptr>(add);vtable[0x300/8]=reinterpret_cast<Ptr>(remove_child);vtable[0xd0/8]=reinterpret_cast<Ptr>(set_flag);vtable[0xd8/8]=reinterpret_cast<Ptr>(clear_flag);
 auto h=make("PrivateWardrobe",0x2003,0x211,{0,0,1920,1080},1),b=make("PrivateWardrobeBrowser",0x2012,0,{0,28,1920,1052},1),p=make("paper_doll_dialog",0x2003,0x110,{20,140,320,490},0);
 auto n=make("char_model",0x2000,0,{1,0,318,426},1),z=make("char_model_zoomin",0x2000,0,{1,0,318,426},0),r=make("robot_model",0x2000,0,{1,0,318,426},0);
 host=&h;browser=&b;paper=&p;normal=&n;zoom=&z;robot=&r;
 for(Widget* w:{host,browser,paper,normal,zoom,robot})widgets[w->bytes.data()]=w;
 children[h.bytes.data()]={b.bytes.data()};children[p.bytes.data()]={n.bytes.data(),z.bytes.data(),r.bytes.data()};
 for(Widget* w:{normal,zoom,robot})wardrobe::field<Ptr>(w->bytes.data(),0x290)=p.bytes.data();
 for(Widget* w:{normal,zoom,robot}){
  wardrobe::field<std::array<float,3>>(w->bytes.data(),0x6b8+0x64)={1,1,1};
  wardrobe::field<float>(w->bytes.data(),0x6b8+0x60)=0.6f;
  wardrobe::field<float>(w->bytes.data(),0x6b8+0x78)=1.32f;
  wardrobe::field<float>(w->bytes.data(),0x6b8+0x80)=.25f;
  wardrobe::field<float>(w->bytes.data(),0x6b8+0x98)=1024;
  wardrobe::field<std::array<float,3>>(w->bytes.data(),0x6e8)={0,0,0}; // unopened native view
 }
 for(size_t off:{size_t(0x528),size_t(0x530),size_t(0x538)})wardrobe::field<Ptr>(p.bytes.data(),off)=off==0x528?n.bytes.data():off==0x530?z.bytes.data():r.bytes.data();
 *reinterpret_cast<Ptr*>(fixture_game+0x13875c0+0x211*8)=h.bytes.data();*reinterpret_cast<Ptr*>(fixture_game+0x13875c0+0x110*8)=p.bytes.data();
 emit(0x8053f0,reinterpret_cast<Ptr>(reset));emit(0x804bd0,reinterpret_cast<Ptr>(appearance));emit(0x805bd0,reinterpret_cast<Ptr>(zoom_change));emit(0x805cc0,reinterpret_cast<Ptr>(noop));emit(0x805d40,reinterpret_cast<Ptr>(noop));
 emit(0x8046f0,reinterpret_cast<Ptr>(model_reload));emit(0x48e340,reinterpret_cast<Ptr>(model_camera));
 // Only imported CRT math is substituted; camera/frustum and yaw code is the
 // original client machine code. Import thunks are six bytes apart.
 *reinterpret_cast<Ptr*>(fixture_game+0xb7fcb0)=reinterpret_cast<Ptr>(fixture_sqrt);
 *reinterpret_cast<Ptr*>(fixture_game+0xb80348)=reinterpret_cast<Ptr>(fixture_fmod);
 wardrobe::array_size=count;wardrobe::array_element=element;wardrobe::value_string=text;wardrobe::value_integer=integer;wardrobe::view_url=view_url;wardrobe::execute_js=execute_js;
 wardrobe::tick();
 std::vector<Value> args={{"114100001,110100001"},{"",700},{"",150},{"",700},{"",550},{"",1}};
 wardrobe::Command command;assert(wardrobe::parse_preview(&args,command));
 assert(wardrobe::preview(command));assert(resets==1&&appearances==std::vector<uint32_t>({114100001,110100001}));
 for(Widget* w:{normal,zoom,robot}){assert(wardrobe::field<Ptr>(w->bytes.data(),0x290)==h.bytes.data());auto rr=wardrobe::rect(w->bytes.data());assert(rr.x==700&&rr.y==178&&rr.w==700&&rr.h==550);}
 assert(wardrobe::rect(p.bytes.data()).x==-10000);
 wardrobe::control("visible",0);assert(!(wardrobe::field<uint64_t>(n.bytes.data(),0x30)&1));wardrobe::control("visible",1);assert(wardrobe::field<uint64_t>(n.bytes.data(),0x30)&1);assert(!(wardrobe::field<uint64_t>(z.bytes.data(),0x30)&1));
 bool off_thread=true;std::thread thread([&]{off_thread=wardrobe::preview(command);});thread.join();assert(!off_thread&&resets==1);
 args[0].text="bad";assert(!wardrobe::parse_preview(&args,command)&&resets==1);args[0].text="114100001";assert(wardrobe::parse_preview(&args,command));
 wardrobe::visible(h.bytes.data(),0);assert(!wardrobe::paper);
 for(Widget* w:{normal,zoom,robot}){assert(wardrobe::field<Ptr>(w->bytes.data(),0x290)==p.bytes.data());assert(wardrobe::rect(w->bytes.data()).w==318);}
 assert(!(wardrobe::field<uint64_t>(p.bytes.data(),0x30)&1)&&wardrobe::rect(p.bytes.data()).x==20);
 assert(wardrobe::preview(command)&&resets==2&&appearances.size()==1);
 std::string object="AionObject",method="WardrobePreview";
 // Actual browser callbacks execute on a worker; no native model access until
 // the game event pump consumes the independent argument copies.
 std::thread worker([&]{wardrobe::callback(nullptr,&object,&method,&args);});worker.join();assert(resets==2&&callback_script.empty());
 args[0].text="110100001";wardrobe::tick();assert(resets==3&&appearances[0]==114100001);
 std::string poll="WardrobePoll";std::vector<Value> empty;
 std::thread poller([&]{wardrobe::callback(nullptr,&object,&poll,&empty);});poller.join();assert(callback_script.find("State(true,1)")!=std::string::npos);
 args[0].text="bad";args[5].number=2;wardrobe::callback(nullptr,&object,&method,&args);wardrobe::tick();wardrobe::callback(nullptr,&object,&poll,&empty);assert(resets==3&&callback_script.find("State(false,2)")!=std::string::npos);
 callback_script.clear();callback_url="https://example.invalid/market/wardrobe";wardrobe::callback(nullptr,&object,&method,&args);assert(resets==3&&callback_script.empty());
 callback_url="http://127.0.0.1:8091/market/wardrobe";args[0].text="114100001";
 for(int i=3;i<=500;++i){args[5].number=i;wardrobe::callback(nullptr,&object,&method,&args);}
 assert(wardrobe::commands.size()==1);wardrobe::tick();assert(resets==4);
 wardrobe::callback(nullptr,&object,&poll,&empty);assert(callback_script.find("State(true,500)")!=std::string::npos);
 std::string ctl="WardrobeControl";std::vector<Value> rotation={{"left"},{"",1}},release={{"left"},{"",0}};
 wardrobe::callback(nullptr,&object,&ctl,&rotation);wardrobe::callback(nullptr,&object,&ctl,&release);wardrobe::tick();assert(wardrobe::field<int>(p.bytes.data(),0x570)==0);
 args[5].number=501;wardrobe::callback(nullptr,&object,&method,&args);wardrobe::destroy(nullptr);wardrobe::tick();assert(resets==4); // view destroyed while request queued
 wardrobe::field<int>(p.bytes.data(),0x57c)=1;wardrobe::control("wings",1);
 assert(wardrobe::field<int>(p.bytes.data(),0x578)==2 && wardrobe::field<int>(p.bytes.data(),0x57c)==0 && wardrobe::field<int>(p.bytes.data(),0x598)==1);
 wardrobe::control("wings",0);assert(wardrobe::field<int>(p.bytes.data(),0x578)==0);
 wardrobe::callback(nullptr,&object,&method,&args);wardrobe::tick();assert(resets==5); // reused address has a fresh state
 std::string stock="ItemPreview";callback_url="http://127.0.0.1:8091/shop";wardrobe::callback(nullptr,&object,&stock,&args);wardrobe::tick();assert(!wardrobe::paper);
 callback_url="http://127.0.0.1:8091/market/wardrobe";
 std::vector<Value> modal={{"visible"},{"",0}};
 wardrobe::callback(nullptr,&object,&ctl,&modal);wardrobe::callback(nullptr,&object,&method,&args);wardrobe::tick();assert(wardrobe::modal_hidden && !(wardrobe::field<uint64_t>(n.bytes.data(),0x30)&1));
 wardrobe::callback(nullptr,&object,&method,&args);wardrobe::tick();assert(!(wardrobe::field<uint64_t>(n.bytes.data(),0x30)&1)); // native reset cannot draw over a confirmation
 modal[1].number=1;wardrobe::callback(nullptr,&object,&ctl,&modal);wardrobe::tick();assert(wardrobe::field<uint64_t>(n.bytes.data(),0x30)&1);
 assert(wardrobe::preview(command));
 wardrobe::tick();assert(wardrobe::moved[0].camera_ready && wardrobe::moved[0].base_camera[1]==5);
 // Real hidden Win32 window messages exercise hit testing, capture/release,
 // screen-to-client wheel coordinates and game-thread camera application.
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"WardrobeMouseFixture";assert(RegisterClassW(&wc));
 HWND hwnd=CreateWindowW(wc.lpszClassName,L"fixture",WS_POPUP,80,90,1920,1080,nullptr,nullptr,wc.hInstance,nullptr);assert(hwnd);
 wardrobe::mouse::window=hwnd;wardrobe::mouse::prior=DefWindowProcW;SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(wardrobe::mouse::receive));wardrobe::tick();
 SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(800,300));SendMessageW(hwnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(900,300));SendMessageW(hwnd,WM_LBUTTONUP,0,MAKELPARAM(900,300));
 assert(wardrobe::field<float>(n.bytes.data(),0x65c)==0);wardrobe::tick();assert(wardrobe::field<float>(n.bytes.data(),0x65c)==45 && wardrobe::mouse::dragging==0);
 wardrobe::mouse::apply(-200,0,0,0);assert(wardrobe::field<float>(n.bytes.data(),0x65c)==315);
 wardrobe::mouse::apply(200,0,0,0);assert(wardrobe::field<float>(n.bytes.data(),0x65c)==45);
 POINT wheelPoint{900,300};ClientToScreen(hwnd,&wheelPoint);
 auto beforeFrustum=wardrobe::field<std::array<float,64>>(n.bytes.data(),0x6b8+0x100);
 SendMessageW(hwnd,WM_MOUSEWHEEL,MAKEWPARAM(0,120),MAKELPARAM(wheelPoint.x,wheelPoint.y));wardrobe::tick();assert(wardrobe::field<float>(n.bytes.data(),0x6ec)<5);
 assert((wardrobe::field<std::array<float,64>>(n.bytes.data(),0x6b8+0x100)!=beforeFrustum)); // real camera planes, not just XYZ
 SendMessageW(hwnd,WM_MOUSEWHEEL,MAKEWPARAM(0,-120),MAKELPARAM(wheelPoint.x,wheelPoint.y));wardrobe::tick();assert(std::abs(wardrobe::field<float>(n.bytes.data(),0x6ec)-5)<0.001);
 SendMessageW(hwnd,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(800,300));SendMessageW(hwnd,WM_MOUSEMOVE,MK_RBUTTON,MAKELPARAM(900,350));SendMessageW(hwnd,WM_RBUTTONUP,0,MAKELPARAM(900,350));wardrobe::tick();assert(wardrobe::field<float>(n.bytes.data(),0x6e8)<0 && wardrobe::field<float>(n.bytes.data(),0x6f0)>0);
 auto cameraAfterDrag=wardrobe::field<std::array<float,3>>(n.bytes.data(),0x6e8);float headingAfterDrag=wardrobe::field<float>(n.bytes.data(),0x65c);
 wardrobe::field<float>(n.bytes.data(),0x65c)=90;wardrobe::tick();assert(wardrobe::moved[0].preview_angle[2]==90);
 headingAfterDrag=90; // Stock Left/Right changes must survive the next Try On.
 wardrobe::preview(command);wardrobe::tick();assert((wardrobe::field<std::array<float,3>>(n.bytes.data(),0x6e8)==cameraAfterDrag) && wardrobe::field<float>(n.bytes.data(),0x65c)==headingAfterDrag);
 wardrobe::field<std::array<float,3>>(n.bytes.data(),0x6e8)={0,0,0};wardrobe::mouse::retry_after=0;wardrobe::tick();assert((wardrobe::field<std::array<float,3>>(n.bytes.data(),0x6e8)==cameraAfterDrag)); // camera invalidated by zone transition
 POINT outside{50,50};ClientToScreen(hwnd,&outside);float previous=wardrobe::mouse::scale;SendMessageW(hwnd,WM_MOUSEWHEEL,MAKEWPARAM(0,120),MAKELPARAM(outside.x,outside.y));wardrobe::tick();assert(wardrobe::mouse::scale==previous);
 // Native confirmations block interaction while preserving the model and camera.
 const auto modelFlags=wardrobe::field<uint64_t>(n.bytes.data(),0x30);
 const auto modalCamera=wardrobe::field<std::array<float,3>>(n.bytes.data(),0x6e8);
 wardrobe::mouse::rotation=9;wardrobe::mouse::wheel=1;wardrobe::field<int>(p.bytes.data(),0x570)=1;
 wardrobe::control("input",0);wardrobe::tick();
 assert(wardrobe::mouse::input_blocked && wardrobe::mouse::rotation==0 && wardrobe::mouse::wheel==0 && wardrobe::field<int>(p.bytes.data(),0x570)==0);
 SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(800,300));
 SendMessageW(hwnd,WM_MOUSEWHEEL,MAKEWPARAM(0,120),MAKELPARAM(wheelPoint.x,wheelPoint.y));wardrobe::tick();
 assert((wardrobe::mouse::dragging==0 && wardrobe::field<uint64_t>(n.bytes.data(),0x30)==modelFlags && wardrobe::field<std::array<float,3>>(n.bytes.data(),0x6e8)==modalCamera));
 wardrobe::control("input",1);assert(!wardrobe::mouse::input_blocked);
 wardrobe::control("visible",0);wardrobe::tick();SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(800,300));assert(wardrobe::mouse::dragging==0);wardrobe::control("visible",1);
 wardrobe::mouse::apply(0,0,0,100);assert(wardrobe::mouse::scale==0.35f);wardrobe::mouse::apply(0,0,0,-100);assert(wardrobe::mouse::scale==2.5f);wardrobe::control("camera-reset",1);assert(wardrobe::mouse::scale==1 && wardrobe::field<float>(n.bytes.data(),0x6ec)==5 && wardrobe::field<float>(n.bytes.data(),0x65c)==0);
 DestroyWindow(hwnd);assert(!wardrobe::mouse::window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
 wardrobe::visible(p.bytes.data(),-1);assert(!wardrobe::paper);
 // The journey browser fills the viewport on the UI thread, including resize.
 auto journey=make("PrivateJourney",0x2003,0x212,{5,5,1280,960},0),journeyBrowser=make("PrivateJourneyBrowser",0x2012,0,{0,0,1280,960},1);
 widgets[journey.bytes.data()]=&journey;widgets[journeyBrowser.bytes.data()]=&journeyBrowser;
 children[journey.bytes.data()]={journeyBrowser.bytes.data()};
 *reinterpret_cast<Ptr*>(fixture_game+0x13875c0+0x212*8)=journey.bytes.data();
 *reinterpret_cast<double*>(fixture_game+0x1378ea8)=1920;*reinterpret_cast<double*>(fixture_game+0x1378eb0)=1080;
 callback_url="http://127.0.0.1:8091/journey?session_id=fixture";
 std::string sessionMethod="JourneySession";std::vector<Value> sessionArgs;
 std::memset(fixture_game+0x130c8f0,0,16);callback_script.clear();
 emit(0x3321c0,reinterpret_cast<Ptr>(request_journey_key));
 wardrobe::callback(nullptr,&object,&sessionMethod,&sessionArgs);
 assert(callback_script.empty());
 wardrobe::callback(nullptr,&object,&sessionMethod,&sessionArgs);
 assert(journey_key_requests==0);wardrobe::tick();assert(journey_key_requests==1);
 wardrobe::callback(nullptr,&object,&sessionMethod,&sessionArgs);wardrobe::tick();assert(journey_key_requests==1);
 for(unsigned i=0;i<16;++i)fixture_game[0x130c8f0+i]=uint8_t(i);
 wardrobe::callback(nullptr,&object,&sessionMethod,&sessionArgs);
 assert(callback_script=="window.JourneySessionReady&&window.JourneySessionReady('000102030405060708090a0b0c0d0e0f')");
 std::memset(fixture_game+0x130c8f0,0,16);
 std::thread sessionWorker([&]{wardrobe::callback(nullptr,&object,&sessionMethod,&sessionArgs);});sessionWorker.join();
 assert(journey_key_requests==1);wardrobe::tick();assert(journey_key_requests==2);
 for(unsigned i=0;i<16;++i)fixture_game[0x130c8f0+i]=uint8_t(i);
 for(const char* refused:{"https://example.invalid/journey","http://127.0.0.1:8092/journey","http://127.0.0.1:8091/journey-extra","http://127.0.0.1:8091/market"}){
  callback_url=refused;callback_script.clear();wardrobe::callback(nullptr,&object,&sessionMethod,&sessionArgs);assert(callback_script.empty());
 }
 callback_url="http://127.0.0.1:8091/journey";
 sessionArgs.push_back({"unexpected",0});callback_script.clear();wardrobe::callback(nullptr,&object,&sessionMethod,&sessionArgs);assert(callback_script.empty());
 callback_url="http://127.0.0.1:8091/journey?session_id=fixture";
 std::cout<<"PASS: Journey session is unavailable without a key and restricted to the exact local Journey page\n";
 std::string journeyMethod="JourneyVisibility";std::vector<Value> journeyArgs={{"",1}};
 std::thread journeyWorker([&]{wardrobe::callback(nullptr,&object,&journeyMethod,&journeyArgs);});journeyWorker.join();
 assert(!(wardrobe::field<uint64_t>(journey.bytes.data(),0x30)&1));wardrobe::tick();
 assert(wardrobe::field<uint64_t>(journey.bytes.data(),0x30)&1);
 for(auto w:{&journey,&journeyBrowser}){auto rr=wardrobe::rect(w->bytes.data());assert(rr.x==0 && rr.y==0 && rr.w==1920 && rr.h==1080);}
 *reinterpret_cast<double*>(fixture_game+0x1378ea8)=3440;*reinterpret_cast<double*>(fixture_game+0x1378eb0)=1440;wardrobe::tick();
 assert(wardrobe::rect(journey.bytes.data()).w==3440 && wardrobe::rect(journeyBrowser.bytes.data()).h==1440);
 journeyArgs[0].number=0;wardrobe::callback(nullptr,&object,&journeyMethod,&journeyArgs);wardrobe::tick();assert(!(wardrobe::field<uint64_t>(journey.bytes.data(),0x30)&1));
 callback_url="https://example.invalid/journey";journeyArgs[0].number=1;wardrobe::callback(nullptr,&object,&journeyMethod,&journeyArgs);wardrobe::tick();assert(!(wardrobe::field<uint64_t>(journey.bytes.data(),0x30)&1));
 callback_url="http://127.0.0.1:8091/journey";wardrobe::callback(nullptr,&object,&journeyMethod,&journeyArgs);wardrobe::destroy(nullptr);wardrobe::tick();assert(!(wardrobe::field<uint64_t>(journey.bytes.data(),0x30)&1));
 std::cout<<"PASS: journey local-page and destroyed-view guards; game-thread fullscreen show/hide and viewport resize\n";
#ifdef VERIFY_NATIVE_WARDROBE_UI
 // The same original paper-doll now belongs to a native addon without Browser.
 emit(0x12e7e0,reinterpret_cast<Ptr>(item_tooltip));
 vtable[0x298/8]=reinterpret_cast<Ptr>(edit_text);vtable[0x290/8]=reinterpret_cast<Ptr>(set_edit_text);
 std::vector<Widget> mailboxes;mailboxes.reserve(14);
 for(const char* id:{"WardrobeNativeTx","WardrobeNativeRx","WardrobeNativeMeta","WardrobeNativePreview","WardrobeNativeControl","WardrobeNativeHover","WardrobePreviewBounds"})mailboxes.push_back(make(id,0x200b,0,{100,180,400,650},0));
 for(int i=1;i<8;++i)mailboxes.push_back(make(("WardrobeNativeRx"+std::to_string(i)).c_str(),0x200b,0,{0,0,1,1},0));
 for(auto& box:mailboxes){widgets[box.bytes.data()]=&box;children[h.bytes.data()].push_back(box.bytes.data());}
 auto confirmation=make("WardrobeConfirm",0x2003,0x213,{1301.25,438.75,517.5,337.5},0);
 widgets[confirmation.bytes.data()]=&confirmation;
 *reinterpret_cast<Ptr*>(fixture_game+0x13875c0+0x213*8)=confirmation.bytes.data();
 auto& kids=children[h.bytes.data()];kids.erase(std::remove(kids.begin(),kids.end(),b.bytes.data()),kids.end());
 wardrobe::field<uint64_t>(h.bytes.data(),0x30)|=1;
 fixture_game[0x130c8f0]=1;
 *reinterpret_cast<double*>(fixture_game+0x1378ea8)=3440;*reinterpret_cast<double*>(fixture_game+0x1378eb0)=1440;
 wardrobe::tick();
 assert(wardrobe::native_ui::active==h.bytes.data());assert(wardrobe::native_ui::token().size()==32);
 assert(wardrobe::rect(h.bytes.data()).w==3440 && wardrobe::rect(h.bytes.data()).h==1440);
 auto modal_rect=wardrobe::rect(confirmation.bytes.data());assert(modal_rect.x==1490 && modal_rect.y==570 && modal_rect.w==460 && modal_rect.h==300);
 assert(mailboxes[2].native_text.find(L"3440|1440|")==0);
 // Execute the publisher's source-rectangle getter/setter, including UV refresh.
 std::array<uint8_t,0x70> image{};std::array<Ptr,0xc0/8> image_vtable{};
 image_vtable[0x30/8]=fixture_game+0x4d8a20;image_vtable[0x38/8]=fixture_game+0x4d8a50;image_vtable[0xb0/8]=reinterpret_cast<Ptr>(uv_refresh);
 wardrobe::field<Ptr>(image.data(),0)=image_vtable.data();wardrobe::field<wardrobe::Rect>(image.data(),0x28)={0,0,64,64};
 fixture_image=image.data();vtable[0x3c0/8]=reinterpret_cast<Ptr>(state_image);
 auto icon=make("WardrobeIcon1",0x2021,0,{0,0,40,40},1);
 wardrobe::native_ui::crop_icon(icon.bytes.data(),40);
 auto source=wardrobe::field<wardrobe::Rect>(image.data(),0x28);assert(source.w==40 && source.h==40 && uv_updates==1);
 wardrobe::native_ui::crop_icon(icon.bytes.data(),40);assert(uv_updates==1);
 wardrobe::native_ui::crop_icon(icon.bytes.data(),64);assert(wardrobe::field<wardrobe::Rect>(image.data(),0x28).w==64); // switching to full-size art must undo the preceding crop
 // Real addon SetUIImage leaves a zero rectangle meaning the entire texture.
 for(unsigned side:{40u,64u}){
  wardrobe::field<wardrobe::Rect>(image.data(),0x28)={0,0,0,0};int prior=uv_updates;
  wardrobe::native_ui::crop_icon(icon.bytes.data(),side);
  source=wardrobe::field<wardrobe::Rect>(image.data(),0x28);assert(source.x==0 && source.y==0 && source.w==side && source.h==side && uv_updates==prior+1);
  wardrobe::native_ui::crop_icon(icon.bytes.data(),side);assert(uv_updates==prior+1);
 }
 fixture_image=nullptr;wardrobe::native_ui::crop_icon(icon.bytes.data(),40);
 for(const auto& comboSize:std::vector<wardrobe::Rect>{{0,0,147.375,27},{0,0,421.875,27},{0,0,196.5,36},{0,0,562.5,36}}){
  auto combo=make("WardrobeFilter",0x2006,0,comboSize,1),arrow=make("drop_btn",0x2001,0,{-26.578125,-13.359375,26.578125,27.84375},1);
  widgets[combo.bytes.data()]=&combo;widgets[arrow.bytes.data()]=&arrow;children[combo.bytes.data()]={arrow.bytes.data()};
  wardrobe::native_ui::layout_combo(combo.bytes.data());auto ar=wardrobe::rect(arrow.bytes.data());
  assert(ar.x==comboSize.w-ar.w && ar.y==0 && ar.h==comboSize.h && ar.w>0 && ar.x>=0);
  children.erase(combo.bytes.data());widgets.erase(combo.bytes.data());widgets.erase(arrow.bytes.data());
 }
 auto slot=make("WardrobeSlot1",0x2001,0,{24,114,44,44},1),row=make("WardrobeRow1",0x2001,0,{650,200,300,62},1),scroll=make("WardrobeScroll",0x201d,0,{648,222,1249,628},1);
 wardrobe::native_ui::hover_widgets={{slot.bytes.data(),"WardrobeSlot1",false},{row.bytes.data(),"WardrobeRow1",true}};wardrobe::native_ui::scroll=scroll.bytes.data();
 assert(wardrobe::native_ui::hit({30,120},true,false)=="WardrobeSlot1");
 assert(wardrobe::native_ui::hit({660,235},true,false)=="WardrobeRow1");
 assert(wardrobe::native_ui::hit({660,210},true,false).empty()); // clipped scroll row
 assert(wardrobe::native_ui::hit({30,120},false,false).empty());
 assert(wardrobe::native_ui::hit({30,120},true,true).empty());
 wardrobe::field<uint64_t>(slot.bytes.data(),0x30)=0;assert(wardrobe::native_ui::hit({30,120},true,false).empty());
 wardrobe::native_ui::tooltip(h.bytes.data(),"item=125000002&count=1");assert(tooltip_query=="nc://aion.ItemInfo/ItemTooltip?item=125000002&count=1");
 wardrobe::native_ui::tooltip(h.bytes.data(),"");assert(tooltip_query.empty());
 assert(!wardrobe::native_ui::icon_name("NativeTx") && !wardrobe::native_ui::icon_name("Icon37") && wardrobe::native_ui::icon_name("SlotIcon32768"));
 std::cout<<"PASS: original image UV setter crops zero-source and padded DDS sprites; full-size art retained; dropdown arrows stay inside both filter widths; clipped rows, hidden slots, modal/focus guards and native item tooltip dispatch\n";
 // Every message must fit the actual 1,023-byte stock text accessor.
 // Eight acknowledged channels reassemble a complete response with UTF-8.
 std::string payload="{\"name\":\"";payload+=std::string(788,'A');payload+="\xce\xa9\xf0\x9f\x98\x80";payload+=std::string(18000,'B');payload+="\"}";
 wardrobe::native_ui::response_body=payload;wardrobe::native_ui::response_sequence=77;wardrobe::native_ui::response_offset=0;
 std::string assembled;size_t batches=0;
 while(!wardrobe::native_ui::response_body.empty()){
  wardrobe::native_ui::deliver_response();
  const auto held=mailboxes[1].native_text;wardrobe::native_ui::deliver_response();assert(mailboxes[1].native_text==held);
  ++batches;
  for(size_t i=0;i<8;++i){auto& box=mailboxes[i?i+6:1];auto packet=wardrobe::native_ui::read(box.bytes.data(),1023);if(packet.empty())continue;
   assert(packet.size()<1023);const auto newline=packet.find('\n');assert(newline!=std::string::npos);
   assert(packet.rfind("77|"+std::to_string(assembled.size())+"|",0)==0);assembled+=packet.substr(newline+1);box.native_text.clear();
  }
 }
 assert(batches==3 && assembled==payload);
 std::cout<<"PASS: complete 18 KB UTF-8 response, stock 1 KB limit, eight-channel acknowledgments and three-frame delivery\n";
 mailboxes[3].native_text=L"42|100000096,125000001,187000001|1";wardrobe::tick();
 assert(wardrobe::native_ui::preview_sequence==42 && wardrobe::native_ui::preview_ready);
 assert(appearances.size()==3 && appearances.back()==187000001);
 assert(wardrobe::rect(normal->bytes.data()).x==100 && wardrobe::rect(normal->bytes.data()).y==180);
 mailboxes[3].native_text=L"43|7|1";wardrobe::tick();assert(!wardrobe::native_ui::preview_ready);
 std::promise<wardrobe::native_ui::Result> delayed;
 wardrobe::native_ui::pending=delayed.get_future();auto epoch=wardrobe::native_ui::generation;
 wardrobe::field<uint64_t>(h.bytes.data(),0x30)&=~uint64_t(1);wardrobe::tick();
 delayed.set_value({epoch,99,"{\"notice\":\"stale\"}"});wardrobe::tick();assert(mailboxes[1].native_text.empty());
 assert(!wardrobe::native_ui::active && !wardrobe::native_ui::pending.valid());
 for(const auto& invalid:{"-1","2147483648","1x"}){bool refused=false;try{wardrobe::native_ui::number(invalid);}catch(...){refused=true;}assert(refused);}
 auto started=std::chrono::steady_clock::now();auto http_worker=std::async(std::launch::async,wardrobe::native_ui::request,1,77,false,"filter=unlocked","fixture-invalid");
 assert(std::chrono::steady_clock::now()-started<std::chrono::milliseconds(100));
 auto response=http_worker.get();assert(response.generation==1 && response.sequence==77 && !response.body.empty());
 std::cout<<"PASS: native-only host, mailbox virtual methods, fullscreen dimensions, native preview anchor, multi-item/wings preview, invalid input, stale completion discard, asynchronous loopback request\n";
#endif
 VirtualFree(fixture_game,0,MEM_RELEASE);
 std::cout<<"PASS: unopened cameras initialize before input; original Game.dll rotation and camera/frustum CPU math; Win32 drag/wheel; appearance and invalid-camera recovery; bounds/zoom limits; modal blocking; native ownership and queued-thread guards\n";
}
