#include "RoboGPUDevice.hpp"

#include "../RoboKernel/RoboPCI.hpp"
#include "../RoboShared/RoboSerial.hpp"

namespace {

constexpr RoboU16 ROBO_GPU_VENDOR_VIRTIO = 0x1AF4u;
constexpr RoboU16 ROBO_GPU_DEVICE_VIRTIO = 0x1050u;
constexpr RoboU32 ROBO_GPU_CMD_GET_DISPLAY = 0x0100u;
constexpr RoboU32 ROBO_GPU_CMD_CREATE_2D = 0x0101u;
constexpr RoboU32 ROBO_GPU_CMD_SET_SCANOUT = 0x0103u;
constexpr RoboU32 ROBO_GPU_CMD_FLUSH = 0x0104u;
constexpr RoboU32 ROBO_GPU_CMD_TRANSFER = 0x0105u;
constexpr RoboU32 ROBO_GPU_CMD_ATTACH = 0x0106u;
constexpr RoboU32 ROBO_GPU_RESP_OK = 0x1100u;
constexpr RoboU32 ROBO_GPU_RESP_DISPLAY = 0x1101u;
constexpr RoboU32 ROBO_GPU_FORMAT_BGRA = 1u;

struct RoboGPUCtrlHeader {
  RoboU32 type;
  RoboU32 flags;
  RoboU64 fence;
  RoboU32 context;
  RoboU32 padding;
};

struct RoboGPURect {
  RoboU32 x;
  RoboU32 y;
  RoboU32 width;
  RoboU32 height;
};

struct RoboVirtioQueue {
  RoboU16 io;
  RoboU32 size;
  RoboU16 availIndex;
  RoboU16 lastUsed;
  RoboBool ready;
  alignas(4096) RoboU8 bytes[65536];
  alignas(16) RoboU8 command[512];
  alignas(16) RoboU8 response[256];
};

RoboVirtioQueue gRoboVirtio{};
RoboGPU* gRoboVirtioGpu = ROBO_NULL;

void RoboGPUOut8(RoboU16 port, RoboU8 value) {
  __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void RoboGPUOut16(RoboU16 port, RoboU16 value) {
  __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

void RoboGPUOut32(RoboU16 port, RoboU32 value) {
  __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

RoboU8 RoboGPUIn8(RoboU16 port) {
  RoboU8 value = 0;
  __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

RoboU16 RoboGPUIn16(RoboU16 port) {
  RoboU16 value = 0;
  __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

const char* RoboGPUVendorName(RoboU16 vendor) {
  if (vendor == 0x8086u) {
    return "Intel";
  }
  if (vendor == 0x1002u) {
    return "AMD";
  }
  if (vendor == 0x10DEu) {
    return "NVIDIA";
  }
  if (vendor == 0x1234u || vendor == 0x1B36u) {
    return "QEMU";
  }
  if (vendor == ROBO_GPU_VENDOR_VIRTIO) {
    return "VirtIO";
  }
  return "PCI display";
}

void RoboGPULogDisplay(const RoboPciDevice* device) {
  RoboSerialWrite("ROBO_GPU: standard driver on ");
  RoboSerialWrite(RoboGPUVendorName(device->vendorId));
  RoboSerialWrite(" ");
  RoboSerialWriteHex64(device->vendorId);
  RoboSerialWrite(":");
  RoboSerialWriteHex64(device->deviceId);
  RoboSerialWriteLine(" via firmware framebuffer");
}

RoboU16 RoboGPUIoBar(const RoboPciDevice* device) {
  for (RoboU32 i = 0; i < 6; ++i) {
    if ((device->bar[i] & 1u) != 0) {
      return static_cast<RoboU16>(device->bar[i] & ~3u);
    }
  }
  return 0;
}

RoboU8* RoboGPUQueueDesc() {
  return gRoboVirtio.bytes;
}

RoboU8* RoboGPUQueueAvail() {
  return gRoboVirtio.bytes + static_cast<RoboU64>(gRoboVirtio.size) * 16ull;
}

RoboU8* RoboGPUQueueUsed() {
  const RoboU64 avail = static_cast<RoboU64>(gRoboVirtio.size) * 16ull + 6ull + static_cast<RoboU64>(gRoboVirtio.size) * 2ull;
  const RoboU64 aligned = (avail + 4095ull) & ~4095ull;
  return gRoboVirtio.bytes + aligned;
}

RoboBool RoboGPUQueueFits() {
  const RoboU8* used = RoboGPUQueueUsed();
  const RoboU64 usedBytes = 4ull + static_cast<RoboU64>(gRoboVirtio.size) * 8ull;
  return (used + usedBytes) <= (gRoboVirtio.bytes + sizeof(gRoboVirtio.bytes)) ? ROBO_TRUE : ROBO_FALSE;
}

void RoboGPUWriteDesc(RoboU32 index, RoboU64 address, RoboU32 length, RoboU16 flags, RoboU16 next) {
  auto* desc = RoboGPUQueueDesc() + static_cast<RoboU64>(index) * 16ull;
  *reinterpret_cast<RoboU64*>(desc + 0) = address;
  *reinterpret_cast<RoboU32*>(desc + 8) = length;
  *reinterpret_cast<RoboU16*>(desc + 12) = flags;
  *reinterpret_cast<RoboU16*>(desc + 14) = next;
}

RoboBool RoboGPUVirtioCommand(RoboU32 commandBytes, RoboU32 responseBytes) {
  if (gRoboVirtio.ready != ROBO_TRUE || commandBytes > sizeof(gRoboVirtio.command) ||
      responseBytes < 8 || responseBytes > sizeof(gRoboVirtio.response)) {
    return ROBO_FALSE;
  }
  RoboMemorySet(gRoboVirtio.response, 0, responseBytes);
  RoboGPUWriteDesc(0, reinterpret_cast<RoboU64>(gRoboVirtio.command), commandBytes, 1, 1);
  RoboGPUWriteDesc(1, reinterpret_cast<RoboU64>(gRoboVirtio.response), responseBytes, 2, 0);

  auto* avail = RoboGPUQueueAvail();
  *reinterpret_cast<RoboU16*>(avail + 0) = 0;
  const RoboU16 slot = gRoboVirtio.availIndex;
  *reinterpret_cast<RoboU16*>(avail + 4ull + static_cast<RoboU64>(slot % gRoboVirtio.size) * 2ull) = 0;
  ++gRoboVirtio.availIndex;
  __asm__ volatile("mfence" ::: "memory");
  *reinterpret_cast<RoboU16*>(avail + 2) = gRoboVirtio.availIndex;
  __asm__ volatile("mfence" ::: "memory");
  RoboGPUOut16(static_cast<RoboU16>(gRoboVirtio.io + 16u), 0);

  for (RoboU32 spin = 0; spin < 1000000u; ++spin) {
    __asm__ volatile("mfence" ::: "memory");
    const RoboU16 used = *reinterpret_cast<volatile RoboU16*>(RoboGPUQueueUsed() + 2);
    if (used != gRoboVirtio.lastUsed) {
      gRoboVirtio.lastUsed = used;
      const RoboU32 type = *reinterpret_cast<RoboU32*>(gRoboVirtio.response);
      return (type == ROBO_GPU_RESP_OK || type == ROBO_GPU_RESP_DISPLAY) ? ROBO_TRUE : ROBO_FALSE;
    }
  }
  gRoboVirtio.ready = ROBO_FALSE;
  RoboSerialWriteLine("ROBO_GPU: virtio command timed out, firmware scanout stays active");
  return ROBO_FALSE;
}

void RoboGPUFillHeader(RoboU32 type) {
  RoboMemorySet(gRoboVirtio.command, 0, sizeof(gRoboVirtio.command));
  auto* header = reinterpret_cast<RoboGPUCtrlHeader*>(gRoboVirtio.command);
  header->type = type;
}

RoboBool RoboGPUVirtioSetup(const RoboPciDevice* device, RoboGPU* gpu) {
  const RoboU16 io = RoboGPUIoBar(device);
  RoboPciWrite16(device->bus, device->slot, device->func, 4,
                 static_cast<RoboU16>(RoboPciRead16(device->bus, device->slot, device->func, 4) | 0x0007u));
  if (io == 0 || gpu == ROBO_NULL || gpu->front == ROBO_NULL || gpu->info.width == 0 || gpu->info.height == 0) {
    return ROBO_FALSE;
  }
  gRoboVirtio.io = io;
  RoboGPUOut8(static_cast<RoboU16>(io + 18u), 0);
  RoboGPUOut8(static_cast<RoboU16>(io + 18u), 1u);
  RoboGPUOut8(static_cast<RoboU16>(io + 18u), 3u);
  RoboGPUOut32(static_cast<RoboU16>(io + 4u), 0);
  RoboGPUOut16(static_cast<RoboU16>(io + 14u), 0);
  const RoboU16 queueSize = RoboGPUIn16(static_cast<RoboU16>(io + 12u));
  if (queueSize < 2 || queueSize > 256) {
    return ROBO_FALSE;
  }
  gRoboVirtio.size = queueSize;
  if (RoboGPUQueueFits() != ROBO_TRUE) {
    return ROBO_FALSE;
  }
  RoboMemorySet(gRoboVirtio.bytes, 0, sizeof(gRoboVirtio.bytes));
  const RoboU64 queuePhysical = reinterpret_cast<RoboU64>(gRoboVirtio.bytes);
  if ((queuePhysical & 4095ull) != 0 || (queuePhysical >> 32) != 0) {
    return ROBO_FALSE;
  }
  RoboGPUOut32(static_cast<RoboU16>(io + 8u), static_cast<RoboU32>(queuePhysical >> 12));
  RoboGPUOut8(static_cast<RoboU16>(io + 18u), 7u);
  (void)RoboGPUIn8(static_cast<RoboU16>(io + 19u));
  gRoboVirtio.availIndex = 0;
  gRoboVirtio.lastUsed = 0;
  gRoboVirtio.ready = ROBO_TRUE;

  RoboGPUFillHeader(ROBO_GPU_CMD_GET_DISPLAY);
  if (RoboGPUVirtioCommand(sizeof(RoboGPUCtrlHeader), 24) != ROBO_TRUE) {
    gRoboVirtio.ready = ROBO_FALSE;
    return ROBO_FALSE;
  }

  RoboGPUFillHeader(ROBO_GPU_CMD_CREATE_2D);
  auto* create = gRoboVirtio.command + sizeof(RoboGPUCtrlHeader);
  *reinterpret_cast<RoboU32*>(create + 0) = 1;
  *reinterpret_cast<RoboU32*>(create + 4) = ROBO_GPU_FORMAT_BGRA;
  *reinterpret_cast<RoboU32*>(create + 8) = gpu->info.width;
  *reinterpret_cast<RoboU32*>(create + 12) = gpu->info.height;
  if (RoboGPUVirtioCommand(static_cast<RoboU32>(sizeof(RoboGPUCtrlHeader) + 16), 24) != ROBO_TRUE) {
    gRoboVirtio.ready = ROBO_FALSE;
    return ROBO_FALSE;
  }

  RoboGPUFillHeader(ROBO_GPU_CMD_ATTACH);
  auto* attach = gRoboVirtio.command + sizeof(RoboGPUCtrlHeader);
  *reinterpret_cast<RoboU32*>(attach + 0) = 1;
  *reinterpret_cast<RoboU32*>(attach + 4) = 1;
  *reinterpret_cast<RoboU64*>(attach + 8) = reinterpret_cast<RoboU64>(gpu->front);
  *reinterpret_cast<RoboU32*>(attach + 16) = gpu->info.height * RoboGPUPitch(gpu);
  *reinterpret_cast<RoboU32*>(attach + 20) = 0;
  if (RoboGPUVirtioCommand(static_cast<RoboU32>(sizeof(RoboGPUCtrlHeader) + 24), 24) != ROBO_TRUE) {
    gRoboVirtio.ready = ROBO_FALSE;
    return ROBO_FALSE;
  }

  RoboGPUFillHeader(ROBO_GPU_CMD_SET_SCANOUT);
  auto* scan = reinterpret_cast<RoboGPURect*>(gRoboVirtio.command + sizeof(RoboGPUCtrlHeader));
  scan->x = 0;
  scan->y = 0;
  scan->width = gpu->info.width;
  scan->height = gpu->info.height;
  *reinterpret_cast<RoboU32*>(gRoboVirtio.command + sizeof(RoboGPUCtrlHeader) + sizeof(RoboGPURect)) = 0;
  *reinterpret_cast<RoboU32*>(gRoboVirtio.command + sizeof(RoboGPUCtrlHeader) + sizeof(RoboGPURect) + 4) = 1;
  if (RoboGPUVirtioCommand(static_cast<RoboU32>(sizeof(RoboGPUCtrlHeader) + sizeof(RoboGPURect) + 8), 24) != ROBO_TRUE) {
    gRoboVirtio.ready = ROBO_FALSE;
    return ROBO_FALSE;
  }

  gRoboVirtioGpu = gpu;
  RoboSerialWriteLine("ROBO_GPU: virtio-gpu 2D ready");
  return ROBO_TRUE;
}

RoboBool RoboGPUVirtioScanout(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height) {
  if (gRoboVirtio.ready != ROBO_TRUE || gpu == ROBO_NULL || gpu != gRoboVirtioGpu || width <= 0 || height <= 0) {
    return ROBO_FALSE;
  }
  RoboGPURect rect{static_cast<RoboU32>(x), static_cast<RoboU32>(y), static_cast<RoboU32>(width),
                   static_cast<RoboU32>(height)};
  RoboGPUFillHeader(ROBO_GPU_CMD_TRANSFER);
  *reinterpret_cast<RoboGPURect*>(gRoboVirtio.command + sizeof(RoboGPUCtrlHeader)) = rect;
  *reinterpret_cast<RoboU64*>(gRoboVirtio.command + sizeof(RoboGPUCtrlHeader) + sizeof(RoboGPURect)) = 0;
  *reinterpret_cast<RoboU32*>(gRoboVirtio.command + sizeof(RoboGPUCtrlHeader) + sizeof(RoboGPURect) + 8) = 1;
  if (RoboGPUVirtioCommand(static_cast<RoboU32>(sizeof(RoboGPUCtrlHeader) + sizeof(RoboGPURect) + 16), 24) != ROBO_TRUE) {
    return ROBO_FALSE;
  }
  RoboGPUFillHeader(ROBO_GPU_CMD_FLUSH);
  *reinterpret_cast<RoboGPURect*>(gRoboVirtio.command + sizeof(RoboGPUCtrlHeader)) = rect;
  *reinterpret_cast<RoboU32*>(gRoboVirtio.command + sizeof(RoboGPUCtrlHeader) + sizeof(RoboGPURect)) = 1;
  return RoboGPUVirtioCommand(static_cast<RoboU32>(sizeof(RoboGPUCtrlHeader) + sizeof(RoboGPURect) + 8), 24);
}

}  // namespace

void RoboGPUDeviceAttach(RoboGPU* gpu) {
  RoboPciManager pci{};
  RoboPciInit(&pci);
  RoboPciScan(&pci);
  RoboPciDevice display{};
  RoboBool found = ROBO_FALSE;
  for (RoboU32 i = 0; i < pci.deviceCount; ++i) {
    if (pci.devices[i].baseClass == 0x03u) {
      display = pci.devices[i];
      found = ROBO_TRUE;
      RoboGPULogDisplay(&display);
    }
  }
  if (found != ROBO_TRUE) {
    RoboSerialWriteLine("ROBO_GPU: standard driver via firmware framebuffer");
  }

  RoboPciDevice virtio{};
  if (RoboPciFindDevice(&pci, ROBO_GPU_VENDOR_VIRTIO, ROBO_GPU_DEVICE_VIRTIO, &virtio) == ROBO_TRUE ||
      (found == ROBO_TRUE && display.vendorId == ROBO_GPU_VENDOR_VIRTIO)) {
    const RoboPciDevice* chosen = virtio.vendorId == ROBO_GPU_VENDOR_VIRTIO ? &virtio : &display;
    if (RoboGPUVirtioSetup(chosen, gpu) == ROBO_TRUE) {
      RoboGPUSetHardwareScanout(RoboGPUVirtioScanout);
    } else {
      RoboSerialWriteLine("ROBO_GPU: virtio present, firmware scanout remains the display");
    }
  }
}
