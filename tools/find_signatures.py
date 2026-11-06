#!/usr/bin/env python3
"""Find candidate function signatures in an ARM64 libminecraftpe.so.

Usage:
  python find_signatures.py libminecraftpe.so [-s boat -s Paddle ...] [-o report.txt]
  python find_signatures.py --mediafire https://www.mediafire.com/file/.../libminecraftpe.so/file

1) Lists dynamic symbols matching the needles (if the binary has any).
2) Finds matching strings in the binary, then ADRP+ADD references to them in
   code, walks back to the function prologue, and prints a masked byte pattern
   usable as `kBoatSpeedSignature` (wildcards on ADRP/B/BL/CBZ/etc).
Candidates are LEADS, not answers: confirm in a disassembler before hooking.
Needs: pip install lief numpy
"""
import argparse, re, struct, sys, urllib.request
import numpy as np, lief

def mediafire_download(page_url, dest):
    html = urllib.request.urlopen(urllib.request.Request(
        page_url, headers={"User-Agent": "Mozilla/5.0"})).read().decode("utf8", "ignore")
    m = re.search(r'https://download\d*\.mediafire\.com/[^"\']+', html)
    if not m: sys.exit("Could not find direct link on the MediaFire page")
    urllib.request.urlretrieve(m.group(0), dest)
    return dest

def mask_word(w):
    if (w & 0x9F000000) in (0x90000000, 0x10000000): return True   # ADRP/ADR
    if (w & 0x7C000000) == 0x14000000: return True                 # B/BL
    if (w & 0x3B000000) == 0x18000000: return True                 # LDR literal
    if (w & 0x7E000000) in (0x34000000, 0x36000000): return True   # CBZ/CBNZ/TBZ
    if (w & 0xFF000010) == 0x54000000: return True                 # B.cond
    return False

def make_sig(code, n=32):
    out = []
    for i in range(0, min(n, len(code)) - 3, 4):
        w = struct.unpack_from("<I", code, i)[0]
        out += ["??"] * 4 if mask_word(w) else ["%02X" % b for b in code[i:i+4]]
    return " ".join(out)

def count_matches(sig, text):
    toks = sig.split()
    pat = b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in toks)
    return len(re.findall(b"(?=" + pat + b")", text, re.S))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("so", nargs="?")
    ap.add_argument("--mediafire")
    ap.add_argument("-s", "--string", action="append", default=[])
    ap.add_argument("-o", "--out", default="signature_report.txt")
    a = ap.parse_args()
    needles = a.string or ["boat", "Boat"]
    path = mediafire_download(a.mediafire, "libminecraftpe.so") if a.mediafire else a.so
    if not path: ap.error("give a .so path or --mediafire URL")
    elf = lief.ELF.parse(path)
    lines = []
    log = lambda s="": (print(s), lines.append(s))

    log("== Dynamic symbols ==")
    for sym in elf.dynamic_symbols:
        if sym.value and any(n.lower() in sym.name.lower() for n in needles):
            log(f"0x{sym.value:x}  {sym.name}")

    text = next(s for s in elf.sections if s.name == ".text")
    tdata = bytes(text.content); tbase = text.virtual_address
    words = np.frombuffer(tdata[: len(tdata) // 4 * 4], dtype="<u4")

    targets = {}
    for sec in elf.sections:
        if sec.name in (".rodata", ".data.rel.ro"):
            blob = bytes(sec.content)
            for n in needles:
                for m in re.finditer(re.escape(n.encode()) + rb"[\x20-\x7e]*\x00", blob):
                    targets[sec.virtual_address + m.start()] = m.group().rstrip(b"\0").decode()
    log(f"\n== {len(targets)} matching strings ==")

    adrp = np.nonzero((words & 0x9F000000) == 0x90000000)[0]
    adrp = adrp[adrp + 1 < len(words)]
    nxt = words[adrp + 1]; cur = words[adrp]
    ok = ((nxt & 0xFFC00000) == 0x91000000) & (((nxt >> 5) & 31) == (cur & 31))
    adrp, cur, nxt = adrp[ok], cur[ok], nxt[ok]
    immlo = (cur >> 29) & 3; immhi = (cur >> 5) & 0x7FFFF
    imm = ((immhi << 2) | immlo).astype(np.int64)
    imm = np.where(imm & (1 << 20), imm - (1 << 21), imm)
    pc = tbase + adrp.astype(np.int64) * 4
    target = ((pc >> 12) + imm << 12) + ((nxt >> 10) & 0xFFF).astype(np.int64)

    log("\n== Candidate functions (string xref -> prologue -> signature) ==")
    seen = set()
    for idx, tgt, p in zip(adrp, target, pc):
        s = targets.get(int(tgt))
        if s is None: continue
        j = int(idx)
        for k in range(j, max(j - 1024, 0), -1):  # walk back <=4 KiB
            w = int(words[k])
            if (w & 0xFFC07FFF) == 0xA9807BFD or (w & 0xFF8003FF) == 0xD10003FF:
                if k in seen: break
                seen.add(k)
                code = tdata[k * 4 : k * 4 + 32]
                sig = make_sig(code)
                n = count_matches(sig, tdata)
                log(f"fn 0x{tbase + k*4:x}  string='{s}'  unique={'YES' if n == 1 else n}")
                log(f"  sig: {sig}")
                break
    open(a.out, "w").write("\n".join(lines))
    print(f"\nWrote {a.out}")

if __name__ == "__main__":
    main()
