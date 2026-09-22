# Experimental UI model

This directory preserves the direction of the former `ui_typedef.h` and
`new_extract/` drafts without mixing them into the working renderer.

- `ui_object.h` is a platform-neutral scene/event/animation data model.
- `block_pool.*` is a fixed-size framebuffer-block pool intended for MCUs.

They are intentionally not part of the CMake target yet. Their ownership,
allocation-failure policy, event routing and animation scheduler still need a
single design before they should become public runtime APIs. None of these
headers includes SDL.
