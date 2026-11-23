#!/usr/bin/env python3
"""Generate original small WAV/AVI fixtures. Requires ffmpeg for testsrc only."""
import math
from pathlib import Path
import struct
import subprocess
import sys
import wave

out = Path(sys.argv[1] if len(sys.argv) > 1 else '.')
out.mkdir(parents=True, exist_ok=True)
with wave.open(str(out / 'tone.wav'), 'wb') as wav:
    wav.setparams((1, 2, 8000, 0, 'NONE', 'not compressed'))
    wav.writeframes(b''.join(struct.pack('<h', int(5000 * math.sin(2 * math.pi * 440 * i / 8000)))
                             for i in range(160000)))
avi = out / 'msvc.avi'
subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-f', 'lavfi',
                '-i', 'testsrc=size=320x240:rate=4:duration=12', '-c:v', 'msvideo1',
                '-pix_fmt', 'rgb555le', str(avi)], check=True)
print('Created original 20-second PCM WAV and 12-second MSVideo1 AVI:', out)
