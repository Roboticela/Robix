#pragma once

#include "../RoboSyscall/RoboSyscall.hpp"

/* Pages from the kernel frame allocator. Firmware already maps this RAM. */
inline void* RoboAllocIdentity(RoboU64 bytes) {
  if (bytes == 0) {
    return ROBO_NULL;
  }
  const RoboU64 pages = (bytes + 4095ull) / 4096ull;
  const RoboU64 address = RoboInvokeSyscall(ROBO_SYS_ALLOC_PHYSICAL, pages);
  if (address == 0) {
    return ROBO_NULL;
  }
  return reinterpret_cast<void*>(address);
}
