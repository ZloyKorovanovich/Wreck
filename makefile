# main src wreck project
cflags = -std=c99 -march=znver3 -O2 -Wall -Iext/inc
ldflags = -Lext/lib -lglfw3 -lgdi32 -lvulkan-1 -lkernel32 -luser32 -lshell32

debug:
	clang $(cflags) -DDEBUG_LOG				\
	src/main.c                              \
	src/render/vulkan.c			            \
	-o out/bin/wreck.exe $(ldflags)

release:
	clang $(cflags)                         \
	src/main.c                              \
	src/render/vulkan.c			            \
	-o out/bin/wreck.exe $(ldflags)
	
run:
	./out/bin/wreck.exe

#shader compilation
vs_cflags = -spirv -T vs_6_0 -E mainVertex
fs_cflags = -spirv -T ps_6_0 -E mainFragment
cs_cflags = -spirv -T cs_6_0 -E mainCompute

shaders:
	dxc $(vs_cflags) res/triangle.hlsl -Fo res/spv/triangle_v.spv
	dxc $(fs_cflags) res/triangle.hlsl -Fo res/spv/triangle_f.spv
	dxc $(vs_cflags) res/blit.hlsl -Fo res/spv/blit_v.spv
	dxc $(fs_cflags) res/blit.hlsl -Fo res/spv/blit_f.spv

models:
	py tool/models_join.py res/gltf out/data/models.bin src/files/models_auto.h
