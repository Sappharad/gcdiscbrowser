PATH = C:\gcdev\devkitPPC_r12\bin

del main.elf
del main.bin
del main.dol
del main.o

powerpc-gekko-gcc -O2 -I C:\gcdev\gclib-april05\include -c main.c -Wa,-mregnames
powerpc-gekko-gcc -mgcn -Wl,--section-start,.init=0x80003100 main.o -lGC -lm -o main.elf
powerpc-gekko-objcopy -O binary main.elf main.dol
pause