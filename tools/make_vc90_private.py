"""Build VC++ 2008 (VC90) x64 *private* assemblies for SPSS 25 on Kush 1.1.

Kush/ReactOS msi does not install Win32 side-by-side assemblies into WinSxS,
so programs that need Microsoft.VC90.* fail with "MSVCR90.dll was not found".
Workaround: copy each assembly as a folder next to the program, with a manifest
that declares the version the program asks for (9.0.21022.8).

Usage: python3 make_vc90_private.py <dir with extracted vc_red.cab> <out dir>
       (vc_red.cab comes from vcredist_x64.exe 9.0.30729.17, extracted with 7z)
Then on Kush run <out>\\fix.bat from the CD.
"""
import os, re, glob, shutil, sys
cab, out = sys.argv[1], sys.argv[2]
shutil.rmtree(out, ignore_errors=True); os.makedirs(out)
low = {f.lower(): f for f in os.listdir(cab)}
bat = []
for m in glob.glob(cab + '/manifest.*_x64.*'):
    suf = m.split('manifest.', 1)[1]
    txt = open(m, encoding='utf-8-sig').read()
    name = re.search(r'name="([^"]+)" version', txt).group(1)
    files = re.findall(r'<file name="([^"]+)"', txt)
    d = os.path.join(out, name); os.makedirs(d)
    for f in files:
        k = (f + '.' + suf).lower()
        if k not in low:
            k = (f.replace('90', '') + '.' + suf).lower()
        shutil.copy(os.path.join(cab, low[k]), os.path.join(d, f))
    with open(os.path.join(d, name + '.manifest'), 'w') as w:
        w.write('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
                '<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">\n'
                '<noInheritable></noInheritable>\n'
                '<assemblyIdentity type="win32" name="%s" version="9.0.21022.8" '
                'processorArchitecture="amd64" publicKeyToken="1fc8b3b9a1e18e3b"></assemblyIdentity>\n'
                '%s</assembly>\n' % (name, ''.join('<file name="%s"></file>\n' % f for f in files)))
    bat.append('xcopy /E /I /Y D:\\%s "C:\\Program Files\\IBM\\SPSS\\Statistics\\25\\%s"' % (name, name))
    print(name, files)
open(os.path.join(out, 'fix.bat'), 'w').write('\r\n'.join(bat) + '\r\n')
