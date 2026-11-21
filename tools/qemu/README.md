# أدوات تجربة Kush في QEMU

- `run_vm.sh <workdir> <disk.raw> [iso]`: بيقلع قرص Kush مثبت من غير شاشة، ويفتح cmd.
- `vm.py`: أوامر للـ QEMU monitor: `shot:name` (صورة)، `sleep:N`، وأسماء المفاتيح (`ret`، `esc`، `meta_l-r`…).
- `typ.py`: كتابة نص بالكيبورد.
- `click.py`: ضغطة ماوس في مكان محدد على الشاشة (بيرجع الماوس للركن الأول، وبعدها بيحركه).
- `serial.log`: فيه كل رسائل النظام (`DbgPrint`)، ومنها `KUSH-EXC` لأي برنامج بيقفل.
- **نسخ ملف مبني جوه القرص من غير إعادة تثبيت:**
  `OFF=$(python3 -c "import struct;d=open('k.raw','rb').read(512);print(struct.unpack('<I',d[454:458])[0]*512)")`
  ثم `mcopy -o -i k.raw@@$OFF build/path/file.dll ::/ReactOS/system32/` (والـ VM مقفول).
- gdb: من الـ monitor `gdbserver tcp::1234`، وبعدها `gdb -ex 'target remote :1234'`. لو الاتصال علّق، جرّب
  `gdbserver none` وبعدها بورت جديد.
