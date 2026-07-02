# main src wreck project
cflags = -std=c99 -march=znver3 -O2 -Wall -Iext/inc
ldflags = -Lext/lib -lglfw3 -lgdi32 -lvulkan-1 -lkernel32 -luser32 -lshell32

debug:
	clang $(cflags)                         \
	src/main.c                              \
	src/gpu/gpu.c			        		\
	src/gpu/gpu_resources.c                 \
	src/gpu/gpu_shaders.c                   \
	src/gpu/gpu_render.c                    \
	src/usr/graphics/graphics.c				\
	src/usr/level.c 		 				\
	src/res/res.c                           \
	-o out/bin/wreck.exe $(ldflags)	
	
asm:
	clang $(cflags)         				\
	-S -masm=intel                          \
	src/main.c                              \
	src/graphics/graphics.c			        \
	src/graphics/render.c                   \
	src/loader/loader.c

run:
	./out/bin/wreck.exe

#shader compilation
vs_cflags = -spirv -T vs_6_0 -E main_vertex
fs_cflags = -spirv -T ps_6_0 -E main_fragment
cs_cflags = -spirv -T cs_6_0 -E main_compute

shaders:
	dxc $(vs_cflags) res/copy_color.hlsl -Fo res/spv/copy_color_v.spv
	dxc $(fs_cflags) res/copy_color.hlsl -Fo res/spv/copy_color_f.spv
	dxc $(vs_cflags) res/copy_depth.hlsl -Fo res/spv/copy_depth_v.spv
	dxc $(fs_cflags) res/copy_depth.hlsl -Fo res/spv/copy_depth_f.spv
	dxc $(vs_cflags) res/water_surface.hlsl -Fo res/spv/water_surface_v.spv
	dxc $(fs_cflags) res/water_surface.hlsl -Fo res/spv/water_surface_f.spv
	dxc $(vs_cflags) res/skybox.hlsl -Fo res/spv/skybox_v.spv
	dxc $(fs_cflags) res/skybox.hlsl -Fo res/spv/skybox_f.spv
	dxc $(vs_cflags) res/water_underwater.hlsl -Fo res/spv/water_underwater_v.spv
	dxc $(fs_cflags) res/water_underwater.hlsl -Fo res/spv/water_underwater_f.spv

models:
	py tool/models_join.py res/gltf out/data/models.bin src/files/models_auto.h
