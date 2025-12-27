cflags = -std=c99 -march=znver3 -O2 -Wall -Iext/inc

ldflags = -Lext/lib

clean:
	rm out/bin/main.o 
	rm out/bin/vk.o 
	rm out/bin/main.exe

compile:
	clang -c $(cflags) src/main.c -o out/bin/main.o
	clang -c $(cflags) src/vk/vk.c -o out/bin/vk.o
	clang -c $(cflags) src/vk/render.c -o out/bin/render.o
	clang -c $(cflags) src/vk/vram.c -o out/bin/vram.o

link: 
	clang out/bin/main.o out/bin/vk.o out/bin/render.o out/bin/vram.o -o out/bin/main.exe $(ldflags) -lglfw3 -lvulkan-1 -lgdi32 -luser32 -lkernel32 -lmsvcrt -llibcmt -lshell32

shaders:
	dxc -spirv -T vs_6_0 -E vertexMain res/triangle.hlsl -Fo out/data/triangle_v.spv
	dxc -spirv -T ps_6_0 -E fragmentMain res/triangle.hlsl -Fo out/data/triangle_f.spv
	dxc -spirv -T vs_6_0 -E vertexMain res/triangle_flip.hlsl -Fo out/data/triangle_flip_v.spv
	dxc -spirv -T ps_6_0 -E fragmentMain res/triangle_flip.hlsl -Fo out/data/triangle_flip_f.spv
	