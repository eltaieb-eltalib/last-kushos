#!/usr/bin/env python3
"""Licensed Fluent artwork for selected legacy shell and namespace resources.
Original resource IDs/art stay unchanged; opt-in extraction selects ID+40000.
"""
from pathlib import Path
import json,re,sys,io,shutil
from PIL import Image
import cairosvg
ROOT=Path(__file__).resolve().parents[2];PKG=Path(sys.argv[1]);ASSET=ROOT/'media/icons/fluent-system'
SIZES=[16,20,24,32,48,64]
SHELL={'IDI_SHELL_CDROM':'cd','IDI_SHELL_PRINTER2':'print','IDI_SHELL_5_12_FLOPPY':'save','IDI_SHELL_3_14_FLOPPY':'save','IDI_SHELL_RAMDISK':'ram',
 'IDI_SHELL_PRINTER':'print','IDI_SHELL_PRINTERS_FOLDER':'print','IDI_SHELL_FONTS_FOLDER':'text_font',
 'IDI_SHELL_CONTROL_PANEL2':'settings','IDI_SHELL_CONTROL_PANEL3':'settings',
 'IDI_SHELL_TSKBAR_STARTMENU':'window_apps','IDI_SHELL_NETWORK_CONNECTIONS':'plug_connected',
 'IDI_SHELL_NETWORK_CONNECTIONS2':'plug_connected','IDI_SHELL_FOLDER_OPTIONS':'folder_settings'}
NTOBJ={'IDI_NTOBJECTFOLDER':'cube_tree','IDI_NTOBJECTDIR':'folder','IDI_NTOBJECTDIROPEN':'folder_open',
 'IDI_NTOBJECTITEM':'document','IDI_NTOBJECTDEVICE':'desktop_tower','IDI_NTOBJECTPORT':'plug_connected',
 'IDI_REGISTRYKEY':'database','IDI_REGISTRYVALUE':'document_text'}
NETSHELL={'IDI_SHELL_NETWORK_FOLDER':'network_adapter'}
rows=[];cache={}
def glyph(concept):
 for style in ['color','filled','regular']:
  for size in [48,32,24,20,16]:
   p=PKG/'icons'/f'{concept}_{size}_{style}.svg'
   if p.exists():return p
 raise RuntimeError('Missing official concept '+concept)
def render(concept):
 if concept in cache:return cache[concept]
 sources=[]
 def layer(name):
  src=glyph(name);sources.append(src.name);shutil.copyfile(src,ASSET/src.name);svg=src.read_text()
  if '_color.svg' not in src.name:
   svg=re.sub(r'<svg\b','<svg fill="#087F83"',svg,count=1).replace('fill="#212121"','fill="#087F83"').replace('fill="currentColor"','fill="#087F83"')
  return Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg.encode(),output_width=256,output_height=256))).convert('RGBA')
 if concept=='folder_settings':
  image=layer('folder');badge=layer('settings').resize((128,128),Image.Resampling.LANCZOS);image.alpha_composite(badge,(124,124))
 else:image=layer(concept)
 out=ASSET/('shell-'+concept+'.ico');image.save(out,sizes=[(s,s) for s in SIZES],bitmap_format='bmp');cache[concept]=(out,sources);return out,sources
for module,rcpath,localpath,mapping in [('shell32','dll/win32/shell32/icon_res.rc','res/icons/fluent',SHELL),('ntobjshex','dll/shellext/ntobjshex/ntobjshex.rc','resources/fluent',NTOBJ),('netshell','dll/shellext/netshell/netshell.rc','res',NETSHELL)]:
 rc=ROOT/rcpath;text=rc.read_text();local=rc.parent/localpath;local.mkdir(parents=True,exist_ok=True)
 for resource,concept in mapping.items():
  out,sources=render(concept);shutil.copyfile(out,local/out.name)
  text=re.sub(r'(?m)^\('+resource+r' \+ 40000\) ICON "[^"\n]+"\n?','',text)
  pattern=r'(?m)^('+resource+r'\s+ICON\s+"([^"\n]+)"[^\n]*)$';m=re.search(pattern,text)
  if not m:raise RuntimeError('Resource missing '+resource)
  original=m[2];text=re.sub(pattern,lambda m:m[1]+'\n('+resource+' + 40000) ICON "'+localpath+'/'+out.name+'"',text,count=1)
  rows.append({'module':module,'resource':resource,'original':original,'fluent':concept,'svg':sources,'fluent_offset':40000})
 rc.write_text(text)
 if module=='shell32':(rc.parent/'shell32.rc').touch()
(ASSET/'shell-map.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Mapped shell resources:',len(rows))
