import socket,time,sys
SP=sys.argv[1]; x=int(sys.argv[2]); y=int(sys.argv[3])
s=socket.socket(socket.AF_UNIX);s.connect(SP+'/mon.sock');s.recv(4096)
def c(m): s.send((m+'\n').encode()); time.sleep(0.15); s.recv(65536)
for i in range(30): c('mouse_move -100 -100')
while x>0 or y>0:
    dx=min(x,40); dy=min(y,40); c('mouse_move %d %d'%(dx,dy)); x-=dx; y-=dy
time.sleep(0.5); c('mouse_button 1'); time.sleep(0.2); c('mouse_button 0')
