#!/usr/bin/env python3
"""A small Bink 1 (revision i) encoder, so that a movie of one's own (any
video FFmpeg reads) plays as the game's movies do, for example the Xbox's
startup movie (bink\\xbox_startup.bik, played before the intro):

    python3 tools/bink_encode.py <input video> <output.bik> [--width 1280 --height 720 --fps 30]

The video: intra DCT, fill and skip blocks (no motion), each intra block at
the coarsest quantizer within --quality (a mean square error budget per
pixel), studio-range BT.601 4:2:0. The audio: the RDFT kind of Bink audio,
stereo, each band quantized at --audio-quality of its RMS. It writes what
the game's decoder (port/linux/src/bink_decoder.c, FFmpeg's) reads; FFmpeg
plays it too. Needs ffmpeg and numpy; the tables are FFmpeg's binkdata.h.
"""
import argparse, math, os, re, struct, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))

def _binkdata(name):
    path = os.path.join(HERE, '..', 'port', 'third_party', 'ffmpeg_bink', 'binkdata.h')
    text = open(path).read()
    m = re.search(r'%s\[[^\]]*\](?:\[[^\]]*\])?\s*=\s*\{(.*?)\};' % name, text, re.S)
    return [int(x, 0) for x in re.findall(r'-?0x[0-9A-Fa-f]+|-?\d+', m.group(1))]

SCAN = np.array(_binkdata('bink_scan'))
IQ = np.array(_binkdata('bink_intra_quant'), dtype=np.int64).reshape(16, 64)   # by scan position

SKIP, INTRA, FILL = 0, 5, 6

# ---------------------------------------------------------------- bits

class Bits:
    def __init__(self):
        self.words = bytearray()
        self.acc = 0
        self.n = 0
        self.total = 0

    def put(self, value, count):
        if count <= 0:
            return
        value &= (1 << count) - 1
        self.acc |= value << self.n
        self.n += count
        self.total += count
        while self.n >= 8:
            self.words.append(self.acc & 0xFF)
            self.acc >>= 8
            self.n -= 8

    def align32(self):
        pad = (-self.total) & 31
        self.put(0, pad)

    def data(self):
        out = bytearray(self.words)
        if self.n:
            out.append(self.acc & 0xFF)
        return bytes(out)

# ---------------------------------------------------------------- the IDCT

A1, A2, A3, A4 = 2896, 2217, 3784, -5352

def _mul(x, y):
    return (x * y) >> 11

def _idct_1d(s, munge):
    a0 = s[0] + s[4]; a1 = s[0] - s[4]; a2 = s[2] + s[6]; a3 = _mul(A1, s[2] - s[6])
    a4 = s[5] + s[3]; a5 = s[5] - s[3]; a6 = s[1] + s[7]; a7 = s[1] - s[7]
    b0 = a4 + a6; b1 = _mul(A3, a5 + a7); b2 = _mul(A4, a5) - b0 + b1
    b3 = _mul(A1, a6 - a4) - b2; b4 = _mul(A2, a7) + b3 - b1
    out = [a0 + a2 + b0, a1 + a3 - a2 + b2, a1 - a3 + a2 + b3, a0 - a2 - b4,
           a0 - a2 + b4, a1 - a3 + a2 - b3, a1 + a3 - a2 - b2, a0 + a2 - b0]
    return [munge(v) for v in out]

def idct_put_blocks(d):
    """d: (N, 64) int64 dequantized (natural order) -> (N, 64) pixels, as the
    decoder's bink_idct_put (before its store's low 8 bits)."""
    d = d.astype(np.int64)
    n = d.shape[0]
    blk = d.reshape(n, 8, 8)
    # columns: src stride 8 (rows of the block), for each column i
    col = blk.transpose(0, 2, 1).reshape(n * 8, 8)  # (n*8 columns, 8 values down)
    zero = (col[:, 1:] == 0).all(axis=1)
    s = [col[:, k] for k in range(8)]
    out = _idct_1d(s, lambda v: v)
    tmp = np.stack(out, axis=1)
    tmp[zero] = col[zero, 0:1]
    tmp = tmp.reshape(n, 8, 8).transpose(0, 2, 1)   # back to [row][col]
    rows = tmp.reshape(n * 8, 8)
    s = [rows[:, k] for k in range(8)]
    out = _idct_1d(s, lambda v: (v + 0x7F) >> 8)
    return np.stack(out, axis=1).reshape(n, 64)

def idct_float_matrix():
    """the IDCT's linear part, pixels = M @ d (no rounding)"""
    m = np.zeros((64, 64))
    for j in range(64):
        e = np.zeros(64); e[j] = 1.0
        blk = e.reshape(8, 8)
        tmp = np.zeros((8, 8))
        for i in range(8):
            s = blk[:, i]
            tmp[:, i] = _fidct(s, 1.0)
        out = np.zeros((8, 8))
        for i in range(8):
            out[i, :] = _fidct(tmp[i, :], 1.0 / 256)
        m[:, j] = out.reshape(64)
    return m

def _fidct(s, scale):
    f = lambda x, y: x * y / 2048.0
    a0 = s[0] + s[4]; a1 = s[0] - s[4]; a2 = s[2] + s[6]; a3 = f(A1, s[2] - s[6])
    a4 = s[5] + s[3]; a5 = s[5] - s[3]; a6 = s[1] + s[7]; a7 = s[1] - s[7]
    b0 = a4 + a6; b1 = f(A3, a5 + a7); b2 = f(A4, a5) - b0 + b1
    b3 = f(A1, a6 - a4) - b2; b4 = f(A2, a7) + b3 - b1
    return np.array([a0 + a2 + b0, a1 + a3 - a2 + b2, a1 - a3 + a2 + b3, a0 - a2 - b4,
                     a0 - a2 + b4, a1 - a3 + a2 - b3, a1 + a3 - a2 - b2, a0 + a2 - b0]) * scale

M = idct_float_matrix()
MINV = np.linalg.inv(M)

# ---------------------------------------------------------------- DCT coefficients

def write_coeffs(bw, q):
    """q: 64 ints by scan position (q[0], the DC, not written here)"""
    mags = [abs(int(v)) for v in q]
    nbits = max(mags[1:]).bit_length() if any(mags[1:]) else 0
    assert nbits <= 15
    bw.put(nbits, 4)
    if not nbits:
        return
    coef = [0] * 128; mode = [0] * 128
    start = end = 64
    for c, m in ((4, 0), (24, 0), (44, 0), (1, 3), (2, 3), (3, 3)):
        coef[end] = c; mode[end] = m; end += 1
    coded = [False] * 64

    def value(c, bits):
        v = int(q[c]); a = abs(v)
        if bits == 0:
            bw.put(1 if v < 0 else 0, 1)
        else:
            bw.put(a - (1 << bits), bits)
            bw.put(1 if v < 0 else 0, 1)
        coded[c] = True

    for bits in range(nbits - 1, -1, -1):
        level = 1 << bits
        pos = start
        while pos < end:
            c, m = coef[pos], mode[pos]
            if not (c | m):
                pos += 1
                continue
            if m == 0:
                span = range(c, min(c + 20, 64))
            elif m == 1:
                span = range(c, min(c + 16, 64))
            elif m == 2:
                span = range(c, min(c + 4, 64))
            else:
                span = (c,)
            sig = any(not coded[k] and mags[k] >= level for k in span)
            bw.put(1 if sig else 0, 1)
            if not sig:
                pos += 1
                continue
            if m == 0 or m == 2:
                if m == 0:
                    coef[pos] = c + 4; mode[pos] = 1
                else:
                    coef[pos] = 0; mode[pos] = 0; pos += 1
                for k in range(c, c + 4):
                    if mags[k] >= level:
                        bw.put(0, 1)
                        value(k, bits)
                    else:
                        bw.put(1, 1)
                        start -= 1
                        coef[start] = k; mode[start] = 3
            elif m == 1:
                mode[pos] = 2
                cc = c
                for _ in range(3):
                    cc += 4
                    coef[end] = cc; mode[end] = 2; end += 1
            else:
                value(c, bits)
                coef[pos] = 0; mode[pos] = 0; pos += 1

# ---------------------------------------------------------------- a plane

def bundle_len(n):
    return int(math.floor(math.log2(n + 511))) + 1

class Plane:
    """a plane's encoder: target (padded) and reconstruction"""

    def __init__(self, width, height, chroma, full_width):
        self.width, self.height, self.chroma = width, height, chroma
        self.bw = (full_width + 15) >> 4 if chroma else (full_width + 7) >> 3
        fh = height * 2 if chroma else height
        self.bh = (fh + 15) >> 4 if chroma else (fh + 7) >> 3
        self.recon = np.zeros((self.bh * 8, self.bw * 8), dtype=np.int64)
        self.have = False

    def encode(self, bw_, target, quality, skip_threshold):
        bwn, bhn = self.bw, self.bh
        pad = np.zeros((bhn * 8, bwn * 8), dtype=np.float64)
        h, w = target.shape
        pad[:h, :w] = target
        if h < pad.shape[0]:
            pad[h:, :w] = target[h - 1:h, :]
        if w < pad.shape[1]:
            pad[:, w:] = pad[:, w - 1:w]
        blocks = pad.reshape(bhn, 8, bwn, 8).transpose(0, 2, 1, 3).reshape(bhn * bwn, 64)
        prev = self.recon.reshape(bhn, 8, bwn, 8).transpose(0, 2, 1, 3).reshape(bhn * bwn, 64)
        n = blocks.shape[0]
        types = np.full(n, INTRA, dtype=np.int64)
        if self.have:
            err = np.abs(blocks - prev).max(axis=1)
            types[err <= skip_threshold] = SKIP
        flat = (blocks.max(axis=1) - blocks.min(axis=1)) <= 1.0
        types[(types == INTRA) & flat] = FILL
        fills = np.clip(np.rint(blocks.mean(axis=1)), 0, 255).astype(np.int64)

        # intra: per block quantizer, the coarsest within the error budget
        intra = np.nonzero(types == INTRA)[0]
        qs = np.zeros((n, 64), dtype=np.int64)
        qidx = np.zeros(n, dtype=np.int64)
        recon_blocks = prev.copy()
        if intra.size:
            d = blocks[intra] @ MINV.T                     # natural order
            ds = d[:, SCAN]                                # by scan position
            best = np.zeros(intra.size, dtype=np.int64)
            budget = quality
            for idx in range(16):
                qq = np.rint(ds * 2048.0 / IQ[idx]).astype(np.int64)
                deq = (qq * IQ[idx]) >> 11
                dn = np.zeros_like(deq, dtype=np.float64)
                dn[:, SCAN] = deq
                e = ((dn @ M.T) - blocks[intra]) ** 2
                ok = e.mean(axis=1) <= budget
                best[ok] = idx
            for attempt in range(16):
                q = np.rint(ds * 2048.0 / IQ[best][:, :]).astype(np.int64)
                q[:, 0] = np.clip(q[:, 0], 0, 2047)
                np.clip(q, -32767, 32767, out=q)
                deq = (q * IQ[best]) >> 11
                dn = np.zeros_like(deq)
                dn[:, SCAN] = deq
                pix = idct_put_blocks(dn)
                bad = ((pix < 0) | (pix > 255)).any(axis=1) | (np.abs(q[:, 1:]) >= 1 << 15).any(axis=1)
                if not bad.any():
                    break
                best[bad & (best > 0)] -= 1
                if (bad & (best == 0)).any() and attempt > 0:
                    # clamp the pixels and try once more
                    sel = bad & (best == 0)
                    ds[sel] = (np.clip(blocks[intra][sel], 1, 254) @ MINV.T)[:, SCAN] * 0.98
            pix = np.clip(pix, 0, 255)
            qs[intra] = q
            qidx[intra] = best
            recon_blocks[intra] = pix
        fi = np.nonzero(types == FILL)[0]
        recon_blocks[fi] = fills[fi][:, None]
        self.recon = recon_blocks.reshape(bhn, bwn, 8, 8).transpose(0, 2, 1, 3).reshape(bhn * 8, bwn * 8)
        self.have = True

        # ---- the bitstream
        width = self.width
        lens = {
            'types': bundle_len(((max(width, 8) + 7) & ~7) >> 3),
            'sub': bundle_len(((max(width, 8) + 7) & ~7) >> 4),
            'colors': bundle_len(bwn * 64),
            'pattern': bundle_len(bwn << 3),
            'xoff': bundle_len(((max(width, 8) + 7) & ~7) >> 3),
            'dc': bundle_len(((max(width, 8) + 7) & ~7) >> 3),
            'run': bundle_len(bwn * 48),
        }
        b = bw_
        # trees: block types, sub types, colors (16 high + its own), pattern,
        # x, y, (dcs: none), run
        b.put(0, 4); b.put(0, 4)
        for _ in range(17):
            b.put(0, 4)
        b.put(0, 4); b.put(0, 4); b.put(0, 4); b.put(0, 4)

        types2 = types.reshape(bhn, bwn)
        need_types = [bwn] * bhn
        need_colors = [int((types2[r] == FILL).sum()) for r in range(bhn)]
        need_dc = [int((types2[r] == INTRA).sum()) for r in range(bhn)]
        state = {'types': [0, False], 'colors': [0, False], 'dc': [0, False]}
        ended_unused = False

        def emit(name, r, need, writer):
            buf, ended = state[name]
            if ended or buf > 0:
                return
            k = r
            while k < bhn and need[k] == 0:
                k += 1
            if k >= bhn:
                b.put(0, lens[name])
                state[name][1] = True
                return
            rows = list(range(r, k + 1))
            writer(rows, need[k])
            state[name][0] = need[k]

        def w_types(rows, count):
            vals = [int(v) for v in types2[rows[-1]]]
            b.put(count, lens['types'])
            if all(v == vals[0] for v in vals):
                b.put(1, 1); b.put(vals[0], 4)
                return
            b.put(0, 1)
            i = 0; last = 0
            while i < len(vals):
                v = vals[i]
                if v == last:
                    run = 1
                    while i + run < len(vals) and vals[i + run] == last:
                        run += 1
                    done = False
                    for code, rl in ((15, 32), (14, 12), (13, 8), (12, 4)):
                        if run >= rl:
                            b.put(code, 4); i += rl; done = True
                            break
                    if done:
                        continue
                b.put(v, 4); last = v; i += 1

        def w_colors(rows, count):
            r = rows[-1]
            idx = np.nonzero(types2[r] == FILL)[0] + r * bwn
            vals = [int(fills[i]) for i in idx]
            b.put(count, lens['colors'])
            if all(v == vals[0] for v in vals):
                b.put(1, 1); b.put(vals[0] >> 4, 4); b.put(vals[0] & 15, 4)
                return
            b.put(0, 1)
            for v in vals:
                b.put(v >> 4, 4); b.put(v & 15, 4)

        def w_dcs(rows, count):
            r = rows[-1]
            idx = np.nonzero(types2[r] == INTRA)[0] + r * bwn
            vals = [int(qs[i, 0]) for i in idx]
            b.put(count, lens['dc'])
            v = vals[0]
            b.put(v, 11)
            rest = vals[1:]
            for i in range(0, len(rest), 8):
                chunk = rest[i:i + 8]
                deltas = []
                for x in chunk:
                    deltas.append(x - v); v = x
                m = max(abs(x) for x in deltas)
                bsize = m.bit_length()
                b.put(bsize, 4)
                if bsize:
                    for x in deltas:
                        b.put(abs(x), bsize)
                        if x:
                            b.put(1 if x < 0 else 0, 1)

        zero_written = False
        for r in range(bhn):
            emit('types', r, need_types, w_types)
            if r == 0:
                b.put(0, lens['sub'])
            emit('colors', r, need_colors, w_colors)
            if r == 0:
                b.put(0, lens['pattern']); b.put(0, lens['xoff']); b.put(0, lens['xoff'])
            emit('dc', r, need_dc, w_dcs)
            if r == 0:
                b.put(0, lens['dc']); b.put(0, lens['run'])
            # consume
            for c in range(bwn):
                t = types2[r, c]
                state['types'][0] -= 1
                if t == FILL:
                    state['colors'][0] -= 1
                elif t == INTRA:
                    state['dc'][0] -= 1
                    i = r * bwn + c
                    write_coeffs(b, qs[i])
                    b.put(int(qidx[i]), 4)
        b.align32()
        return types

# ---------------------------------------------------------------- audio (RDFT kind)

WMA = [100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400,
       5300, 6400, 7700, 9500, 12000, 15500, 24500]

class Audio:
    def __init__(self, rate, channels):
        self.rate, self.channels = rate, channels
        sr = rate * channels
        bits = 9 if rate < 22050 else 10 if rate < 44100 else 11
        bits += int(math.log2(channels))
        self.n = 1 << bits
        self.overlap = self.n // 16
        self.block = self.n - self.overlap           # interleaved samples
        half = (sr + 1) // 2
        nb = 1
        while nb < 25:
            if half <= WMA[nb - 1]:
                break
            nb += 1
        self.num_bands = nb
        self.bands = [2] + [(WMA[i - 1] * self.n // half) & ~1 for i in range(1, nb)] + [self.n]
        self.root = 2.0 / (math.sqrt(self.n) * 32768.0)
        self.qtable = [math.exp(i * 0.15289164787221953823) * self.root for i in range(96)]

    def spectrum(self, seg):
        """the decoder's coefficients for a segment of n interleaved samples
        (int16 scale): its inverse RDFT gives n/2 times the signal for
        c[2k] = Re X[k], c[2k+1] = -Im X[k], c[1] = Re X[n/2] (checked
        against the decoder's), and the output is in [-1, 1]"""
        n = self.n
        X = np.fft.rfft(np.asarray(seg) / 32768.0) / (n / 2)
        c = np.zeros(n)
        c[0::2] = X.real[:n // 2]
        c[1::2] = -X.imag[:n // 2]
        c[1] = X.real[n // 2]
        return c

    def encode_block(self, b, seg, quality):
        n = self.n
        c = self.spectrum(seg)
        b.put(0, 0)
        # c[0], c[1] as floats (times root on decode)
        for v in (c[0], c[1]):
            write_float(b, v / self.root)
        # band quantizers
        qi = []
        for k in range(self.num_bands):
            lo, hi = self.bands[k], self.bands[k + 1]
            band = c[lo:hi]
            rms = math.sqrt(float(np.mean(band ** 2))) if hi > lo else 0.0
            step = max(rms * quality, self.root * 4.0)
            i = int(round(math.log(step / self.root) / 0.15289164787221953823))
            i = min(max(i, 0), 95)
            # keep the coefficients within 15 bits
            peak = float(np.max(np.abs(band))) if hi > lo else 0.0
            while i < 95 and peak / self.qtable[i] >= 32767:
                i += 1
            qi.append(i)
        for i in qi:
            b.put(i, 8)
        bandof = np.zeros(n, dtype=np.int64)
        for k in range(self.num_bands):
            bandof[self.bands[k]:self.bands[k + 1]] = k
        qv = np.array([self.qtable[qi[k]] for k in bandof])
        ints = np.rint(c / qv).astype(np.int64)
        ints[:2] = 0
        i = 2
        while i < n:
            j = min(i + 8, n)
            grp = ints[i:j]
            b.put(0, 1)
            m = int(np.max(np.abs(grp)))
            width = m.bit_length()
            b.put(width, 4)
            if width:
                for v in grp:
                    v = int(v)
                    b.put(abs(v), width)
                    if v:
                        b.put(1 if v < 0 else 0, 1)
            i = j
        b.align32()
        return ints, qv

def write_float(b, f):
    # decoder: power 5 bits, mantissa 23 bits: f = m * 2^(power-23), sign
    sign = 1 if f < 0 else 0
    f = abs(f)
    if f == 0:
        b.put(0, 5); b.put(0, 23); b.put(0, 1)
        return
    best = None
    for power in range(32):
        m = int(round(f / 2.0 ** (power - 23)))
        if m < (1 << 23):
            best = (power, m)
            break
    if best is None:
        best = (31, (1 << 23) - 1)
    b.put(best[0], 5); b.put(best[1], 23); b.put(sign, 1)

# ---------------------------------------------------------------- the file

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('input'); ap.add_argument('output')
    ap.add_argument('--width', type=int, default=960)
    ap.add_argument('--height', type=int, default=540)
    ap.add_argument('--fps', type=int, default=30)
    ap.add_argument('--rate', type=int, default=48000)
    ap.add_argument('--quality', type=float, default=6.0, help='intra block mean square error budget')
    ap.add_argument('--skip', type=float, default=3.0)
    ap.add_argument('--audio-quality', type=float, default=0.12)
    ap.add_argument('--frames', type=int, default=0)
    ap.add_argument('--start', type=float, default=0)
    ap.add_argument('--recon', default='')
    a = ap.parse_args()
    W, H = a.width, a.height
    vf = f'scale={W}:{H}:flags=lanczos:out_color_matrix=bt601:out_range=tv,fps={a.fps},format=yuv420p'
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-ss', str(a.start), '-i', a.input, '-vf', vf, '-f', 'rawvideo', '-'],
                         stdout=subprocess.PIPE, check=True).stdout
    fsize = W * H * 3 // 2
    nframes = len(raw) // fsize
    if a.frames:
        nframes = min(nframes, a.frames)
    pcm = subprocess.run(['ffmpeg', '-v', 'error', '-ss', str(a.start), '-i', a.input, '-vn', '-ac', '2', '-ar', str(a.rate),
                          '-f', 's16le', '-'], stdout=subprocess.PIPE, check=True).stdout
    samples = np.frombuffer(pcm, dtype='<i2').astype(np.float64)
    audio = Audio(a.rate, 2)
    print(f'{nframes} frames {W}x{H}, audio {len(samples)//2} samples, frame_len {audio.n}', file=sys.stderr)

    Y = Plane(W, H, False, W)
    U = Plane(W // 2, H // 2, True, W)
    V = Plane(W // 2, H // 2, True, W)
    per_frame = a.rate * 2 / a.fps                       # interleaved samples per video frame
    lead = 2                                             # blocks ahead
    blocks_total = int(math.ceil(len(samples) / audio.block)) + 1
    padded = np.concatenate([samples, np.zeros(audio.n + audio.block * 2)])
    next_block = 0
    packets = []
    for f in range(nframes):
        base = f * fsize
        y = np.frombuffer(raw, dtype=np.uint8, count=W * H, offset=base).reshape(H, W).astype(np.float64)
        u = np.frombuffer(raw, dtype=np.uint8, count=W * H // 4, offset=base + W * H).reshape(H // 2, W // 2).astype(np.float64)
        v = np.frombuffer(raw, dtype=np.uint8, count=W * H // 4, offset=base + W * H * 5 // 4).reshape(H // 2, W // 2).astype(np.float64)
        # audio blocks for this frame
        want = min(blocks_total, int(math.ceil((f + 1) * per_frame / audio.block)) + lead)
        if f == nframes - 1:
            want = blocks_total
        ab = Bits()
        ab.put(0, 32)
        count = 0
        while next_block < want:
            seg = padded[next_block * audio.block: next_block * audio.block + audio.n]
            audio.encode_block(ab, seg, a.audio_quality)
            next_block += 1; count += 1
        ab.put(0, 0)
        adata = ab.data()
        adata = struct.pack('<I', count * audio.block * 2) + adata[4:] if count else b''
        vb = Bits()
        vb.put(0, 32)
        Y.encode(vb, y, a.quality, a.skip)
        V.encode(vb, v, a.quality, a.skip * 0.7)
        U.encode(vb, u, a.quality, a.skip * 0.7)
        vdata = vb.data()
        if a.recon:
            with open(a.recon, 'ab' if f else 'wb') as rf:
                rf.write(Y.recon[:H, :W].astype(np.uint8).tobytes())
                rf.write(U.recon[:H // 2, :W // 2].astype(np.uint8).tobytes())
                rf.write(V.recon[:H // 2, :W // 2].astype(np.uint8).tobytes())
        packets.append(struct.pack('<I', len(adata)) + adata + vdata)
        if f % 30 == 0:
            print(f'frame {f}: {len(packets[-1])} bytes', file=sys.stderr)

    header_size = 44 + 4 + 4 + 4 + 4 * nframes
    offsets = []
    pos = header_size
    for p in packets:
        offsets.append(pos); pos += len(p)
    file_size = pos
    hdr = b'BIKi' + struct.pack('<IIIIIIIIII', file_size - 8, nframes, max(len(p) for p in packets), nframes,
                                W, H, a.fps, 1, 0, 1)
    hdr += struct.pack('<I', audio.n * 2)                 # largest decoded size
    hdr += struct.pack('<HH', a.rate, 0x2000 | 0x4000)  # rate, stereo 16-bit
    hdr += struct.pack('<I', 0)                          # track id
    for i, o in enumerate(offsets):
        hdr += struct.pack('<I', o | (1 if i == 0 else 0))
    assert len(hdr) == header_size
    with open(a.output, 'wb') as fp:
        fp.write(hdr)
        for p in packets:
            fp.write(p)
    print(f'wrote {a.output}: {file_size} bytes', file=sys.stderr)

if __name__ == '__main__':
    main()
