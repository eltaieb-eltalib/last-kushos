import socket,time,sys
from PIL import Image
SP=sys.argv[1]
s=socket.socket(socket.AF_UNIX);s.connect(SP+'/mon.sock');s.recv(4096)
def cmd(c): s.send((c+'\n').encode()); time.sleep(0.4); s.recv(65536)
for a in sys.argv[2:]:
    if a.startswith('sleep:'): time.sleep(float(a[6:]))
    elif a.startswith('shot:'):
        cmd('screendump '+SP+'/t.ppm'); Image.open(SP+'/t.ppm').save(SP+'/'+a[5:]+'.png')
    else: cmd('sendkey '+a)
