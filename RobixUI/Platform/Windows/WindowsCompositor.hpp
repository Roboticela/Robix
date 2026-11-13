#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

namespace Robix::UI {

struct RoboCompositor;

/* Retained tiles on the GPU. The window draws those tiles instead of copying a bitmap into the swap chain. */
RoboCompositor* RoboCompositorCreate(ID3D11Device* device);
void RoboCompositorDestroy(RoboCompositor* compositor);

/*
 * Upload the damaged tiles and draw the window.
 * scale is 1 when the bitmap already matches the screen. A live pinch passes the extra magnification.
 * Returns 1 when the frame was presented, 0 when the screen is busy, and -1 when this path cannot run.
 */
int RoboCompositorPublish(RoboCompositor* compositor, ID3D11DeviceContext* context, IDXGISwapChain* swap,
                          const unsigned char* pixels, unsigned width, unsigned height, unsigned pitch, bool full,
                          const int* damageX, const int* damageY, const int* damageW, const int* damageH, int damageCount,
                          float scale, float anchorX, float anchorY);

}
