# vulkan-rendering-journey

A collection of vulkan renderers, from triangle drawing to global illumination.

Roughly following [Brendan Galea's tutorials](https://www.youtube.com/watch?v=Y9U9IE0gVHA&list=PL8327DO66nu9qYVKLDmdLW_84-yE4auCR) at the moment.

### Dependencies

-   GLFW 3
-   vulkan dev headers

### Build and Run Instructions

This project is using [meson](https://mesonbuild.com/index.html) since it seemed easiest at the time. To build and run the `triangle` project (others follow similarly):

```bash
cd triangle
meson output
meson compile -C output
./output/triangle
```

These instructions can be very mildly simplified after https://github.com/mesonbuild/meson/issues/9584 is solved.

For (probably) optimal performance and using clang+mold:

```bash
cd triangle
CC=clang CXX=clang++ CC_LD=mold CXX_LD=mold meson output
meson compile -j8 -C output
./output/triangle
```

## Reference Links
- <https://vulkano.rs/guide/introduction>
- <https://renderdoc.org/vulkan-layer-guide.html>
- <https://kylemayes.github.io/vulkanalia/>
- <https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html>
- <https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/>
- <https://github.com/ARM-software/vulkan_best_practice_for_mobile_developers#introduction>
- <https://github.com/gwihlidal/ash-nv-rt>
- <https://gpuopen.com/vulkan/#tutorials>
- <https://vkguide.dev/>
- <https://www.intel.com/content/www/us/en/developer/articles/training/api-without-secrets-introduction-to-vulkan-preface.html>
- <https://www.ea.com/frostbite/news/moving-frostbite-to-pb>
- <https://google.github.io/filament/Filament.html>
- <https://github.com/cadenji/foolrenderer>
- <https://raytracing.github.io/>
- <https://github.com/GPSnoopy/RayTracingInVulkan>
- <https://github.com/SaschaWillems/Vulkan>
- <https://thebookofshaders.com/>
- <https://www.thingiverse.com/thing:2984264>
