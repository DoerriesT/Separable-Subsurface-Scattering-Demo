glslc --target-env=vulkan1.0 -O -Werror -c shadow_vert.vert -o shadow_vert.spv
glslc --target-env=vulkan1.0 -O -Werror -c lighting_vert.vert -o lighting_vert.spv
glslc --target-env=vulkan1.0 -O -Werror -c lighting_frag.frag -o lighting_frag.spv
glslc --target-env=vulkan1.0 -O -Werror -c -DSSS=1 lighting_frag.frag -o lighting_frag_SSS.spv
glslc --target-env=vulkan1.0 -O -Werror -c skybox_vert.vert -o skybox_vert.spv
glslc --target-env=vulkan1.0 -O -Werror -c skybox_frag.frag -o skybox_frag.spv
glslc --target-env=vulkan1.0 -O -Werror -c fullscreen_vert.vert -o fullscreen_vert.spv
glslc --target-env=vulkan1.0 -O -Werror -c sssBlur_comp.comp -o sssBlur_comp.spv
glslc --target-env=vulkan1.0 -O -Werror -c postprocess_comp.comp -o postprocess_comp.spv

pause