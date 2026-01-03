cflags = -std=c99 -march=znver3 -O2 -Wall -Iext/inc
ldflags = -Lext/lib 

clean:
	rm out/bin/*.o 
	rm out/bin/*.exe

compile:
	clang -c $(cflags) src/main.c -o out/bin/main.o
	clang -c $(cflags) src/vulkan/vulkan.c -o out/bin/vulkan.o
	clang -c $(cflags) src/vulkan/render.c -o out/bin/render.o

link: 
	clang out/bin/*.o -o out/bin/main.exe $(ldflags) -fuse-ld=lld-link -lglfw3 -lvulkan-1 -lgdi32 -luser32 -lkernel32 -lshell32

shaders:
	dxc -spirv -T vs_6_0 -E vertexMain res/triangle.hlsl -Fo out/data/triangle_v.spv
	dxc -spirv -T ps_6_0 -E fragmentMain res/triangle.hlsl -Fo out/data/triangle_f.spv

	dxc -spirv -T vs_6_0 -E vertexMain res/triangle_flip.hlsl -Fo out/data/triangle_flip_v.spv
	dxc -spirv -T ps_6_0 -E fragmentMain res/triangle_flip.hlsl -Fo out/data/triangle_flip_f.spv

	dxc -spirv -T vs_6_0 -E vertexMain res/cube.hlsl -Fo out/data/cube_v.spv
	dxc -spirv -T ps_6_0 -E fragmentMain res/cube.hlsl -Fo out/data/cube_f.spv

	dxc -spirv -T cs_6_0 -E computeMain res/grid.hlsl -Fo out/data/grid_c.spv 
	