import subprocess,re,os,glob,collections,sys
SP='/tmp/claude-0/-home-user-last-kushos/566e914f-110e-5fac-8c4e-cadfc6298dcb/scratchpad'
core=SP+'/ff/x/core'
# imports per dll
imp=collections.defaultdict(set)
for f in glob.glob(core+'/*.exe')+glob.glob(core+'/*.dll'):
    out=subprocess.run(['x86_64-w64-mingw32-objdump','-p',f],capture_output=True,text=True).stdout
    cur=None
    for l in out.split('\n'):
        m=re.match(r'\s*DLL Name: (\S+)',l)
        if m: cur=m.group(1).lower(); continue
        m=re.match(r'\s*[0-9a-f]+\s+\d+\s+(\S+)$',l)
        if cur and m: imp[cur].add(m.group(1))
# apiset table WIN10
tab={}
for l in open(SP+'/ros/sdk/lib/apisets/apisets.table.c',encoding='latin1'):
    m=re.search(r'^\s*\{ RTL_CONSTANT_STRING\(L"([^"]+)"\), RTL_CONSTANT_STRING\(L"([^"]*)"\), ([^}]+)\}',l)
    if m and 'WIN10' in m.group(3): tab[m.group(1).lower()+'.dll']=m.group(2).lower()
# exports of kush dlls
dllpath={}
for line in open(SP+'/build/boot/bootcd.Release.lst'):
    if '=' in line and line.lower().split('=')[0].endswith('.dll'):
        dllpath[os.path.basename(line.split('=')[0]).lower()]=line.split('=',1)[1].strip()
expcache={}
def exports(d):
    if d in expcache: return expcache[d]
    p=dllpath.get(d); s=set()
    if p and os.path.exists(p):
        out=subprocess.run(['x86_64-w64-mingw32-objdump','-p',p],capture_output=True,text=True).stdout
        on=False
        for l in out.split('\n'):
            if 'Ordinal/Name Pointer' in l: on=True; continue
            if on:
                m=re.match(r'\s*\[\s*\d+\]\s+(\S+)',l)
                if m: s.add(m.group(1))
                elif l.strip()=='' and s: on=False
    expcache[d]=s; return s
local=set(os.path.basename(f).lower() for f in glob.glob(core+'/*'))
for d in sorted(imp):
    if d in local: continue
    if d.startswith(('api-ms-','ext-ms-')):
        t=tab.get(d)
        if not t: print('NOAPISET',d,len(imp[d])); continue
        miss=sorted(f for f in imp[d] if f not in exports(t))
        if miss: print('MISSING',d,'->',t,len(miss),' '.join(miss[:12]))
    else:
        if d not in dllpath: print('NODLL',d,len(imp[d]))
        else:
            miss=sorted(f for f in imp[d] if f not in exports(d))
            if miss: print('MISSINGFN',d,len(miss),' '.join(miss[:12]))
