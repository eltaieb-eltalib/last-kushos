import gdb, struct, bisect, os
D=os.path.dirname(os.path.abspath(__file__)) if '__file__' in dir() else '.'
D='/tmp/claude-0/-home-user-last-kushos/566e914f-110e-5fac-8c4e-cadfc6298dcb/scratchpad/dbg'
inf=gdb.selected_inferior()
def rd(a,n): return bytes(inf.read_memory(a,n))
def q(a): return struct.unpack('<Q',rd(a,8))[0]
def phys(on): gdb.execute('maintenance packet Qqemu.PhyMemMode:%d'%on, to_string=True)
def rdp(pa,n):
    phys(1)
    try: return bytes(inf.read_memory(pa,n))
    finally: phys(0)
def v2p(cr3,va):
    t=cr3&~0xfff
    for sh in (39,30,21,12):
        e=struct.unpack('<Q',rdp(t+((va>>sh)&0x1ff)*8,8))[0]
        if not e&1: return None
        if sh in (30,21) and e&0x80:
            return (e&0x000ffffffffff000&~((1<<sh)-1))|(va&((1<<sh)-1))
        t=e&0x000ffffffffff000
    return t|(va&0xfff)
def urd(cr3,va,n):
    out=b''
    while n>0:
        c=min(n,0x1000-(va&0xfff)); pa=v2p(cr3,va)
        out+= rdp(pa,c) if pa is not None else b'\0'*c
        va+=c; n-=c
    return out
def uq(cr3,a): return struct.unpack('<Q',urd(cr3,a,8))[0]
def ustr(cr3,a):
    l=struct.unpack('<H',urd(cr3,a,2))[0]; b=uq(cr3,a+8)
    return urd(cr3,b,l).decode('utf-16le','replace')
def kstr(a):
    l=struct.unpack('<H',rd(a,2))[0]; b=q(a+8); return rd(b,l).decode('utf-16le','replace')
# symbols
mods=[]  # (base,size,name,symlist)
def loadsym(name,base,pref):
    p=os.path.join(D,name+'.sym')
    if not os.path.exists(p): return None
    L=[]
    for line in open(p):
        x=line.split()
        if len(x)<2: continue
        L.append((int(x[0],16)-pref+base,x[1]))
    L.sort(); return L
KB=0xFFFFF80000000000
def kmods():
    head=KB+0x570e00; e=q(head); r=[]
    while e!=head:
        base=q(e+48); size=struct.unpack('<I',rd(e+64,4))[0]; nm=kstr(e+88)
        r.append((base,size,nm)); e=q(e)
    return r
PREF={'ntoskrnl.exe':0x400000,'win32k.sys':0x10000,'hal.dll':0x10000,'ntdll.dll':0x7ffb7000000,'user32.dll':0x7ffac000000,'kernel32.dll':0x7ffb5000000,'gdi32.dll':0x7ffae000000,'ole32.dll':0x7ffa1000000}
KM=[(KB+0x400000,0x1c1000,'ntkrnlmp.exe',loadsym('ntkrnlmp.exe',KB+0x400000,0x400000))]
def modname(base):
    try:
        pe=struct.unpack('<I',rd(base+0x3c,4))[0]; er=struct.unpack('<I',rd(base+pe+24+112,4))[0]
        if er: 
            nr=struct.unpack('<I',rd(base+er+12,4))[0]; return rd(base+nr,40).split(b'\0')[0].decode()
    except Exception: pass
    return 'mod_%x'%base
def kfind(a):
    for b,s,n,L in KM:
        if b<=a<b+s: return
    if not (0xfffff80000000000<=a<0xffffffffffffffff): return
    b=a&~0xfff
    for i in range(0,0x400):
        try:
            if rd(b,2)==b'MZ':
                pe=struct.unpack('<I',rd(b+0x3c,4))[0]
                if pe<0x1000 and rd(b+pe,4)==b'PE\0\0':
                    sz=struct.unpack('<I',rd(b+pe+24+56,4))[0]
                    if a<b+sz:
                        n=modname(b); ln=n.lower()
                        KM.append((b,sz,n,loadsym(ln,b,PREF[ln]) if ln in PREF else None)); return
        except Exception: return
        b-=0x1000
def sym(a,M):
    for b,s,n,L in M:
        if b<=a<b+s:
            if L:
                i=bisect.bisect_right(L,(a,'~'))-1
                if i>=0: return '%s!%s+0x%x'%(n,L[i][1],a-L[i][0])
            return '%s+0x%x'%(n,a-b)
    return None
sysp=q(KB+0x575a60)
p=sysp; seen=0
while True:
    name=rd(p+608,15).split(b'\0')[0].decode('latin1')
    pid=q(p+208)
    if name.lower().startswith(gdb.parameter('args') or 'stats'):
        cr3=q(p+40); peb=q(p+696)
        print('== PROCESS',name,'pid',pid,'cr3',hex(cr3),'peb',hex(peb))
        UM=[]
        try:
            ldr=uq(cr3,peb+24); h=ldr+16; e=uq(cr3,h); n=0
            while e!=h and n<300:
                b=uq(cr3,e+48); s=struct.unpack('<I',urd(cr3,e+64,4))[0]; nm=ustr(cr3,e+88)
                UM.append((b,s,nm,loadsym(nm.lower(),b,PREF[nm.lower()]) if nm.lower() in PREF else None)); e=uq(cr3,e); n+=1
        except Exception as ex: print('ldr err',ex)
        print('modules:',', '.join('%s@%x'%(m[2],m[0]) for m in UM))
        th=p+648; t=q(th)
        while t!=th:
            et=t-1008; tid=q(et+888+8)
            st=rd(et+340,1)[0]; wr=rd(et+146,1)[0]
            print('\n-- thread',tid,'state',st,'waitreason',wr,'win32',hex(q(et+592)), 'start',sym(q(et+992),UM) or hex(q(et+992)))
            ks=q(et+56); ini=q(et+40)
            if st==8 and ks and ini and 0<ini-ks<0x10000:
                data=rd(ks,ini-ks)
                for i in range(0,len(data)-400,8):
                    cs=struct.unpack_from('<Q',data,i+368)[0]; ss=struct.unpack_from('<Q',data,i+392)[0]
                    rip=struct.unpack_from('<Q',data,i+360)[0]
                    if (cs&0xffff)==0x10 and (ss&0xffff) in (0x18,0) and rip>>40==0xfffff8:
                        kfind(rip)
                        print('  TRAPFRAME @%x rip=%s fault=%x err=%x rsp=%x'%(ks+i, sym(rip,KM) or hex(rip), struct.unpack_from('<Q',data,i+208)[0], struct.unpack_from('<Q',data,i+352)[0]&0xffffffff, struct.unpack_from('<Q',data,i+384)[0]))
            if ks and ini and 0<ini-ks<0x10000:
                data=rd(ks,ini-ks); out=[]
                for i in range(0,len(data)-7,8):
                    v=struct.unpack_from('<Q',data,i)[0]; kfind(v); s=sym(v,KM)
                    if s and ('!' in s) and s not in out[-1:]: out.append(s)
                print('  K:', ' <- '.join(out[:14]))
            tf=q(et+456)
            if tf:
                try:
                    rip=q(tf+360); rsp=q(tf+384)
                    print('  user rip',sym(rip,UM) or hex(rip))
                    data=urd(cr3,rsp,0x3000); out=[]
                    for i in range(0,len(data)-7,8):
                        v=struct.unpack_from('<Q',data,i)[0]; s=sym(v,UM)
                        if s and s not in out[-1:]: out.append(s)
                    print('  U:', ' <- '.join(out[:22]))
                except Exception as ex: print('  user err',ex)
            t=q(t)
    p=q(p+216)-216
    if p==sysp or seen>200: break
    seen+=1
