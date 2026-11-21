import subprocess,re,os,glob,collections,sys
SP=sys.argv[1]; core=SP+'/ff/x/core'; start=sys.argv[2] if len(sys.argv)>2 else 'firefox.exe'
def imports(f):
    out=subprocess.run(['x86_64-w64-mingw32-objdump','-p',f],capture_output=True,text=True).stdout
    imp=collections.defaultdict(set); cur=None
    for l in out.split('\n'):
        m=re.match(r'\s*DLL Name: (\S+)',l)
        if m: cur=m.group(1).lower(); continue
        m=re.match(r'\s*[0-9a-f]+\s+\d+\s+(\S+)$',l)
        if cur and m: imp[cur].add(m.group(1))
    return imp
local={os.path.basename(f).lower():f for f in glob.glob(core+'/*')}
# static closure
todo=[start]; seen=set(); need=collections.defaultdict(set)
while todo:
    d=todo.pop()
    if d in seen: continue
    seen.add(d)
    for k,v in imports(local[d]).items():
        if k in local: todo.append(k)
        else: need[k]|=v
print('closure:',sorted(seen))
tab={}
for l in open(SP+'/ros/sdk/lib/apisets/apisets.table.c',encoding='latin1'):
    m=re.search(r'^\s*\{ RTL_CONSTANT_STRING\(L"([^"]+)"\), RTL_CONSTANT_STRING\(L"([^"]*)"\), ([^}]+)\}',l)
    if m: tab[m.group(1).lower()+'.dll']=(m.group(2).lower(),m.group(3).strip())
dllpath={}
for line in open(SP+'/'+os.environ.get('BUILDDIR','build')+'/boot/bootcd.Release.lst'):
    if '=' in line and line.lower().split('=')[0].endswith('.dll'):
        dllpath[os.path.basename(line.split('=')[0]).lower()]=line.split('=',1)[1].strip()
ec={}
def exports(d):
    if d in ec: return ec[d]
    p=dllpath.get(d); s=set()
    if p and os.path.exists(p):
        out=subprocess.run(['x86_64-w64-mingw32-objdump','-p',p],capture_output=True,text=True).stdout
        on=False
        for l in out.split('\n'):
            if 'Ordinal/Name Pointer' in l: on=True; continue
            if on:
                m=re.match(r'\s*\[\s*\d+\]\s+(\S+)',l)
                if m: s.add(m.group(1))
    ec[d]=s; return s
ALT=['kernelbase_ros.dll','kernel32.dll','kernel32_vista.dll','ntdll.dll','ntdll_vista.dll','advapi32.dll','advapi32_vista.dll','version.dll','psapi.dll','shlwapi.dll','ole32.dll','combase.dll','ucrtbase.dll','user32.dll','shcore.dll','sechost.dll','powrprof.dll']
def where(fn):
    return [a for a in ALT if fn in exports(a)]
for d in sorted(need):
    tgt=tab.get(d,(d,''))[0] if d.startswith(('api-ms','ext-ms')) else d
    flags=tab.get(d,('',''))[1]
    have=exports(tgt) if tgt else set()
    miss=sorted(f for f in need[d] if f not in have)
    if d.startswith(('api-ms','ext-ms')) and 'WIN10' not in flags: print('APISET-NOT-WIN10',d,'->',tgt or '?',flags)
    if not tgt or tgt not in dllpath: print('NO-DLL',d,'->',tgt, len(need[d]))
    for f in miss: print('  MISS',d,'->',tgt,f,'| found in:',','.join(where(f)) or 'NOWHERE')
