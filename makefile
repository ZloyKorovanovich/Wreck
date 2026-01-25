# base library
base_lib:
	clang -c -std=c99 -O2 -Wall base/base.c -o ext/lib/base.obj
	llvm-lib ext/lib/base.obj /out:ext/lib/base.lib
	cp base/base.h ext/inc/base.h
	rm ext/lib/base.obj

base_test:
	clang -std=c99 -O2 -Wall -Iext/inc base/test.c -o base/test.exe -Lext/lib -lbase

# main src wreck project
cflags = -std=c99 -march=znver3 -O2 -Wall -Iext/inc
ldflags = -Lext/lib -lbase -fuse-ld=lld-link -lglfw3 -lgdi32 -lvulkan-1 -luser32 -lshell32

wreck:
	clang $(cflags) src/main.c src/vulkan/vulkan.c src/vulkan/render.c -o out/bin/wreck.exe $(ldflags) 

run:
	./out/bin/wreck.exe

#shader compilation
vs_cflags = -spirv -T vs_6_0 -E vertexMain
fs_cflags = -spirv -T ps_6_0 -E fragmentMain
cs_cflags = -spirv -T cs_6_0 -E computeMain

shaders:
	dxc $(vs_cflags) res/triangle.hlsl -Fo out/data/triangle_v.spv
	dxc $(fs_cflags) res/triangle.hlsl -Fo out/data/triangle_f.spv
	dxc $(vs_cflags) res/default.hlsl -Fo out/data/default_v.spv
	dxc $(fs_cflags) res/default.hlsl -Fo out/data/default_f.spv

# tools
tools:
	clang $(cflags) tool/src/modelconstruct.c -o tool/modelconstruct.exe $(ldflags) 

