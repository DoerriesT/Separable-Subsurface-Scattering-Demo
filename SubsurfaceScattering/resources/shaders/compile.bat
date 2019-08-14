glslc --target-env=vulkan1.0 -O -Werror -c shadow_vert.vert -o shadow_vert.spv
glslc --target-env=vulkan1.0 -O -Werror -c lighting_vert.vert -o lighting_vert.spv
glslc --target-env=vulkan1.0 -O -Werror -c lighting_frag.frag -o lighting_frag.spv

pause