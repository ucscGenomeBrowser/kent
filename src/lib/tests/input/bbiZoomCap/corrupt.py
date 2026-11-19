"""Write copies of a bigBed/bigWig with the zoom-1 index damaged the way #38819 damaged it."""
import shutil, struct, sys
src, highOut, wrapOut = sys.argv[1:4]
def lastLeaf(f):
    f.seek(64); red, res, zdata, zidx = struct.unpack('<IIQQ', f.read(24))
    off = zidx + 48
    while True:
        f.seek(off); isLeaf, r, cnt = struct.unpack('<BBH', f.read(4))
        if isLeaf:
            pos = off + 4 + 32 * (cnt - 1); f.seek(pos)
            return zidx, pos, struct.unpack('<IIIIQQ', f.read(32))
        f.seek(off + 4 + 24 * (cnt - 1)); off = struct.unpack('<IIIIQ', f.read(24))[4]
for out, how in ((highOut, 'high'), (wrapOut, 'wrap')):
    shutil.copy(src, out)
    with open(out, 'r+b') as f:
        zidx, pos, item = lastLeaf(f)
        offset, size = item[4], item[5]
        if how == 'high':      # files of 2-4 GB: high 32 bits set
            bad = size | 0xFFFFFFFF00000000
        else:                  # offset + size wraps to 1, below the index
            bad = (1 << 64) - offset + 1
        f.seek(pos + 24); f.write(struct.pack('<Q', bad))
        f.seek(zidx + 32); f.write(struct.pack('<Q', zidx | 0xFFFFFFFF00000000))
