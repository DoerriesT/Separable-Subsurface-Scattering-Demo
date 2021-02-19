# Separable Subsurface Scattering Demo
Implementation of the Separable Subsurface Scattering technique from http://www.iryoku.com/separable-sss/ in C++ and Vulkan 1.0.
Additionally this demo features physically-based shading, image-based lighting, shadow mapping and temporal anti-aliasing.

# Controls
- Right click + mouse rotates the camera.
- Mouse scroll wheel zooms in and out.
- A GUI window offers additional options such as toggling SSS or TAA, changing window resolution or adjusting the light position.

# Requirements
- Vulkan 1.0
- 256 MB RAM
- 1 GB video memory

# How to build
The project comes as a Visual Studio 2017 solution and already includes all dependencies. The Application can be build as both x86 and x64.

# Screenshots
Here are some screenshots showcasing the difference that the subsurface scattering effect makes:

| SSS off  | SSS on |
| ------------- | ------------- |
| ![Subsurface Scattering Off 0](sss0off.png?raw=false "Subsurface Scattering Off")| ![Subsurface Scattering On 0](sss0on.png?raw=false "Subsurface Scattering On")  |
| ![Subsurface Scattering Off 2](sss2off.png?raw=false "Subsurface Scattering Off")| ![Subsurface Scattering On 2](sss2on.png?raw=false "Subsurface Scattering On")  |
| ![Subsurface Scattering Off 1](sss1off.png?raw=false "Subsurface Scattering Off")| ![Subsurface Scattering On 1](sss1on.png?raw=false "Subsurface Scattering On")  |

# Credits
- Head mesh https://www.3dscanstore.com/blog/Free-3D-Head-Model
- HDRI Environment https://hdrihaven.com/hdri/?c=indoor&h=de_balie
- tinyobjloader https://github.com/syoyo/tinyobjloader
- Dear ImGui https://github.com/ocornut/imgui
- volk https://github.com/zeux/volk
- GLFW https://www.glfw.org/
- glm https://github.com/g-truc/glm
- gli https://github.com/g-truc/gli
