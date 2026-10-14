// OpenBlock software renderer: renders a 3D voxel scene to a binary PPM image.
// Clean-room code.
#pragma once

#include <string>

struct Host; // defined in bindings.h

// Renders the current scene (Parts under Workspace) to `path` as a binary
// PPM (P6) image of `width` x `height` pixels. Returns true on success,
// false on bad dimensions, missing scene, or file errors.
bool oboxRenderPPM(Host* host, const std::string& path, int width, int height);
