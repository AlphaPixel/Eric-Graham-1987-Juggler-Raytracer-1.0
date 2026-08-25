"""Vamos intuition.library stub for running the original Amiga `ssg` binary.

Git-tracked copy; installed into the amitools checkout by
`tools/vamos-ext/install.sh` (see `tools/vamos-ext/README.md`).

The important change from the earlier no-op version is in `OpenWindow`: it
allocates a real, output-sized BitMap (six planes) and wires it to the window's
RastPort, so that `GraphicsLibrary.WritePixel` has somewhere to plot and `ssg`'s
`O=` writer reads back genuine bitplane data.  `ssg` reaches the planes as
window->RPort->BitMap->Planes[0..5] with stride BitMap.BytesPerRow, so those are
the fields we populate.

Struct offsets (classic AmigaOS layout):
    NewWindow.Width   = NewWindow + 4     (WORD)
    NewWindow.Height  = NewWindow + 6     (WORD)
    Window.RPort      = Window + 0x32     (APTR)
    RastPort.BitMap   = RastPort + 4      (APTR)
    BitMap.BytesPerRow= BitMap + 0        (UWORD)
    BitMap.Rows       = BitMap + 2        (UWORD)
    BitMap.Flags      = BitMap + 4        (UBYTE)
    BitMap.Depth      = BitMap + 5        (UBYTE)
    BitMap.Planes[i]  = BitMap + 8 + 4*i  (PLANEPTR)
"""

from amitools.vamos.libcore import LibImpl
from amitools.vamos.log import log_intuition
from amitools.vamos.lib.TimerDevice import TimerDevice

DEPTH = 6  # HAM6


class IntuitionLibrary(LibImpl):
    def setup_lib(self, ctx, base_addr):
        self.screens = {}
        self.windows = {}

    def finish_lib(self, ctx):
        self.screens = {}
        self.windows = {}

    def DisplayAlert(self, ctx, alertNumber, string, height):
        msg = ctx.mem.r_cstr(string)
        log_intuition.error(
            "-----> DisplayAlert: #%08x - '%s'@%08x <-----", alertNumber, msg, string
        )

    def AutoRequest(self, ctx, window, body, posText, negText, pFlag, nFlag, width, height):
        itext = ctx.mem.r32(body + 12)  # IntuiText.IText
        msg = ctx.mem.r_cstr(itext)
        log_intuition.error("-----> AutoRequest '%s'", msg)

    def EasyRequestArgs(self, ctx, window, easyStruct, idcmpPtr, args):
        es_TextFormat = ctx.mem.r32(easyStruct + 12)  # EasyStruct.es_TextFormat
        msg = ctx.mem.r_cstr(es_TextFormat)
        log_intuition.error("-----> EasyRequest '%s'", msg)

    def CurrentTime(self, ctx, secs_ptr, micros_ptr):
        secs, micros = TimerDevice.get_sys_time()
        ctx.mem.w32(secs_ptr, secs)
        ctx.mem.w32(micros_ptr, micros)

    def OpenScreen(self, ctx, newScreen):
        mem = ctx.alloc.alloc_memory(0x80, label="Intuition.OpenScreen")
        if mem is None:
            log_intuition.info("OpenScreen(%08x) -> NULL", newScreen)
            return 0
        ctx.mem.clear_block(mem.addr, mem.size, 0)
        self.screens[mem.addr] = mem
        log_intuition.info("OpenScreen(%08x) -> %08x", newScreen, mem.addr)
        return mem.addr

    def CloseScreen(self, ctx, screen):
        mem = self.screens.pop(screen, None)
        if mem is not None:
            ctx.alloc.free_memory(mem)
        log_intuition.info("CloseScreen(%08x)", screen)

    def OpenWindow(self, ctx, newWindow):
        width = ctx.mem.r16(newWindow + 4)   # NewWindow.Width
        height = ctx.mem.r16(newWindow + 6)  # NewWindow.Height
        if width <= 0 or height <= 0:
            width, height = 320, 200         # defensive fallback
        rowbytes = ((width + 15) // 16) * 2
        plane_size = rowbytes * height

        win_mem = ctx.alloc.alloc_memory(0x80, label="Intuition.OpenWindow")
        rp_mem = ctx.alloc.alloc_memory(0x64, label="Intuition.Window.RastPort")
        bm_mem = ctx.alloc.alloc_memory(0x28, label="Intuition.Window.BitMap")
        plane_mem = [
            ctx.alloc.alloc_memory(plane_size, label="Intuition.Window.Plane%d" % p)
            for p in range(DEPTH)
        ]
        allocs = [win_mem, rp_mem, bm_mem] + plane_mem
        if any(m is None for m in allocs):
            for m in allocs:
                if m is not None:
                    ctx.alloc.free_memory(m)
            log_intuition.info("OpenWindow(%08x) -> NULL", newWindow)
            return 0

        for m in allocs:
            ctx.mem.clear_block(m.addr, m.size, 0)

        # BitMap header
        ctx.mem.w16(bm_mem.addr + 0, rowbytes)   # BytesPerRow
        ctx.mem.w16(bm_mem.addr + 2, height)     # Rows
        ctx.mem.w8(bm_mem.addr + 4, 0)           # Flags
        ctx.mem.w8(bm_mem.addr + 5, DEPTH)       # Depth
        for p in range(DEPTH):
            ctx.mem.w32(bm_mem.addr + 8 + 4 * p, plane_mem[p].addr)  # Planes[p]

        # RastPort.BitMap and Window.RPort
        ctx.mem.w32(rp_mem.addr + 4, bm_mem.addr)     # RastPort.BitMap
        ctx.mem.w32(win_mem.addr + 0x32, rp_mem.addr)  # Window.RPort

        self.windows[win_mem.addr] = allocs
        log_intuition.info(
            "OpenWindow(%08x) -> %08x RPort=%08x BitMap=%08x %dx%d rb=%d",
            newWindow, win_mem.addr, rp_mem.addr, bm_mem.addr, width, height, rowbytes,
        )
        return win_mem.addr

    def CloseWindow(self, ctx, window):
        allocs = self.windows.pop(window, None)
        if allocs is not None:
            for m in allocs:
                ctx.alloc.free_memory(m)
        log_intuition.info("CloseWindow(%08x)", window)

    def SetPointer(self, ctx, window, pointer, height, width, xOffset, yOffset):
        log_intuition.info("SetPointer(%08x, %08x)", window, pointer)
