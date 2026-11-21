import socket,time,sys
SP=sys.argv[1]; txt=sys.argv[2]
s=socket.socket(socket.AF_UNIX);s.connect(SP+'/mon.sock');s.recv(4096)
m={' ':'spc','\\':'backslash',':':'shift-semicolon','.':'dot','_':'shift-minus','-':'minus','/':'slash','>':'shift-dot','"':'shift-apostrophe','*':'shift-8','%':'shift-5','|':'shift-backslash','=':'equal'}
for ch in txt:
    if ch=='\n': k='ret'
    elif ch in m: k=m[ch]
    elif ch.isupper(): k='shift-'+ch.lower()
    else: k=ch
    s.send(('sendkey '+k+'\n').encode()); time.sleep(0.3); s.recv(65536)
