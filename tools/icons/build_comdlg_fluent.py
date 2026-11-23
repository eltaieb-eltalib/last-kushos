from pathlib import Path
import io,re
import cairosvg
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
SOURCES=ROOT/'media/icons/fluent-system'
OUT=ROOT/'dll/win32/comdlg32/res/bitmaps/kush-file-toolbar.bmp'
ICONS=[('arrow_up_24_regular.svg','#087F83'),('desktop_48_filled.svg','#087F83'),('folder_add_24_regular.svg','#B47D08'),('list_24_regular.svg','#334B5C'),('table_24_regular.svg','#334B5C')]
atlas=Image.new('RGB',(24*len(ICONS),24),(255,0,255))
for index,(name,color) in enumerate(ICONS):
    svg=(SOURCES/name).read_text()
    svg=re.sub(r'<svg\b','<svg fill="'+color+'"',svg,count=1).replace('fill="#212121"','fill="'+color+'"').replace('fill="currentColor"','fill="'+color+'"')
    png=cairosvg.svg2png(bytestring=svg.encode(),output_width=72,output_height=72)
    image=Image.open(io.BytesIO(png)).convert('RGBA').resize((24,24),Image.Resampling.LANCZOS)
    bg=Image.new('RGB',image.size,(247,248,250));bg.paste(image,mask=image.getchannel('A'))
    mask=image.getchannel('A').point(lambda v:255 if v>20 else 0)
    tile=Image.new('RGB',image.size,(255,0,255));tile.paste(bg,mask=mask)
    atlas.paste(tile,(index*24,0))
atlas.save(OUT)
print(OUT)
