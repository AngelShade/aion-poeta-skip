// Native addon widgets exchange bounded messages with this UI-thread bridge.
// HTTP runs on a worker; that worker never touches Game.dll, widgets or Lua.
#pragma comment(lib,"winhttp.lib")
namespace wardrobe { namespace native_ui {
struct Result { unsigned generation,sequence; std::string body; };
std::future<Result> pending;
Ptr active=nullptr;
unsigned generation=0,preview_sequence=0;
bool preview_ready=false,verified=false;
ULONGLONG auth_retry=0;
std::wstring previous_meta;
std::string response_body;
unsigned response_sequence=0;
size_t response_offset=0;
constexpr size_t response_channels=8,chunk_bytes=800;
struct Icon { Ptr widget; unsigned item; };
struct Hover { Ptr widget; std::string name; bool row; };
std::vector<Icon> icon_widgets;
std::vector<Hover> hover_widgets;
Ptr scroll=nullptr;
ULONGLONG visual_poll=0;
std::string hover_name;
unsigned (*icon_side)(unsigned)=nullptr;
const unsigned slot_masks[]={1,2,4,2048,8,16,4096,32768,32};
bool icon_name(const std::string& name){
    for(auto mask:slot_masks)if(name=="SlotIcon"+std::to_string(mask))return true;
    for(int i=1;i<=36;++i)if(name=="Icon"+std::to_string(i))return true;
    return false;
}
Rect absolute(Ptr widget){Rect result{};method<void(__cdecl*)(Ptr,Rect*)>(widget,0x58)(widget,&result);return result;}
bool contains(const Rect& r,POINT p){return r.w>0 && r.h>0 && p.x>=r.x && p.y>=r.y && p.x<r.x+r.w && p.y<r.y+r.h;}
std::string hit(POINT cursor,bool foreground,bool modal){
    if(!foreground || modal)return {};
    for(const auto& candidate:hover_widgets){
        if(!(field<uint64_t>(candidate.widget,0x30)&1))continue;
        if(candidate.row && (!scroll || !contains(absolute(scroll),cursor)))continue;
        if(contains(absolute(candidate.widget),cursor))return candidate.name;
    }
    return {};
}
void crop_icon(Ptr widget,unsigned side){
    // Addon SetSelectionRect changes the highlight rectangle, not texture UVs.
    // UIStaticImage's state image owns the source rectangle and UV refresh.
    if(!widget || (side!=40 && side!=64))return;
    Ptr image=method<Ptr(__cdecl*)(Ptr,int)>(widget,0x3c0)(widget,0);
    if(!image)return;
    const Rect* source=method<const Rect*(__cdecl*)(Ptr,int)>(image,0x38)(image,0);
    if(!source)return;
    // The live client stores 0 x 0 for the default full DDS source. Treat that
    // as uncropped, not as an image that has not loaded.
    const Rect desired{0,0,double(side),double(side)};
    if(source->x!=desired.x || source->y!=desired.y || source->w!=desired.w || source->h!=desired.h)
        method<void(__cdecl*)(Ptr,int,const Rect*)>(image,0x30)(image,0,&desired);
}
void layout_combo(Ptr combo){
    if(!combo)return;
    Ptr arrow=lookup(combo,"drop_btn",0x2001);if(!arrow)return;
    auto parent=rect(combo);if(parent.w<24 || parent.h<16)return;
    const double side=(std::min)(parent.h,32.0);
    const Rect desired{parent.w-side,0,side,parent.h};auto old=rect(arrow);
    if(old.x!=desired.x || old.y!=desired.y || old.w!=desired.w || old.h!=desired.h)rect(arrow,desired);
}
void tooltip(Ptr dialog,const std::string& query){
    const auto title=query.empty()?std::string():"nc://aion.ItemInfo/ItemTooltip?"+query;
    reinterpret_cast<void(__cdecl*)(Ptr,uint32_t,const char*)>(game()+0x12e7e0)
        (game()+0x130b850,field<uint32_t>(dialog,0x340),title.c_str());
}
std::string response_name(size_t index){return index?"WardrobeNativeRx"+std::to_string(index):"WardrobeNativeRx";}
std::string read(Ptr widget,size_t limit=8192){
    if(!widget)return {};
    const wchar_t* text=method<const wchar_t*(__cdecl*)(Ptr)>(widget,0x298)(widget);
    if(!text)return {};
    size_t n=0;while(n<=limit && text[n])++n;if(n>limit)return {};
    int bytes=WideCharToMultiByte(CP_UTF8,0,text,int(n),nullptr,0,nullptr,nullptr);
    std::string out(bytes,0);if(bytes)WideCharToMultiByte(CP_UTF8,0,text,int(n),out.data(),bytes,nullptr,nullptr);return out;
}
void write(Ptr widget,const std::wstring& text){
    if(widget){field<int>(widget,0x40c)=1048576;method<void(__cdecl*)(Ptr,const wchar_t*)>(widget,0x290)(widget,text.c_str());}
}
std::wstring wide(const std::string& text){
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);
    if(!n && !text.empty())return L"{\"error\":\"Wardrobe returned invalid text.\"}";
    std::wstring out(n,0);if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),out.data(),n);return out;
}
// GetWidgetText converts at most 1,023 UTF-8 bytes in this client. Use eight
// small, acknowledged channels instead of patching Lua's stack or text API.
// UTF-8 boundaries are preserved; all channel reads remain ordinary addon APIs.
void deliver_response(){
    if(response_body.empty() || !active)return;
    std::array<Ptr,response_channels> channels{};
    for(size_t i=0;i<channels.size();++i){
        channels[i]=lookup(active,response_name(i).c_str(),0x200b);
        if(!channels[i] || !read(channels[i],1023).empty())return;
    }
    for(auto channel:channels){
        size_t end=(std::min)(response_offset+chunk_bytes,response_body.size());
        while(end<response_body.size() && (uint8_t(response_body[end])&0xc0)==0x80)--end;
        const bool final=end==response_body.size();
        std::string packet=std::to_string(response_sequence)+"|"+std::to_string(response_offset)+"|"+(final?"1":"0")+"\n";
        packet+=response_body.substr(response_offset,end-response_offset);
        write(channel,wide(packet));response_offset=end;
        if(final){response_body.clear();response_offset=0;break;}
    }
}
std::vector<std::string> split(const std::string& text,char separator){
    std::vector<std::string> out;size_t start=0;
    for(;;){size_t end=text.find(separator,start);out.push_back(text.substr(start,end-start));if(end==std::string::npos)break;start=end+1;}return out;
}
unsigned number(const std::string& text,unsigned max=2147483647){
    if(text.empty() || text.size()>10)throw std::runtime_error("Invalid native Wardrobe number");
    unsigned long long n=0;for(char c:text){if(c<'0'||c>'9')throw std::runtime_error("Invalid number");n=n*10+c-'0';if(n>max)throw std::runtime_error("Number exceeds limit");}return unsigned(n);
}
std::string token(){
    auto p=game()+0x130c8f0;bool present=false;std::string out;constexpr char hex[]="0123456789abcdef";
    for(int i=0;i<16;++i){present|=p[i]!=0;out+=hex[p[i]>>4];out+=hex[p[i]&15];}return present?out:"";
}
struct Internet { HINTERNET handle=nullptr; ~Internet(){if(handle)WinHttpCloseHandle(handle);} };
Result request(unsigned epoch,unsigned sequence,bool post,std::string params,std::string session){
    Result result{epoch,sequence,"{\"error\":\"Wardrobe could not connect. Refresh to retry.\"}"};
    try{
        params+="&session_id="+session;
        Internet transport{WinHttpOpen(L"Aion Native Wardrobe/1",WINHTTP_ACCESS_TYPE_NO_PROXY,nullptr,nullptr,0)};
        if(!transport.handle)return result;
        WinHttpSetTimeouts(transport.handle,3000,3000,6000,12000);
        Internet connection{WinHttpConnect(transport.handle,L"127.0.0.1",8091,0)};if(!connection.handle)return result;
        std::wstring path=L"/market/wardrobe/";path+=post?L"action":L"state?"+wide(params);
        Internet query{WinHttpOpenRequest(connection.handle,post?L"POST":L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0)};
        if(!query.handle)return result;
        DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(query.handle,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
        const wchar_t* headers=L"Content-Type: application/x-www-form-urlencoded\r\nOrigin: http://127.0.0.1:8091\r\n";
        if(!WinHttpSendRequest(query.handle,headers,DWORD(-1),post?params.data():nullptr,post?DWORD(params.size()):0,post?DWORD(params.size()):0,0) || !WinHttpReceiveResponse(query.handle,nullptr))return result;
        std::string body;std::array<char,16384> chunk;DWORD got=0;
        do{if(!WinHttpReadData(query.handle,chunk.data(),DWORD(chunk.size()),&got))return result;if(body.size()+got>1048576)return result;body.append(chunk.data(),got);}while(got);
        if(!body.empty())result.body=std::move(body);
    }catch(...){/* A failed request stays an error result, never a UI exception. */}return result;
}
void tick(){
    Ptr dialog=find_host();if(dialog && !lookup(dialog,"WardrobeNativeTx",0x200b))dialog=nullptr;
    if(dialog!=active){
        if(active)tooltip(active,"");
        active=dialog;++generation;preview_sequence=0;preview_ready=false;previous_meta.clear();response_body.clear();response_offset=0;
        icon_widgets.clear();hover_widgets.clear();hover_name.clear();visual_poll=0;scroll=nullptr;
        if(dialog){
            scroll=lookup(dialog,"WardrobeScroll",0x201d);
            for(auto mask:slot_masks){auto name="WardrobeSlot"+std::to_string(mask);auto p=lookup(dialog,name.c_str(),0x2001);if(p)hover_widgets.push_back({p,name,false});}
            for(int i=1;i<=36;++i){auto name="WardrobeRow"+std::to_string(i);auto p=lookup(dialog,name.c_str(),0x2001);if(p)hover_widgets.push_back({p,name,true});}
        }
        if(!dialog && paper)detach();
    }
    if(pending.valid() && pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto result=pending.get();if(active && result.generation==generation){response_body=std::move(result.body);response_sequence=result.sequence;response_offset=0;}
    }
    if(!active)return;
    if(!verified){
        const uint8_t request_prologue[]={0x40,0x53,0x48,0x81,0xec,0x50,0x04,0,0};
        if(std::memcmp(game()+0x3321c0,request_prologue,sizeof(request_prologue))){write(lookup(active,"WardrobeNativeMeta",0x200b),L"unsupported");return;}verified=true;
    }
    deliver_response();
    std::string session=token();const auto now=GetTickCount64();
    if(session.empty() && now>=auth_retry){auth_retry=now+10000;reinterpret_cast<void(__cdecl*)()>(game()+0x3321c0)();}
    Ptr tx=lookup(active,"WardrobeNativeTx",0x200b),preview_tx=lookup(active,"WardrobeNativePreview",0x200b),controls=lookup(active,"WardrobeNativeControl",0x200b);
    auto command=read(tx);
    if(!command.empty() && !pending.valid() && !session.empty()){
        write(tx,L"");try{auto parts=split(command,'|');if(parts.size()!=3 || (parts[1]!="G" && parts[1]!="P"))throw std::runtime_error("Invalid request");
            const unsigned sequence=number(parts[0]);pending=std::async(std::launch::async,request,generation,sequence,parts[1]=="P",parts[2],session);
        }catch(...){write(lookup(active,"WardrobeNativeRx",0x200b),L"0\n{\"error\":\"Invalid Wardrobe request.\"}");}
    }
    auto appearance=read(preview_tx,512);
    if(!appearance.empty()){
        write(preview_tx,L"");try{
            auto parts=split(appearance,'|');if(parts.size()!=3)throw std::runtime_error("Invalid preview");Command c;c.request=int(number(parts[0]));
            Ptr bounds=lookup(active,"WardrobePreviewBounds",0x200b);if(!bounds)throw std::runtime_error("Missing preview bounds");c.bounds=rect(bounds);
            for(const auto& id:split(parts[1],',')){if(id.empty())continue;auto n=number(id,199999999);if(n<100000000 || c.items.size()>=32)throw std::runtime_error("Invalid item");c.items.push_back(n);}
            preview_ready=preview(c);preview_sequence=unsigned(c.request);if(preview_ready)control("wings",parts[2]=="1");
        }catch(...){preview_ready=false;}
    }
    auto action=read(controls,64);if(!action.empty()){write(controls,L"");auto parts=split(action,'|');if(parts.size()==2)control(parts[0],parts[1]=="1");}
    Ptr icons_tx=lookup(active,"WardrobeNativeIcons",0x200b);
    auto icon_command=read(icons_tx,4096);
    if(!icon_command.empty()){
        write(icons_tx,L"");icon_widgets.clear();visual_poll=0;
        for(const auto& assignment:split(icon_command,',')){
            auto parts=split(assignment,':');if(parts.size()!=2 || !icon_name(parts[0]))continue;
            try{auto item=number(parts[1],199999999);auto p=lookup(active,("Wardrobe"+parts[0]).c_str(),0x2021);if(p && item>=100000000)icon_widgets.push_back({p,item});}catch(...){}
        }
    }
    Ptr hover_tx=lookup(active,"WardrobeNativeHover",0x200b);
    auto hover=read(hover_tx,512);
    if(!hover.empty()){
        write(hover_tx,L"");
        // Reuse the native tooltip event used by the stock item browser. The
        // original receiver builds the item tooltip and places it at the mouse.
        const auto query=hover=="clear"?std::string():hover;
        if(query.empty() || query.rfind("item=",0)==0){
            tooltip(active,query);
        }
    }
    const double width=*reinterpret_cast<double*>(game()+0x1378ea8),height=*reinterpret_cast<double*>(game()+0x1378eb0);
    if(std::isfinite(width) && std::isfinite(height) && width>=640 && height>=480 && width<=16384 && height<=16384){
        const Rect r{0,0,width,height};auto old=rect(active);if(old.x || old.y || old.w!=width || old.h!=height)rect(active,r);
        // Addon dialogs otherwise center in a scaled 1280x960 reference area.
        // Match the physical-pixel host and keep the confirmation on screen.
        bool modal_visible=false;
        Ptr confirmation=nullptr;
        for(int id=0x20e;id<=0x221;++id){
            Ptr modal=*reinterpret_cast<Ptr*>(game()+0x13875c0+id*8);if(!modal)continue;
            const char* name=method<const char*(__cdecl*)(Ptr)>(modal,0xa8)(modal);
            if(name && std::strcmp(name,"WardrobeConfirm")==0){
                confirmation=modal;
                modal_visible=(field<uint64_t>(modal,0x30)&1)!=0;
                const Rect desired{(width-460)/2,(height-300)/2,460,300};auto current=rect(modal);
                if(current.x!=desired.x || current.y!=desired.y || current.w!=desired.w || current.h!=desired.h)rect(modal,desired);
                break;
            }
        }
        if(now>=visual_poll){
            visual_poll=now+50;
            layout_combo(lookup(active,"WardrobeFilter",0x2006));
            if(confirmation)layout_combo(lookup(confirmation,"WardrobeUnlockSource",0x2006));
            for(const auto& icon:icon_widgets)crop_icon(icon.widget,icon_side?icon_side(icon.item):40);
            HWND window=mouse::window;if(!window)EnumWindows(mouse::find_window,reinterpret_cast<LPARAM>(&window));
            POINT cursor{};bool valid=window && GetForegroundWindow()==window && GetCursorPos(&cursor) && ScreenToClient(window,&cursor);
            hover_name=hit(cursor,valid,modal_visible);
        }
        std::wstring meta=std::to_wstring(int(width))+L"|"+std::to_wstring(int(height))+L"|"+std::to_wstring(preview_sequence)+L"|"+(preview_ready?L"1":L"0")+L"|"+(session.empty()?L"0":L"1")+L"|"+wide(hover_name);
        if(meta!=previous_meta){write(lookup(active,"WardrobeNativeMeta",0x200b),meta);previous_meta=meta;}
    }
}
struct Bind { Bind(){native_tick_hook=tick;} } bind;
} }
