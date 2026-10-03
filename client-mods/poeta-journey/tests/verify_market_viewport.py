"""Execute market sizing with the publisher SetRect body in an isolated process."""
import argparse
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from patch_game_dll import *

k = ctypes.WinDLL('kernel32', use_last_error=True)
k.VirtualAlloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint32, ctypes.c_uint32]
k.VirtualAlloc.restype = ctypes.c_void_p
k.VirtualProtect.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
k.VirtualFree.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint32]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--original-dll',type=Path,required=True)
original=parser.parse_args().original_dll.read_bytes()
size = 0x1450000
base = k.VirtualAlloc(None, size, 0x3000, 4)
if not base:
    raise ctypes.WinError(ctypes.get_last_error())


def put(offset, value):
    ctypes.memmove(base + offset, value, len(value))


def geometry(widget):
    return struct.unpack_from('<dddd', widget, 0x50)


try:
    # The actual native setter writes rectangles and invokes its ordinary
    # before/after-layout callbacks. Only those callbacks are stubbed.
    put(MARKET_RECT_RVA, original[MARKET_RECT_RVA:0x5acb3c])
    put(MARKET_RECT_RVA, b'\xe9' + struct.pack('<i', MARKET_RECT_HOOK_RVA - MARKET_RECT_RVA - 5) + b'\x90' * 5)
    put(MARKET_RECT_HOOK_RVA, build_market_rect_code())
    put(0x5abbc0, original[0x5abbc0:0x5abbd1])
    put(0x1000, b'\xc3')
    put(0x1010, bytes.fromhex('488b81a8020000c3'))  # browser-child lookup
    old = ctypes.c_uint32()
    assert k.VirtualProtect(base, size, 0x20, ctypes.byref(old))
    assert k.VirtualProtect(base + UI_WIDTH_RVA, 0x40, 4, ctypes.byref(old))
    vt = ctypes.create_string_buffer(0x500)
    for offset, rva in ((0xa8, 0x5abbc0), (0x1a8, MARKET_RECT_RVA), (0x338, 0x1010), (0x478, 0x1000), (0x480, 0x1000)):
        struct.pack_into('<Q', vt, offset, base + rva)
    keep = []

    def widget(name):
        w = ctypes.create_string_buffer(0x600)
        text = ctypes.create_string_buffer(name.encode())
        head = ctypes.create_string_buffer(24)
        struct.pack_into('<Q', head, 0, ctypes.addressof(head))
        for offset, pointer in ((0, ctypes.addressof(vt)), (0x10, ctypes.addressof(text)), (0x2a0, ctypes.addressof(head))):
            struct.pack_into('<Q', w, offset, pointer)
        struct.pack_into('<Q', w, 0x28, max(16, len(name)))
        struct.pack_into('<dddd', w, 0x50, 240, 0, 1440, 1080)
        keep.extend((w, text, head))
        return w

    fn = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p)(base + MARKET_RECT_RVA)
    dialog, browser = widget('PrivateWarehouse'), widget('PrivateWarehouseBrowser')
    struct.pack_into('<Q', dialog, 0x2a8, ctypes.addressof(browser))
    requested = ctypes.create_string_buffer(struct.pack('<dddd', 240, 0, 1440, 1080))
    count = 0
    for width, height in ((1024, 768), (1280, 1024), (1920, 1080), (2560, 1440), (3440, 1440), (5120, 1440), (1920, 1200)):
        for ui_percent in (75, 100, 125, 150):
            scale = min(width / 1280, height / 960) * ui_percent / 100
            put(UI_WIDTH_RVA, struct.pack('<dd', width, height))
            put(UI_SCALE_RVA, struct.pack('<d', scale))
            fn(dialog, requested)
            assert geometry(dialog) == (0, 0, width, height)
            assert geometry(browser) == (0, 25 * scale, width, height - 25 * scale)
            assert requested.raw[:32] == struct.pack('<dddd', 240, 0, 1440, 1080)
            count += 1
    # Repeating the call at unchanged position still updates width and height.
    fn(dialog, requested)
    cash, cash_browser = widget('PrivateCashShop'), widget('PrivateCashShopBrowser')
    struct.pack_into('<Q', cash, 0x2a8, ctypes.addressof(cash_browser))
    for width, height in ((1024, 768), (1280, 1024), (1920, 1080), (2560, 1440), (3440, 1440), (5120, 1440), (1920, 1200)):
        for ui_percent in (75, 100, 125, 150):
            scale = min(width / 1280, height / 960) * ui_percent / 100
            put(UI_WIDTH_RVA, struct.pack('<dd', width, height))
            put(UI_SCALE_RVA, struct.pack('<d', scale))
            fn(cash, requested)
            assert geometry(cash) == (0, 0, width, height)
            assert geometry(cash_browser) == (0, 25 * scale, width, height - 25 * scale)
            count += 1
    for name in ('', 'P', 'PrivateWarehouseX', 'PrivateWarehouseBrowserX', 'PrivateCashShopX', 'PrivateCashShopBrowserX', 'Inventory'):
        w = widget(name)
        fn(w, requested)
        assert geometry(w) == (240, 0, 1440, 1080), name
    wardrobe, wardrobe_browser = widget('PrivateWardrobe'), widget('PrivateWardrobeBrowser')
    struct.pack_into('<Q', wardrobe, 0x2a8, ctypes.addressof(wardrobe_browser))
    for width, height in ((1024,768),(1920,1080),(3440,1440),(5120,1440)):
        for ui_percent in (75,100,125,150):
            scale = min(width/1280,height/960)*ui_percent/100
            put(UI_WIDTH_RVA,struct.pack('<dd',width,height));put(UI_SCALE_RVA,struct.pack('<d',scale))
            fn(wardrobe,requested)
            assert geometry(wardrobe)==(0,0,width,height)
            assert geometry(wardrobe_browser)==(0,25*scale,width,height-25*scale)
            count += 1
    journey, journey_browser = widget('PrivateJourney'), widget('PrivateJourneyBrowser')
    struct.pack_into('<Q',journey,0x2a8,ctypes.addressof(journey_browser))
    for width,height in ((1024,768),(1280,1024),(1920,1080),(2560,1440),(3440,1440),(5120,1440),(1920,1200)):
        for ui_percent in (75,100,125,150):
            put(UI_WIDTH_RVA,struct.pack('<dd',width,height))
            put(UI_SCALE_RVA,struct.pack('<d',min(width/1280,height/960)*ui_percent/100))
            fn(journey,requested)
            assert geometry(journey)==(0,0,width,height)
            assert geometry(journey_browser)==(0,0,width,height)
            fn(journey_browser,requested)
            assert geometry(journey_browser)==(0,0,width,height)
            count += 1
    for name in ('PrivateJourneyX','PrivateJourneyBrowserX'):
        w=widget(name);fn(w,requested)
        assert geometry(w)==(240,0,1440,1080),name
    for width, height in ((0, 1080), (1920, 0), (-1, 1080), (float('nan'), 1080)):
        put(UI_WIDTH_RVA, struct.pack('<dd', width, height))
        fn(dialog, requested)
        assert geometry(dialog) == (240, 0, 1440, 1080)
    # No child yet during XML loading must be harmless.
    put(UI_WIDTH_RVA, struct.pack('<dd', 3440, 1440))
    struct.pack_into('<Q', dialog, 0x2a8, 0)
    fn(dialog, requested)
    assert geometry(dialog) == (0, 0, 3440, 1440)
    assert MARKET_RECT_HOOK_RVA + len(build_market_rect_code()) <= BROWSER_HOOK_RVA
    print(f'PASS: {count} native resolution/UI-scale cases, live resize, browser inset, unchanged-position resize, other-widget fallback, invalid viewport and missing child.')
finally:
    k.VirtualFree(base, 0, 0x8000)
