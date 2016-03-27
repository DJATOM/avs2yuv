avs2yuv
=======

A simple tool for executing avisynth scripts.

Compile
=======

gcc avs2yuv.c -o avs2yuv.exe -O3 -ffast-math -Wall -Wshadow -Wempty-body -I. -std=gnu99 -fomit-frame-pointer -s -fno-tree-vectorize -fno-zero-initialized-in-bss 
=== or ===
icl /Tc avs2yuv.c -O3 -W0 -I. -Qstd=c99 -o avs2yuv.exe