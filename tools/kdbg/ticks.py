import gdb,struct,time
inf=gdb.selected_inferior()
def q(a): return struct.unpack('<Q',bytes(inf.read_memory(a,8)))[0]
def d(a): return struct.unpack('<I',bytes(inf.read_memory(a,4)))[0]
K=0xFFFFF80000000000
def snap():
    return dict(tick=q(K+0x56b510), inc=d(K+0x56b500), maxinc=d(K+0x56b508), it=q(0xFFFFF78000000008), st=q(0xFFFFF78000000014))
print(snap())
