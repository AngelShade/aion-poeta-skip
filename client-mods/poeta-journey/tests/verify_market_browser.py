"""Execute the generated browser bridge in this isolated test process, with stub native callees."""
import argparse, ctypes, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from patch_game_dll import *
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--original-dll',type=Path,required=True)
args=parser.parse_args()
k = ctypes.WinDLL('kernel32',use_last_error=True)
k.VirtualAlloc.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_uint32,ctypes.c_uint32];k.VirtualAlloc.restype=ctypes.c_void_p
k.VirtualProtect.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint32)]
k.VirtualFree.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_uint32]
size=0x1450000
base=k.VirtualAlloc(None,size,0x3000,4)
if not base:raise ctypes.WinError(ctypes.get_last_error())
url='http://127.0.0.1:8091/shop'
urls=[url,'http://127.0.0.1:8091/market','http://127.0.0.1:8091/market/wardrobe','http://127.0.0.1:8091/journey']
captured=(ctypes.c_uint64*3)()
captured_url=ctypes.create_string_buffer(256)
token_requests=ctypes.c_uint32()
def put(offset,code):ctypes.memmove(base+offset,code,len(code))
try:
 hook=build_browser_hook_code(urls)
 put(BROWSER_HOOK_RVA,hook)
 auth=build_market_auth_code(urls[1:]);put(MARKET_AUTH_HOOK_RVA,auth)
 put(MARKET_AUTH_RVA,b'\xe9'+struct.pack('<i',MARKET_AUTH_HOOK_RVA-MARKET_AUTH_RVA-5))
 # Stub the native pending-token path without issuing a game packet.
 put(0xb544b0,b'\x31\xc0\xc3')
 put(MARKET_AUTH_RVA+len(MARKET_AUTH_ORIGINAL),b'\xb8\x33\0\0\0\xc3')
 put(NATIVE_REQUEST_TOKEN_RVA,b'\x48\xb8'+struct.pack('<Q',ctypes.addressof(token_requests))+b'\xff\x00\x31\xc0\xc3')
 # Capture direct-navigation ABI arguments; return marker 17.
 capture=Assembler(BROWSER_LOAD_RVA)
 capture.emit(b'\x48\xb8'+struct.pack('<Q',ctypes.addressof(captured))+b'\x48\x89\x08\x48\x89\x50\x08\x4c\x89\x40\x10')
 capture.emit(b'\x48\xba'+struct.pack('<Q',ctypes.addressof(captured_url))+b'\x31\xc9')
 capture.label('copy');capture.emit(b'\x45\x8a\x0c\x08\x44\x88\x0c\x0a\xff\xc1\x45\x84\xc9');capture.branch(b'\x0f\x85','copy');capture.emit(b'\xb8\x11\0\0\0\xc3')
 put(BROWSER_LOAD_RVA,capture.finish())
 # The fallback executed the real displaced prologue. Restore its ABI frame;
 # return marker 34 instead of invoking publisher auth from a test process.
 put(BROWSER_AUTH_RVA+len(BROWSER_PROLOGUE),b'\x48\x8b\x74\x24\x40\x48\x83\xc4\x28\xb8\x22\0\0\0\xc3')
 old=ctypes.c_uint32()
 if not k.VirtualProtect(base,size,0x20,ctypes.byref(old)):raise ctypes.WinError(ctypes.get_last_error())
 assert k.VirtualProtect(base+NATIVE_SECURITY_TOKEN_RVA,16,4,ctypes.byref(old))
 fn=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p)(base+BROWSER_HOOK_RVA)
 native=ctypes.create_string_buffer(0x344);wrapper=ctypes.create_string_buffer(0x18)
 struct.pack_into('<Q',wrapper,0x10,ctypes.addressof(native));struct.pack_into('<i',native,0x340,7)
 exact=ctypes.create_string_buffer(url.encode())
 assert fn(wrapper,exact)==17
 assert tuple(captured)==(base+BROWSER_MANAGER_RVA,7,ctypes.addressof(exact))
 for value in ['', 'h','https://127.0.0.1:8091/shop',url+'?x=1',url+'x','http://example.invalid/']:
  assert fn(wrapper,ctypes.create_string_buffer(value.encode()))==34,value
 market=ctypes.create_string_buffer(urls[1].encode())
 assert fn(wrapper,market)==51,'missing token must use the native token request path'
 token=b'ABCDEFGHIJKLMNOP';put(NATIVE_SECURITY_TOKEN_RVA,token)
 assert fn(wrapper,market)==17
 assert tuple(captured)[:2]==(base+BROWSER_MANAGER_RVA,7)
 assert captured_url.value==urls[1].encode()+b'?session_id='+token.hex().encode()
 for token in (bytes(range(16)),bytes(range(240,256)),b'0123456789abcdef'):
  put(NATIVE_SECURITY_TOKEN_RVA,token);assert fn(wrapper,market)==17
  assert captured_url.value==urls[1].encode()+b'?session_id='+token.hex().encode()
 assert fn(wrapper,ctypes.create_string_buffer((urls[1]+'?x=1').encode()))==34
 wardrobe=ctypes.create_string_buffer(urls[2].encode())
 assert fn(wrapper,wardrobe)==17
 assert captured_url.value==urls[2].encode()+b'?session_id='+b'0123456789abcdef'.hex().encode()
 for value in (urls[2]+'x',urls[2]+'?x=1',urls[2][:-1]):
  assert fn(wrapper,ctypes.create_string_buffer(value.encode()))==34,value
 assert fn(wrapper,None)==34
 journey=ctypes.create_string_buffer(urls[3].encode())
 put(NATIVE_SECURITY_TOKEN_RVA,bytes(16))
 assert fn(wrapper,journey)==17, 'Journey must open its local shell without the publisher pending-key queue'
 assert captured_url.value==urls[3].encode() and token_requests.value==1
 assert tuple(captured)[:2]==(base+BROWSER_MANAGER_RVA,7)
 put(NATIVE_SECURITY_TOKEN_RVA,b'0123456789abcdef')
 assert fn(wrapper,journey)==17
 assert captured_url.value==urls[3].encode()+b'?session_id='+b'0123456789abcdef'.hex().encode()
 assert token_requests.value==1, 'An existing key must not request another one'
 for value in (urls[3]+'x',urls[3]+'?x=1',urls[3][:-1]):
  assert fn(wrapper,ctypes.create_string_buffer(value.encode()))==34,value
 struct.pack_into('<i',native,0x340,-1);assert fn(wrapper,exact)!=17
 struct.pack_into('<Q',wrapper,0x10,0);assert fn(wrapper,exact)!=17
 for value in ['https://example.invalid/', 'http://127.0.0.1:8091/'+('x'*200),url+'\n']:
  try:build_browser_hook_code(value)
  except ValueError:pass
  else:raise AssertionError('Accepted invalid URL')
 # Exact differential verifies only the three original call sites and caves changed.
 original=args.original_dll.read_bytes()
 patched=build_dll(args.original_dll,['transmog','broker','warehouse'],urls)
 allowed=[(CALLER_RVA,CALLER_RVA+5),(HOOK_RVA,HOOK_RVA+len(build_hook_code(['transmog','broker','warehouse']))),(BROWSER_AUTH_RVA,BROWSER_AUTH_RVA+9),(BROWSER_HOOK_RVA,BROWSER_HOOK_RVA+len(hook)),(PREVIEW_DOCK_RVA,PREVIEW_DOCK_RVA+len(PREVIEW_DOCK_ORIGINAL)),(PREVIEW_DOCK_HOOK_RVA,PREVIEW_DOCK_HOOK_RVA+len(build_preview_dock_code()))]
 allowed += [(MARKET_AUTH_RVA,MARKET_AUTH_RVA+len(MARKET_AUTH_ORIGINAL)),(MARKET_AUTH_HOOK_RVA,MARKET_AUTH_HOOK_RVA+len(auth))]
 allowed += [(offset+3,offset+7) for offset in BROWSER_TOOLTIP_MODES]
 allowed += [(MARKET_RECT_RVA,MARKET_RECT_RVA+len(MARKET_RECT_ORIGINAL)),(MARKET_RECT_HOOK_RVA,MARKET_RECT_HOOK_RVA+len(build_market_rect_code()))]
 assert len(original)==len(patched)
 for i,(a,b) in enumerate(zip(original,patched)):
  if a!=b:assert any(start<=i<end for start,end in allowed),hex(i)
 print('PASS: native account token encoding, missing-token request fallback, exact market/shop URLs, publisher fallback, view guards and DLL modification bounds.')
finally:k.VirtualFree(base,0,0x8000)
