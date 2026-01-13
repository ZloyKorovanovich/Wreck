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

run:
	./out/bin/main.exe

#shader compilation
vs_cflags = -spirv -T vs_6_0 -E vertexMain
fs_cflags = -spirv -T ps_6_0 -E fragmentMain
cs_cflags = -spirv -T cs_6_0 -E computeMain

shaders:
# car graphics
	dxc $(vs_cflags) res/car.hlsl -Fo out/shaders/car_v.spv
	dxc $(fs_cflags) res/car.hlsl -Fo out/shaders/car_f.spv
# road graphics
	dxc $(vs_cflags) res/road.hlsl -Fo out/shaders/road_v.spv
	dxc $(fs_cflags) res/road.hlsl -Fo out/shaders/road_f.spv
# traffic compute
	dxc $(cs_cflags) res/traffic.hlsl -Fo out/shaders/traffic_c.spv 

clean_shaders:
	rm out/shaders/*.spv

#tooling
tools:
	clang $(cflags) tool/src/objparse.c -o tool/objparse.exe -Lext/lib -fuse-ld=lld-link -luser32 -lkernel32 -lshell32
	clang $(cflags) tool/src/dirview.c -o tool/dirview.exe -Lext/lib -fuse-ld=lld-link -luser32 -lkernel32 -lshell32

clean_tools:
	rm tool/*.exe

#3d models
models:
	tool/obj_parser res/models/test_model.obj out/models/test_model.mesh
