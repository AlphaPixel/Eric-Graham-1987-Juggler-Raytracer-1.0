"""Vamos graphics.library stub for running the original Amiga `ssg` binary.

This is a git-tracked copy of the graphics.library implementation used to run
the 1987 `ssg` executable under `vamos` (amitools) for reverse-engineering and
verification.  It is installed into the (gitignored) amitools checkout by
`tools/vamos-ext/install.sh`; see `tools/vamos-ext/README.md`.

Unlike the earlier no-op version, `WritePixel` writes real pixels into a real
BitMap.  `ssg` plots every rendered pixel with `SetAPen`/`WritePixel` into the
window's RastPort and then writes its `O=` output file straight from that
RastPort's BitMap planes (see FUN_00224acc in the decompilation:
window->RPort->BitMap->Planes[0..5], stride = BitMap.BytesPerRow).  With a real
BitMap wired up in OpenWindow (IntuitionLibrary), plotting here makes the `O=`
bitplane body a genuine byte-for-byte oracle for the reconstructed renderer.

Amiga struct offsets used (classic layout):
    RastPort.BitMap   = RastPort + 4      (APTR)
    BitMap.BytesPerRow= BitMap + 0        (UWORD)
    BitMap.Rows       = BitMap + 2        (UWORD)
    BitMap.Planes[i]  = BitMap + 8 + 4*i  (PLANEPTR)
"""

from amitools.vamos.libcore import LibImpl


class GraphicsLibrary(LibImpl):
    def setup_lib(self, ctx, base_addr):
        self.pens = {}          # rastport addr -> current FgPen
        self.palettes = {}      # viewport addr -> {index: (r,g,b)}
        self.pixel_count = 0

    def finish_lib(self, ctx):
        self.pens = {}
        self.palettes = {}
        self.pixel_count = 0

    def SetRGB4(self, ctx, vp, index, red, green, blue):
        palette = self.palettes.setdefault(vp, {})
        palette[index & 0x1F] = (red & 0x0F, green & 0x0F, blue & 0x0F)

    def SetAPen(self, ctx, rp, pen):
        self.pens[rp] = pen & 0xFF

    def WritePixel(self, ctx, rp, x, y):
        """Plot pen self.pens[rp] at (x, y) into rp->BitMap's six planes."""
        self.pixel_count += 1
        # x, y arrive as unsigned 32-bit; treat as signed for edge/flush calls
        # such as WritePixel(-1, h-1) that ssg makes past the raster.
        if x >= 0x80000000:
            x -= 0x100000000
        if y >= 0x80000000:
            y -= 0x100000000
        bm = ctx.mem.r32(rp + 4)                 # RastPort.BitMap
        if bm == 0:
            return 0
        bpr = ctx.mem.r16(bm + 0)                # BitMap.BytesPerRow
        rows = ctx.mem.r16(bm + 2)               # BitMap.Rows
        if bpr == 0 or rows == 0:
            return 0
        if x < 0 or y < 0 or y >= rows:          # clip
            return 0
        byte = x >> 3
        if byte >= bpr:
            return 0
        off = y * bpr + byte
        mask = 0x80 >> (x & 7)
        pen = self.pens.get(rp, 0)
        for p in range(6):                       # HAM6: six bitplanes
            plane = ctx.mem.r32(bm + 8 + p * 4)  # BitMap.Planes[p]
            if plane == 0:
                continue
            v = ctx.mem.r8(plane + off)
            if pen & (1 << p):
                v |= mask
            else:
                v &= (~mask) & 0xFF
            ctx.mem.w8(plane + off, v)
        return 0
