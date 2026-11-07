#pragma once

#include "RoboGPU.hpp"

/*
 * RoboGPUDevice — one display driver for every GPU.
 * Firmware already lit a linear framebuffer, so that scanout works on Intel, AMD,
 * NVIDIA, and virtual machines without a vendor driver. When a virtio-gpu is
 * present, the same picture is also submitted as a 2D resource.
 */
void RoboGPUDeviceAttach(RoboGPU* gpu);
