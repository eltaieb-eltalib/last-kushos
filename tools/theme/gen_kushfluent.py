#!/usr/bin/env python3
"""Build KushFluent from the existing GPLv3 Modern theme, with original flat parts."""
from pathlib import Path
from PIL import Image, ImageDraw
import shutil,re
ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'media/themes/Modern/modern.msstyles'
OUT=ROOT/'media/themes/KushFluent/kushfluent.msstyles'
if not OUT.exists(): shutil.copytree(BASE,OUT)
(OUT.parent/'CMakeLists.txt').write_text('add_subdirectory(kushfluent.msstyles)\n')
p=OUT/'CMakeLists.txt';s=(BASE/'CMakeLists.txt').read_text().replace('modern','kushfluent').replace('${CMAKE_CURRENT_BINARY_DIR}/bitmaps/*', '${CMAKE_CURRENT_SOURCE_DIR}/bitmaps/*').replace('${CMAKE_CURRENT_BINARY_DIR}/lang/*', '${CMAKE_CURRENT_SOURCE_DIR}/lang/*').replace('${CMAKE_CURRENT_BINARY_DIR}/textfiles/*', '${CMAKE_CURRENT_SOURCE_DIR}/textfiles/*');p.write_text(s)
# Keep bitmap dependencies when adding converted INI dependencies.
cmake=OUT/'CMakeLists.txt';text=cmake.read_text();text=text.replace('set_source_files_properties(kushfluent.rc PROPERTIES OBJECT_DEPENDS', 'set_property(SOURCE kushfluent.rc APPEND PROPERTY OBJECT_DEPENDS');cmake.write_text(text)
p=OUT/'kushfluent.rc';s=(BASE/'modern.rc').read_text().replace('Modern Theme','Kush Fluent Theme').replace('"Modern\\0"','"KushFluent\\0"').replace('modern.msstyles','kushfluent.msstyles');p.write_text(s)
(OUT/'modern.rc').unlink(missing_ok=True)
for p in (OUT/'textfiles/Light').glob('*.INI'):
 s=p.read_text()
 if '[Button.Pushbutton]\nTransparent = True' not in s:
  s=s.replace('[Button.Pushbutton]','[Button.Pushbutton]\nTransparent = True\nTransparentColor = 255 0 255')
 s=re.sub(r'Tahoma,\s*(8|9)', 'Inter, 9', s, flags=re.I)
 s=re.sub(r'CaptionFont\s*=.*', 'CaptionFont = Inter SemiBold, 9', s)
 changes={'ActiveCaption':'243 246 250','InactiveCaption':'246 247 249','GradientActiveCaption':'243 246 250','GradientInactiveCaption':'246 247 249','CaptionText':'30 35 41','InactiveCaptionText':'117 123 132','Btnface':'247 248 250','MenuBar':'247 248 250','Highlight':'13 115 119','HighlightText':'255 255 255','MenuHilight':'221 240 239','CaptionBarHeight':'28','TextShadowOffset':'0,0','TextShadowColor':'243 246 250'}
 for key,value in changes.items(): s=re.sub(r'(?im)^'+key+r'\s*=.*$',key+' = '+value,s)
 # A pale client background for themed dialogs, without changing their layout.
 s=s.replace('BorderColor = 239 238 243','BorderColor = 247 248 250').replace('FillColor = 239 238 243','FillColor = 247 248 250')
 # The pale Kush taskbar needs dark clock text, unlike the inherited Modern tray.
 s=re.sub(r'(\[TrayNotify::Clock\][^\[]*?TextColor\s*=\s*)[^\n]*',r'\g<1>30 35 41',s)
 s=s.replace('TextColor = 127 124 115','TextColor = 30 35 41')
 s=s.replace('EdgeShadowColor = 165 175 185','EdgeShadowColor = 226 231 237')
 s=s.replace('BorderColor = 150 150 150','BorderColor = 202 208 216')
 for part in ['ScrollBar.GripperHorz','ScrollBar.GripperVert']:
  token='['+part+']'
  start=s.find(token);end=s.find('\n[',start+1)
  if start >= 0:
   end=len(s) if end < 0 else end
   section=s[start:end]
   if 'TransparentColor' not in section:s=s[:start]+section+'\nTransparentColor = 255 0 255\n'+s[end:]
 p.write_text(s)
B=OUT/'bitmaps/Light'
MAG=(255,0,255); ACTIVE=(243,246,250); INACTIVE=(246,247,249); BORDER=(202,208,216)
def frames(name, states=2, kind='top'):
 p=B/('LIGHT_'+name+'.bmp'); old=Image.open(p);w,h=old.size; sh=h//states
 im=Image.new('RGB',(w,h),MAG)
 for i in range(states):
  tile=Image.new('RGB',(w,sh),MAG);d=ImageDraw.Draw(tile);fill=ACTIVE if i==0 else INACTIVE
  if kind=='top':d.rounded_rectangle((0,0,w-1,sh+15),radius=7,fill=fill,outline=BORDER,width=1)
  elif kind=='bottom':d.rounded_rectangle((0,-15,w-1,sh-1),radius=7,fill=fill,outline=BORDER,width=1)
  else:
   d.rectangle((0,0,w-1,sh-1),fill=fill);x=0 if kind=='left' else w-1;d.line((x,0,x,sh-1),fill=BORDER)
  im.paste(tile,(0,i*sh))
 im.save(p)
for n in ['FRAMECAPTION','FRAMECAPTIONSIZING','FRAMECAPTIONMIN','SMALLFRAMECAPTION','SMALLFRAMECAPTIONSIZING','FRAMEMAXIMIZED']:
 frames(n)
frames('FRAMEBOTTOM',kind='bottom');frames('FRAMELEFT',kind='left');frames('FRAMERIGHT',kind='right')
def buttons(name,count,caption=False,close=False):
 p=B/('LIGHT_'+name+'.bmp');w,h=Image.open(p).size;sh=h//count;im=Image.new('RGB',(w,h),MAG)
 for i in range(count):
  tile=Image.new('RGB',(w,sh),MAG);d=ImageDraw.Draw(tile)
  fill=(255,255,255);edge=(199,205,214)
  if caption:fill=ACTIVE;edge=ACTIVE
  if i%4==1:fill=(229,234,240);edge=(184,193,205)
  if i%4==2:fill=(217,229,232);edge=(13,115,119)
  if close and i%4 in (1,2):fill=(232,86,86);edge=fill
  if i%4==3:fill=(244,245,247);edge=(220,224,230)
  d.rounded_rectangle((0,0,w-1,sh-1),radius=4 if not caption else 2,fill=fill,outline=edge,width=1)
  if name=='BUTTON' and i==4:d.rounded_rectangle((0,0,w-1,sh-1),radius=4,outline=(13,115,119),width=1)
  im.paste(tile,(0,i*sh))
 im.save(p)
buttons('BUTTON',5);buttons('CAPTIONBUTTON',8,caption=True);buttons('CLOSEBUTTON',8,caption=True,close=True);buttons('SMALLCLOSEBUTTON',8,caption=True,close=True)
# Flat interior chrome, original procedural artwork. Preserve every atlas size.
def flat(name,color=(247,248,250)):
 p=B/('LIGHT_'+name+'.bmp');Image.new('RGB',Image.open(p).size,color).save(p)
for n in ['TOOLBARBACKGROUND','EXPLORERBARTOOLBARBACKGROUND','SCROLLSHAFTHORIZONTAL','SCROLLSHAFTVERTICAL']:
 flat(n)
for n in ['TOOLBARGRIPPER','TOOLBARGRIPPERVERT','SCROLLTHUMBGRIPPERHORIZONTAL','SCROLLTHUMBGRIPPERVERTICAL']:
 flat(n,MAG)
for n in ['TOOLBARBUTTONS','TOOLBARBUTTONSSPLIT','TOOLBARBUTTONSSPLITDROPDOWN','EXPLORERBARTOOLBARBUTTONS','EXPLORERBARTOOLBARBUTTONSSPLIT','EXPLORERBARTOOLBARBUTTONSSPLITDROPDOWN']:
 p=B/('LIGHT_'+n+'.bmp');w,h=Image.open(p).size;sh=h//6;im=Image.new('RGB',(w,h),MAG);d=ImageDraw.Draw(im)
 for state in [1,2,4,5]:
  color=(231,239,242) if state==1 else (218,237,237)
  d.rounded_rectangle((0,state*sh,w-1,(state+1)*sh-1),radius=3,fill=color)
 im.save(p)
for n,vertical in [('SCROLLTHUMBVERTICAL',True),('SCROLLTHUMBHORIZONTAL',False)]:
 p=B/('LIGHT_'+n+'.bmp');w,h=Image.open(p).size;sh=h//4;im=Image.new('RGB',(w,h),(247,248,250));d=ImageDraw.Draw(im)
 for state,color in enumerate([(151,162,175),(122,135,150),(87,108,125),(210,216,224)]):
  y=state*sh
  box=(w//2-2,y+2,w//2+2,y+sh-3) if vertical else (2,y+sh//2-2,w-3,y+sh//2+2)
  d.rounded_rectangle(box,radius=2,fill=color)
 im.save(p)
def arrow_atlas(name,count=16):
 p=B/('LIGHT_'+name+'.bmp');w,h=Image.open(p).size;sh=h//count;im=Image.new('RGB',(w,h),(247,248,250));d=ImageDraw.Draw(im)
 for state in range(count):
  direction=state//4;cx=w//2;cy=state*sh+sh//2;r=max(2,min(w,sh)//4);col=(47,58,69) if state%4!=3 else (167,176,187)
  coords=[(-r,1),(0,-r+1),(r,1)]
  if direction==1:coords=[(-r,-1),(0,r-1),(r,-1)]
  elif direction==2:coords=[(1,-r),(-r+1,0),(1,r)]
  elif direction==3:coords=[(-1,-r),(r-1,0),(-1,r)]
  d.line([(cx+x,cy+y) for x,y in coords],fill=col,width=1)
 im.save(p)
for n in ['SCROLLARROWS','SCROLLARROWGLYPHS','SCROLLARROWGLYPHSSMALL']:arrow_atlas(n)
p=B/'LIGHT_COMBOBUTTON.bmp';w,h=Image.open(p).size;sh=h//4;im=Image.new('RGB',(w,h),(247,248,250));d=ImageDraw.Draw(im)
for state in range(4):
 d.rounded_rectangle((0,state*sh,w-1,(state+1)*sh-1),radius=3,fill=(255,255,255) if state==0 else (233,240,243),outline=(207,214,222))
im.save(p)
p=B/'LIGHT_COMBOBUTTONGLYPH.bmp';w,h=Image.open(p).size;sh=h//4;im=Image.new('RGB',(w,h),MAG);d=ImageDraw.Draw(im)
for state in range(4):
 cx=w//2;cy=state*sh+sh//2;d.line([(cx-2,cy-1),(cx,cy+1),(cx+2,cy-1)],fill=(47,58,69) if state!=3 else (167,176,187),width=1)
im.save(p)
# Flat tab surfaces and native checkbox/radio state atlases. Geometry and
# Win32 state ordering stay unchanged; these are procedural presentation assets.
for p in B.glob('LIGHT_TABITEM*.bmp'):
 w,h=Image.open(p).size;sh=h//5;im=Image.new('RGB',(w,h),MAG)
 for state in range(5):
  tile=Image.new('RGB',(w,sh),MAG);d=ImageDraw.Draw(tile)
  fill=(247,248,250) if state not in (1,2) else ((232,243,242) if state==1 else (255,255,255))
  d.rounded_rectangle((0,0,w-1,sh+2),radius=3,fill=fill,outline=(216,222,230),width=1)
  if state==2:d.rectangle((4,sh-2,w-5,sh-1),fill=(13,115,119))
  im.paste(tile,(0,state*sh))
 im.save(p)
for name in ['LIGHT_TABBACKGROUND.bmp','LIGHT_TABBACKGROUND133.bmp']:
 p=B/name;im=Image.open(p);Image.new('RGB',im.size,(247,248,250)).save(p)
p=B/'LIGHT_TABPANEEDGE.bmp';w,h=Image.open(p).size;im=Image.new('RGB',(w,h),(255,255,255));d=ImageDraw.Draw(im);d.rectangle((0,0,w-1,h-1),outline=(214,220,228),width=1);im.save(p)
for size in [13,16,25]:
 for radio in [False,True]:
  count=8 if radio else 12;scale=4;im=Image.new('RGBA',(size*scale,size*count*scale),(0,0,0,0));d=ImageDraw.Draw(im)
  for state in range(count):
   group=state//4;disabled=state%4==3;hot=state%4==1;pressed=state%4==2;y=state*size*scale
   pad=1*scale;edge=(160,169,180) if disabled else ((13,115,119) if hot or pressed or group else (112,125,140))
   fill=(241,243,246) if disabled else (((8,88,94) if pressed else ((10,135,139) if hot else (13,115,119))) if group else ((214,234,234) if pressed else ((232,243,242) if hot else (255,255,255))))
   box=(pad,y+pad,(size-1)*scale-pad,y+(size-1)*scale-pad)
   if radio:d.ellipse(box,fill=fill,outline=edge,width=scale)
   else:d.rounded_rectangle(box,radius=2*scale,fill=fill,outline=edge,width=scale)
   ink=(167,176,187) if disabled else (255,255,255)
   if group==1:
    if radio:
     inset=4*scale;d.ellipse((inset,y+inset,(size-1)*scale-inset,y+(size-1)*scale-inset),fill=ink)
    else:
     d.line([(3*scale,y+size*scale//2),(5*scale,y+(size-4)*scale),((size-3)*scale,y+3*scale)],fill=ink,width=scale+scale//2)
   elif group==2:d.line((3*scale,y+size*scale//2,(size-4)*scale,y+size*scale//2),fill=ink,width=scale+scale//2)
  small=im.resize((size,size*count),Image.Resampling.LANCZOS);im=Image.new('RGB',small.size,MAG);im.paste(small.convert('RGB'),mask=small.getchannel('A').point(lambda a:255 if a>=128 else 0));im.save(B/(('LIGHT_RADIOBUTTON' if radio else 'LIGHT_CHECKBOX')+str(size)+'.bmp'))
for p in (OUT/'textfiles/Light').glob('*.INI'):
 s=p.read_text()
 for part in ['Button.Checkbox','Button.Radiobutton']:
  token='['+part+']';start=s.lower().find(token.lower());end=s.find('\n[',start+1);end=len(s) if end<0 else end
  section=s[start:end].rstrip()+'\n'
  if not re.search(r'(?im)^Transparent\s*=\s*True\s*$', section):section+='\nTransparent = True\n'
  if not re.search(r'(?im)^TransparentColor\s*=', section):section+='TransparentColor = 255 0 255\n'
  if part=='Button.Radiobutton':section=section.replace('ImageFile2 = Light\\RadioButton13.bmp','ImageFile2 = Light\\RadioButton16.bmp').replace('ImageFile3 = Light\\RadioButton13.bmp','ImageFile3 = Light\\RadioButton25.bmp')
  s=s[:start]+section+s[end:]
 p.write_text(s)
# Flat slider thumbs. Keep every source atlas size and five Win32 states.
for p in B.glob('LIGHT_TRACKBAR*.bmp'):
 w,h=Image.open(p).size;sh=h//5;scale=4;im=Image.new('RGBA',(w*scale,h*scale),(0,0,0,0));d=ImageDraw.Draw(im)
 for state in range(5):
  y=state*sh*scale;disabled=state==4;pressed=state==2;hot=state in (1,3)
  edge=(209,215,223) if disabled else ((13,115,119) if hot or pressed else (157,170,181))
  fill=(237,240,244) if disabled else ((8,88,94) if pressed else ((232,243,242) if hot else (255,255,255)))
  ink=(177,186,196) if disabled else ((255,255,255) if pressed else (13,115,119))
  pad=scale;radius=max(2,min(w,sh)//2-1)*scale
  d.rounded_rectangle((pad,y+pad,(w-1)*scale-pad,y+(sh-1)*scale-pad),radius=radius,fill=fill,outline=edge,width=scale)
  cx=(w-1)*scale//2;cy=y+(sh-1)*scale//2
  if w<sh:box=(cx-scale,cy-3*scale,cx+scale,cy+3*scale)
  else:box=(cx-3*scale,cy-scale,cx+3*scale,cy+scale)
  d.rounded_rectangle(box,radius=scale,fill=ink)
 small=im.resize((w,h),Image.Resampling.LANCZOS);im=Image.new('RGB',small.size,MAG);im.paste(small.convert('RGB'),mask=small.getchannel('A').point(lambda a:255 if a>=128 else 0));im.save(p)
for name in ['LIGHT_SLIDERTRACK.bmp','LIGHT_SLIDERTRACKVERT.bmp']:
 p=B/name;Image.new('RGB',Image.open(p).size,(190,203,213)).save(p)
for p in (OUT/'textfiles/Light').glob('*.INI'):
 s=p.read_text()
 for part in ['TrackBar.Thumb','TrackBar.ThumbBottom','TrackBar.ThumbTop','TrackBar.ThumbLeft','TrackBar.ThumbRight','TrackBar.ThumbVert']:
  token='['+part+']';start=s.lower().find(token.lower());end=s.find('\n[',start+1);end=len(s) if end<0 else end
  section=s[start:end].rstrip()+'\n'
  if not re.search(r'(?im)^Transparent\s*=\s*True\s*$',section):section+='Transparent = True\n'
  if not re.search(r'(?im)^TransparentColor\s*=',section):section+='TransparentColor = 255 0 255\n'
  s=s[:start]+section+s[end:]
 # A zero-width source center (3+3 margins on a 6px track) only draws end caps.
 for part in ['TrackBar.Track','TrackBar.TrackVert']:
  start=s.lower().find(('['+part+']').lower());end=s.find('\n[',start+1);end=len(s) if end<0 else end
  section=re.sub(r'(?im)^SizingMargins\s*=.*$', 'SizingMargins = 2, 2, 2, 2',s[start:end]);s=s[:start]+section+s[end:]
 p.write_text(s)
# Flat native up/down buttons and four-state chevrons, preserving atlas geometry.
for p in B.glob('LIGHT_SPINBUTTONBACKGROUND*.bmp'):
 w,h=Image.open(p).size;sh=h//4;im=Image.new('RGB',(w,h),MAG);d=ImageDraw.Draw(im)
 for state in range(4):
  y=state*sh;fill=[(255,255,255),(232,243,242),(8,88,94),(247,248,250)][state];edge=[(202,208,216),(13,115,119),(8,88,94),(226,231,237)][state]
  d.rounded_rectangle((0,y,w-1,y+sh-1),radius=2,fill=fill,outline=edge,width=1)
 im.save(p)
for direction in ['UP','DOWN','LEFT','RIGHT']:
 p=B/('LIGHT_SPIN'+direction+'GLYPH.bmp');w,h=Image.open(p).size;sh=h//4;im=Image.new('RGB',(w,h),MAG);d=ImageDraw.Draw(im)
 points={'UP':[(1,3),(3,1),(5,3)],'DOWN':[(1,2),(3,4),(5,2)],'LEFT':[(3,1),(1,3),(3,5)],'RIGHT':[(2,1),(4,3),(2,5)]}[direction]
 for state in range(4):
  ink=[(47,58,69),(13,115,119),(255,255,255),(167,176,187)][state];d.line([(x,y+state*sh) for x,y in points],fill=ink,width=1)
 im.save(p)
(OUT/'KUSH-SOURCE.txt').write_text('KushFluent is derived from the Modern theme already shipped in this ReactOS/Kush source tree. Original GPLv3 LICENSE is preserved. Existing controls, glyphs and dark variant retain their original authorship. Light frame, button, tab, checkbox, radio, slider and spin-button bitmaps are original procedural drawings from tools/theme/gen_kushfluent.py. No Microsoft Windows bitmap, logo or font is included.\n')
for p in (OUT/'lang').glob('*.rc'):
 s=p.read_text().replace('\"Modern\"','\"Kush Fluent\"');p.write_text(s)
print(OUT)
