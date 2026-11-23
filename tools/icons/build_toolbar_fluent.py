#!/usr/bin/env python3
"""Build the 47-slot Explorer toolbar atlas from official Fluent SVGs.
Original command indices, dimensions and magenta image-list masks stay fixed.
"""
from pathlib import Path
import io,json,re,shutil,sys
import cairosvg
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[2];PKG=Path(sys.argv[1]);OUT=ROOT/'dll/win32/browseui/res/fluent';OUT.mkdir(parents=True,exist_ok=True)
SOURCES=ROOT/'media/icons/fluent-system';SOURCES.mkdir(parents=True,exist_ok=True)
NAMES=['arrow_left','arrow_right','star','star_add','window','cut','copy','clipboard_paste','arrow_undo','arrow_redo','delete','document_add','folder_open','save','document_search','document_settings','question_circle','search','search','arrow_upload','board','list','grid','table','arrow_download','arrow_download','arrow_download','arrow_download','arrow_up','plug_connected','plug_disconnected','folder_add','grid','arrow_export','arrow_import','phone','phone','board','grid','text_font','table','folder_arrow_right','folder_arrow_left','folder','folder_arrow_right','folder_copy','window_settings']
FALLBACK={'star_add':['star'],'folder_copy':['copy'],'save_edit':['save'],'document_settings':['document'],'hard_drive':['storage'],'folder_arrow_right':['folder_arrow_right','folder'],'folder_arrow_left':['folder_arrow_left','folder'],'window_settings':['window'],'clipboard_paste':['clipboard'],'text_font':['text_font_size'],'document_search':['document'],'folder_open':['folder']}
MAP=[]
def render(name,size,hot=False):
 src=None
 for concept in [name]+FALLBACK.get(name,[]):
  for n in [size,24,20,16,32,48]:
   p=PKG/'icons'/f'{concept}_{n}_regular.svg'
   if p.exists():src=p;break
  if src:break
 if not src:raise RuntimeError('Missing Fluent icon: '+name)
 svg=src.read_text();color='#334B5C' if not hot else '#087F83'
 if name.startswith('folder'):color='#B47D08'
 if name in ['arrow_left','arrow_right','arrow_up','search']:color='#087F83'
 svg=re.sub(r'<svg\b','<svg fill="'+color+'"',svg,count=1).replace('fill="#212121"','fill="'+color+'"').replace('fill="currentColor"','fill="'+color+'"')
 png=cairosvg.svg2png(bytestring=svg.encode(),output_width=size*3,output_height=size*3)
 im=Image.open(io.BytesIO(png)).convert('RGBA').resize((size,size),Image.Resampling.LANCZOS)
 # Binary mask avoids magenta fringes: antialias against the actual pale bar.
 bg=Image.new('RGB',im.size,(247,248,250));bg.paste(im,mask=im.getchannel('A'));alpha=im.getchannel('A').point(lambda v:255 if v>20 else 0)
 tile=Image.new('RGB',im.size,(255,0,255));tile.paste(bg,mask=alpha)
 shutil.copyfile(src,SOURCES/src.name)
 return tile,src.name
for num,size,hot in [('214',24,False),('215',24,True),('216',16,False),('217',16,True)]:
 atlas=Image.new('RGB',(47*size,size),(255,0,255))
 for i,name in enumerate(NAMES):
  tile,src=render(name,size,hot);atlas.paste(tile,(i*size,0))
  if num=='214':MAP.append({'slot':i,'concept':name,'source_svg':src})
 atlas.save(OUT/(num+'.bmp'))
for num,hot in [('230',False),('231',True)]:render('arrow_right',20,hot)[0].save(OUT/(num+'.bmp'))
(SOURCES/'toolbar-map.json').write_text(json.dumps(MAP,indent=2)+'\n')
rc=ROOT/'dll/win32/browseui/browseui.rc';s=rc.read_text()
if 'KUSH_FLUENT_TOOLBAR' not in s:
 for ident,num in [('IDB_SHELL_EXPLORER_LG','214'),('IDB_SHELL_EXPLORER_LG_HOT','215'),('IDB_SHELL_EXPLORER_SM','216'),('IDB_SHELL_EXPLORER_SM_HOT','217'),('IDB_SHELL_GO','230'),('IDB_SHELL_GO_HOT','231')]:
  s=re.sub(r'(?m)^'+ident+r'\s+BITMAP\s+"res/'+num+r'\.bmp"',ident+' BITMAP "res/fluent/'+num+'.bmp"\n('+ident+' + 40000) BITMAP "res/'+num+'.bmp"',s)
 rc.write_text('/* KUSH_FLUENT_TOOLBAR: 47 command slots retained, ClassicIcons rollback. */\n'+s)
for file,ids in [('toolsband.cpp',['IDB_SHELL_EXPLORER_LG','IDB_SHELL_EXPLORER_LG_HOT']),('addressband.cpp',['IDB_GOBUTTON_NORMAL','IDB_GOBUTTON_HOT'])]:
 p=ROOT/'dll/win32/browseui'/file;s=p.read_text()
 if '#include <kushicons.h>' not in s:s=s.replace('#include "precomp.h"','#include "precomp.h"\n#define KUSH_ICONS_NO_LOAD_MACROS\n#include <kushicons.h>')
 for ident in ids:s=s.replace('MAKEINTRESOURCEW('+ident+')','MAKEINTRESOURCEW(KushBitmapResourceId(_AtlBaseModule.GetResourceInstance(), '+ident+'))')
 p.write_text(s)
p=ROOT/'dll/win32/browseui/CMakeLists.txt';s=p.read_text()
if 'kush_toolbar_icon_deps' not in s:
 s+='\nfile(GLOB kush_toolbar_icon_deps "${CMAKE_CURRENT_SOURCE_DIR}/res/fluent/*.bmp")\nadd_rc_deps(browseui.rc ${kush_toolbar_icon_deps})\n';p.write_text(s)
print('Fluent toolbar: 47 unchanged slots, 4 atlases and 2 Go buttons')
