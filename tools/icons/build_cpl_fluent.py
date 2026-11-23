#!/usr/bin/env python3
"""Generate licensed Fluent icons for Control Panel, preserving resource IDs.

Usage: build_cpl_fluent.py /path/to/@fluentui/svg-icons/package
Original artwork remains at its original ID; Fluent artwork uses ID+40000.
ClassicIcons=1 keeps the originals through the opt-in presentation helper.
"""
from pathlib import Path
import io,json,re,shutil,sys
import cairosvg
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[2]
PKG=Path(sys.argv[1])
ASSETS=ROOT/'media/icons/fluent-system'
ASSETS.mkdir(parents=True,exist_ok=True)
SIZES=[16,20,24,32,48,64]
BASE={'console':'window_console','access':'accessibility','hotplug':'usb_plug','input':'keyboard','mmsys':'speaker_2','powercfg':'battery_charge','wined3dcfg':'cube','sysdm':'desktop_tower','openglcfg':'cube','appwiz':'apps','desk':'desktop','inetcpl':'globe','timedate':'calendar_clock','joy':'games','telephon':'phone','hdwwiz':'plug_connected','main':'cursor_click','intl':'local_language','usrmgr':'people'}
SPECIAL={
 'IDI_KEYBOARD':'keyboard','IDC_CPLICON_2':'keyboard','IDI_CPLICON_2':'keyboard',
 'IDI_MIDICON':'music_note_2','IDI_JOY_ICON':'games','IDI_CAMERA_ICON':'camera',
 'IDI_SOUND_SECTION':'music_note_2','IDI_SOUND_ASSIGNED':'speaker_2','IDI_MICROPHONE_ICON':'mic',
 'IDI_NO_HW':'speaker_off','IDI_MUTED_ICON':'speaker_mute','IDI_SHUTDOWN_ICON':'power',
 'IDI_PLAY_ICON':'play','IDI_STOP_ICON':'stop','IDI_SPEAKIMG':'speaker_box',
 'IDI_AC':'plug_connected','IDI_ACDC':'plug_connected','IDI_DC':'battery_10',
 'IDI_SCREEN':'desktop','IDI_HIBERNATE':'weather_moon','IDI_BAT_CHARGE_CRIT':'battery_charge_critical',
 'IDI_BAT_CHARGE_LOW':'battery_charge_low','IDI_BAT_CHARGE_HALF':'battery_charge_half',
 'IDI_BAT_CHARGE_HIGH':'battery_charge_high','IDI_BAT_CHARGE_FULL':'battery_charge_full',
 'IDI_BAT_DISCHRG_CRIT':'battery_warning','IDI_BAT_DISCHRG_LOW':'battery_2',
 'IDI_BAT_DISCHRG_HALF':'battery_5','IDI_BAT_DISCHRG_HIGH':'battery_8',
 'IDI_BAT_DISCHRG_FULL':'battery_10','IDI_BAT_UNKNOWN':'battery_unknown','IDI_BAT_NONE':'battery_none',
 'IDI_USERPROF':'person_settings','IDI_HARDPROF':'desktop_tower','IDI_DEVMGR':'settings',
 'IDI_LICENSE':'certificate','IDI_UP':'chevron_up','IDI_DOWN':'chevron_down','IDI_ADDHW':'plug_connected',
 'IDI_APPICO':'apps','IDI_INSTICO':'arrow_download','IDI_RECBINICO':'delete',
 'IDI_DOWNLOADICO':'arrow_download','IDI_APPINETICO':'globe',
 'ICO_CERTIFICATES':'certificate','ICO_RESTRICTED':'prohibited','ICO_HISTORY':'history',
 'ICO_TRUSTED':'shield_checkmark','ICO_HOME':'home','IDI_WARNINGICON':'warning',
 'IDI_SEARCHICON':'search','IDI_FOLDER_OPEN':'folder_open','IDI_FOLDER_CLOSED':'folder',
 'IDI_MOUSE_LEFT':'cursor_click','IDI_MOUSE_RIGHT':'cursor_click','IDI_MOUSE_SPEED':'cursor',
 'IDI_MOUSE_BUTTON':'cursor_click','IDI_MOUSE_TRAILS':'cursor','IDI_MOUSE_HIDE':'cursor_hover_off',
 'IDI_PRINTERS_FOLDER':'print','IDI_FONTS_FOLDER':'text_font','IDI_ADMIN_TOOLS_FOLDER':'wrench',
 'IDI_MOUSE_POINTER':'cursor','IDI_MOUSE_WHEEL':'arrow_sort','IDI_REPEAT_RATE':'keyboard',
 'IDI_REPEAT_DELAY':'clock','IDI_LOCK_KEY':'lock_closed','IDI_USER':'person',
 'IDI_LOCKED_USER':'person_prohibited','IDI_GROUP':'people',
 '4354':'speaker_2','4363':'speaker_2','4367':'speaker_2','4368':'speaker_2',
 '4369':'mic','4379':'speaker_2','4380':'mic'}
# Explicit alternatives for concepts whose catalogue names differ.
FALLBACK={'cursor_hover_off':['cursor_hover','cursor'],'local_language':['translate','globe'],
 'calendar_clock':['calendar','clock'],'speaker_box':['speaker_2'],'certificate':['ribbon','shield'],
 'person_settings':['person'],'person_prohibited':['person_lock','person'],'folder_open':['folder'],
 'text_font':['text_font_size','text'],'window_console':['code'],'cube':['cube_multiple']}
CACHE={};MAP=ASSETS/'cpl-map.json';ROWS=[x for x in json.loads(MAP.read_text()) if x['resource']!='IDI_DOT'] if MAP.exists() else []
COMPOSITES={'battery_charge_critical':'battery_0','battery_charge_low':'battery_2','battery_charge_half':'battery_5','battery_charge_high':'battery_8','battery_charge_full':'battery_10','battery_unknown':'battery_0','battery_none':'battery_0'}
def glyph(concept):
 concept=COMPOSITES.get(concept,concept)
 for name in [concept]+FALLBACK.get(concept,[]):
  for style in ['color','filled','regular']:
   for size in [48,32,24,20]:
    p=PKG/'icons'/f'{name}_{size}_{style}.svg'
    if p.exists():return p
 raise RuntimeError('No Fluent artwork for '+concept)
def make(concept):
 if concept in CACHE:return CACHE[concept]
 src=glyph(concept);svg=src.read_text();color='#087F83'
 if concept in ['keyboard','desktop','desktop_tower','window_console','print','usb_plug']:color='#1969B3'
 if concept in ['warning','battery_0','prohibited','speaker_off','speaker_mute','delete']:color='#BE3A48'
 if concept in ['games','cube','cube_multiple','mic','music_note_2']:color='#704BB2'
 if '_color.svg' not in src.name:
  svg=re.sub(r'<svg\b','<svg fill="'+color+'"',svg,count=1)
  svg=svg.replace('fill="#212121"','fill="'+color+'"').replace('fill="currentColor"','fill="'+color+'"')
 png=cairosvg.svg2png(bytestring=svg.encode(),output_width=256,output_height=256)
 im=Image.open(io.BytesIO(png)).convert('RGBA')
 if concept in COMPOSITES:
  d=ImageDraw.Draw(im)
  if concept.startswith('battery_charge_'):
   d.ellipse((158,158,254,254),fill='#087F83',outline='white',width=5)
   d.polygon([(212,172),(181,211),(202,211),(195,240),(230,198),(210,198)],fill='white')
  elif concept=='battery_none':d.line((35,215,222,38),fill='#BE3A48',width=15)
  else:
   d.ellipse((158,158,254,254),fill='#59697D',outline='white',width=5)
   d.arc((187,179,225,213),180,465,fill='white',width=7);d.line((206,211,206,224),fill='white',width=7);d.ellipse((202,233,210,241),fill='white')
 out=ASSETS/(concept+'.ico')
 im.save(out,sizes=[(x,x) for x in SIZES],bitmap_format='bmp')
 shutil.copyfile(src,ASSETS/src.name)
 CACHE[concept]=(out,src.name);return CACHE[concept]
for module,default in BASE.items():
 rc=ROOT/'dll/cpl'/module/(module+'.rc')
 if not rc.exists():raise RuntimeError(str(rc))
 text=rc.read_text()
 generated = 'KUSH_CPL_FLUENT' in text
 if generated:
  for m in re.finditer(r'(?m)^(\w+) ICON "([^"]+)"\n\(\1 \+ 40000\) ICON "resources/fluent/([^"]+)"',text):
   concept=SPECIAL.get(m[1],default)
   out,source=make(concept)
   text=re.sub(r'(?m)^\('+re.escape(m[1])+r' \+ 40000\) ICON "resources/fluent/[^"]+"','('+m[1]+' + 40000) ICON "resources/fluent/'+out.name+'"',text)
   local=rc.parent/'resources/fluent';local.mkdir(parents=True,exist_ok=True);shutil.copyfile(out,local/out.name)
   ROWS.append({'module':module,'resource':m[1],'original':m[2],'fluent':concept,'svg':source,'fluent_offset':40000})
 def replace(m):
  ident,old=m.group(1).strip(),m.group(2)
  if ident=='IDI_DOT':return m.group(0)
  original=rc.parent/old
  # Several old resources are intentionally transparent placeholders, not icons.
  try:
   im=Image.open(original).convert('RGBA');alpha=im.getchannel('A');bbox=alpha.getbbox()
   if not bbox or (bbox[2]-bbox[0])*(bbox[3]-bbox[1])<4:return m.group(0)
  except FileNotFoundError:raise
  concept=SPECIAL.get(ident,default);out,source=make(concept)
  local=rc.parent/'resources/fluent';local.mkdir(parents=True,exist_ok=True);shutil.copyfile(out,local/out.name)
  ROWS.append({'module':module,'resource':ident,'original':old,'fluent':concept,'svg':source,'fluent_offset':40000})
  return f'{ident} ICON "{old}"\n({ident} + 40000) ICON "resources/fluent/{out.name}"'
 if not generated:
  text=re.sub(r'(?m)^\s*([\w]+)\s+ICON\s+"([^"]+)"',replace,text)
  rc.write_text('/* KUSH_CPL_FLUENT: generated mapping, original IDs preserved. */\n'+text)
 else:rc.write_text(text)
 # Opt in at the shared project header, so internal dialog icon loads also roll back.
 cm=rc.parent/'CMakeLists.txt';c=cm.read_text();pch=re.search(r'add_pch\([^\s]+\s+([^\s)]+)',c)
 if pch:
  header=rc.parent/pch[1];h=header.read_text()
  if '#include <kushicons.h>' not in h:header.write_text(h+'\n#include <kushicons.h>\n')
  own=rc.parent/(module+'.h')
  if own.exists() and own != header:
   h=own.read_text()
   if '#include <kushicons.h>' not in h:own.write_text(h+'\n#include <kushicons.h>\n')
 else:
  for source in list(rc.parent.glob('*.c'))+list(rc.parent.glob('*.cpp')):
   s=source.read_text(encoding='latin1');inc=list(re.finditer(r'(?m)^#include[^\n]*$',s))
   if inc and '#include <kushicons.h>' not in s:
    pos=inc[-1].end();source.write_text(s[:pos]+'\n#include <kushicons.h>'+s[pos:],encoding='latin1')
 if 'advapi32' not in c:c+=f'\nadd_importlibs({module} advapi32)\n'
 if 'kush_fluent_icon_deps' not in c:
  c+='\nfile(GLOB kush_fluent_icon_deps "${CMAKE_CURRENT_SOURCE_DIR}/resources/fluent/*.ico")\n'
  c+=f'add_rc_deps({module}.rc ${{kush_fluent_icon_deps}})\n'
 cm.write_text(c)
 for source in list(rc.parent.glob('*.c'))+list(rc.parent.glob('*.cpp')):
  s=source.read_text(encoding='latin1');s=re.sub(r'(->idIcon\s*=\s*)([^;\n]+)(;)',lambda m:m[0] if m[2].startswith('KushOwnIconId(') else m[1]+'KushOwnIconId('+m[2]+')'+m[3],s)
  s=re.sub(r'(pszIcon\s*=\s*MAKEINTRESOURCE[AW]?\(\s*)([^()\n]+)(\s*\))',lambda m:m[1]+'KushOwnIconId('+m[2]+')'+m[3],s)
  source.write_text(s,encoding='latin1')
 MAP.write_text(json.dumps(ROWS,indent=2)+'\n')
ROWS=list({(x['module'],x['resource']):x for x in ROWS}.values())
MAP.write_text(json.dumps(ROWS,indent=2)+'\n')
licenses=list(PKG.glob('LICENSE*'))
if licenses:shutil.copyfile(licenses[0],ASSETS/'LICENSE')
if not (ASSETS/'LICENSE').exists():raise RuntimeError('Supply upstream MIT LICENSE in media/icons/fluent-system/LICENSE')
print('Mapped icons:',len(ROWS),'unique glyphs:',len({x['fluent'] for x in ROWS}),'modules:',len(BASE))
