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
