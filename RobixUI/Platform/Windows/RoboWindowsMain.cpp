#include "RoboWindows.hpp"

#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR command, int) {
  RoboWindowsHost host;
  return host.Run(command);
}
