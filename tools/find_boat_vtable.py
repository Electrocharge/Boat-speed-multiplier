import lief, re, struct, numpy as np
P="/mnt/user-data/uploads/libminecraftpe.so"
e=lief.ELF.parse(P)
rel={}
for r in e.relocations:
    if r.type in (1027,) or 'RELATIVE' in str(r.type):
        rel[r.address]=r.addend
print("relative relocs:",len(rel))
ro=next(s for s in e.sections if s.name=='.rodata'); blob=bytes(ro.content)
names={}
for n in (b"4Boat\0",b"9ChestBoat\0",b"4Boat"):
    for m in re.finditer(re.escape(n),blob): names[ro.virtual_address+m.start()]=n
print({hex(k):v for k,v in names.items()})
inv={}
for a,v in rel.items(): inv.setdefault(v,[]).append(a)
for nm,n in names.items():
    if n==b"4Boat" and blob[nm-ro.virtual_address+5:nm-ro.virtual_address+6]!=b"\0": continue
    for loc in inv.get(nm,[]):
        T=loc-8; print("typeinfo",hex(T),"for",n)
        for v in inv.get(T,[]):
            print("  vtable ref at",hex(v),"-> funcs start",hex(v+8))

text=next(s for s in e.sections if s.name=='.text'); tb=text.virtual_address; td=bytes(text.content)
def fn_words(addr,maxb=12000):
    o=addr-tb
    return np.frombuffer(td[o:o+maxb],dtype='<u4')
consts={'0.04':(0xD70A,0x3D23),'0.005':(0xD70A,0x3BA3),'0.2':(0xCCCD,0x3E4C),'0.1':(0xCCCD,0x3DCC)}
base=0x13d56768
ents=[]
a=base
while a<0x13d571c8:
    v=rel.get(a)
    if v is None: break
    ents.append(v); a+=8
print("vtable entries:",len(ents))
for i,f in enumerate(ents):
    if not (tb<=f<tb+len(td)): continue
    w=fn_words(f)
    # function end: first RET (0xD65F03C0) after at least some words, cut at 3rd ret
    rets=np.nonzero(w==0xD65F03C0)[0]
    end=int(rets[min(2,len(rets)-1)])+1 if len(rets) else len(w)
    w=w[:end]
    hit=[]
    for nm,(lo,hi) in consts.items():
        m1=((w & 0xFFFFFFE0)==(0x52800000|(lo<<5)))
        m2=((w & 0xFFFFFFE0)==(0x72A00000|(hi<<5)))
        if m1.any() and m2.any(): hit.append(nm)
    if hit: print(i,hex(f),"words",len(w),hit)

print("=== call-graph depth search ===")
def fend(f):
    w=fn_words(f,20000)
    rets=np.nonzero(w==0xD65F03C0)[0]
    # take up to the last ret before next likely prologue is hard; use first 3rd ret
    end=int(rets[min(3,len(rets)-1)])+1 if len(rets) else len(w)
    return w[:end]
def has(w,nm):
    lo,hi=consts[nm]
    return ((w & 0xFFFFFFE0)==(0x52800000|(lo<<5))).any() and ((w & 0xFFFFFFE0)==(0x72A00000|(hi<<5))).any()
def bls(f,w):
    idx=np.nonzero((w & 0xFC000000)==0x94000000)[0]
    out=[]
    for i in idx:
        imm=int(w[i]&0x3FFFFFF)
        if imm&(1<<25): imm-=1<<26
        t=f+int(i)*4+imm*4
        if tb<=t<tb+len(td): out.append(t)
    return out
seen=set()
for i,f in enumerate(ents):
    if not (tb<=f<tb+len(td)): continue
    w=fend(f)
    for t in set(bls(f,w)):
        if t in seen: continue
        seen.add(t)
        w2=fend(t)
        h=[n for n in ('0.04','0.005') if has(w2,n)]
        if h: print("vslot",i,hex(f),"-> callee",hex(t),h,len(w2))

print("=== Boat overrides vs Actor ===")
anm=re.search(rb"5Actor\0",blob); anm=ro.virtual_address+anm.start()
actor=None
for loc in inv.get(anm,[]):
    T=loc-8
    for v in inv.get(T,[]): actor=v+8; break
    if actor: break
print("actor vtable",hex(actor) if actor else None)
import sys
sys.path.insert(0,''+__import__('os').path.dirname(__import__('os').path.abspath(__file__))+'')
from find_signatures import make_sig
def fadd(w): return int(((w&0xFF20FC00)==0x1E202800).sum())  # fadd scalar
def fmul(w): return int(((w&0xFF20FC00)==0x1E200800).sum())
rows=[]
for i,f in enumerate(ents):
    af=rel.get(actor+8*i) if actor else None
    if af==f or not (tb<=f<tb+len(td)): continue
    w=fend(f)
    rows.append((i,f,len(w),fmul(w),fadd(w)))
for r in sorted(rows,key=lambda r:-(r[3]+r[4]))[:12]:
    i,f,n,m,a=r
    o=f-tb
    print(f"slot {i} fn {hex(f)} words {n} fmul {m} fadd {a}\n   sig {make_sig(td[o:o+32])}")
print("override count",len(rows))
