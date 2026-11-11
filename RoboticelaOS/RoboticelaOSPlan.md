# RoboticelaOS

The whole plan for a desktop operating system in the class of Windows and macOS: a screen you can use, accounts, files, apps, settings, devices, network, power, updates, and recovery. The image is Robix. Kernel, drivers, services, and programs are `.rx`. Windows are `.rxu`. Spatial scenes are `.rx3d`. The machine contract stays the one in `RefCode` and `RobixUI/RoboShared`.

Stages A through E put a desktop on the screen before the rest of the machine exists. Later stages keep that same screen and fill in the system underneath it. A stage is open until its Robix feature test, every file test, and its screen or serial proof have been run and recorded.

The first proof target is QEMU `pc` on `x86_64-unknown-none`. Other CPUs and a desk machine are Stage AF.

Use the names as GPU not Gpu, ARM64 not Arm64 and so on

---

## 1. What you see, and when

QEMU is started with `-kernel` from Stage A, so the framebuffer is the proof. The bootloader replaces that shortcut in Stage F. The pixels stay the same.

| Stage | On the screen |
| :--- | :--- |
| A | Dark desktop, blue bar, cooked logo |
| B | The real `.rxu` shell: bar, icons, one window |
| C | Cursor moves. A button click changes the window |
| D | The `.rx3d` wallpaper is behind the windows |
| E | Drag, focus, menus, clock, cursor shapes, snapping |
| N | Login and lock screens in front of that desktop |
| O | Dock, launchpad, notifications, control center, widgets, spaces |
| Q | Files windows with real folders |
| R | Settings |
| JB | Device Manager, each device on its built-in driver |
| JC | The same window after an Intel driver is installed |
| AC | Editor, calculator, image viewer, browser shell |

---

## 2. Rules for every stage

**The first task is a Robix feature.** Each stage opens with a statement of the language or toolchain feature that makes that stage easier for every later programmer. That feature is implemented and tested in the Robix compiler before any OS file for the stage is written. The OS code then calls the feature. A stage that starts by hand-rolling the hard part has skipped its first task.

**Every `.rx` file has a `.test.rx` beside it.** The test file uses `TestFile` with `Covers` set to that source file, and `Assert.Equal`, `Assert.True`, `Assert.False`, or `AssertNear`. `robix test` runs the interpreter and the native compiler and compares them. A test that touches a port, a control register, or an interrupt is listed in `RoboticelaOS/Tests/NATIVE_ONLY.md`. That file still has a host test for the pure part (the bit math, the parser, the table lookup). The image boot is the second proof, recorded in `Documentation/Status.md`.

**A stage closes only when all three exist.** The Robix feature's own test passed. Every new `.test.rx` passed on both engines. The screen pixel or the serial line for that stage was captured from QEMU.

Hosted calls (`Print`, `File`, `Thread`, `Network`, `Args`) stay out of the image. Machine calls (`CPU`, `Port`, `Memory.Pages`, `Memory.Address`, `Memory.At`, `Timer`, `System.Call`, `Volatile`, `ASM`) stay out of Windows programs. Pure functions are the ones the tests call.

Kernel tables are structs. There is no garbage collector. `Unsafe` owns frames the kernel allocates. Until global constants are native-complete (Stage F's feature), numeric contracts are functions that return the literal.

---

## 3. Sources that stay as they are

| Source | What it already decides |
| :--- | :--- |
| `Examples/Machine` | Pages, GDT, memory map, VGA cell `0xB8000`, CMOS, two stacks, timer, ATA sector 0. `Output: Kernel`, `Target: x86_64-unknown-none`, `Entry: Start`, `Profile: embedded`. |
| `Examples/Machine/Bios.s` | 16-bit reset. Robix starts after long mode. |
| `RefCode/RoboKernelMain.cpp` | Bring-up order, splash ceilings, runtime entry, idle poll. |
| `RefCode/RoboCPU.cpp` | MADT, 16 cores, trampoline at `0x8000`, mailboxes, INIT-SIPI. |
| `RefCode/RoboPCI.hpp`, `RoboInput.hpp`, `RoboHID.hpp`, `RoboVirtioMouse.hpp`, `RoboVMMouse.hpp`, `RoboI2CTouchpad.hpp`, `RoboXHCI.hpp`, `RoboInputRules.hpp` | Bus scan and the input hosts. |
| `RobixUI/RoboShared/RoboHandoff.hpp` | Magic `0x444E4852` (`RHND`), version 4. |
| `RobixUI/RoboGPU` | Cached back buffer, damage list, front scanout, cursor blend. |
| `RobixUI/RoboShared` | Cursor pack `RCUR`, icon catalog `RICP`, cooked icons `ROIC`, font pack. |

`RoboKernelMain.cpp` calls memory, IDT, processes, drivers, the filesystem, syscalls, the loader, serial, and splash. Those modules are written in `.rx` in the stages below. The AP trampoline in RefCode passes the first argument in `RCX` (Windows). Robix freestanding uses System V, so the Robix trampoline loads `RDI` and keeps `RSP` 8 bytes off a 16-byte boundary. Mailbox addresses stay the ones in `RoboCPU.cpp`.

### 3.1 RoboHandoff

`Shared/RoboHandoff.rx` matches `RoboHandoff.hpp` field for field. Magic `0x444E4852`, version `4`. Framebuffer pitch is `pixelsPerScanLine * bytesPerPixel`, 4 bytes per pixel, XRGB8888. Blob size 0 means that blob is absent. Input flags: bit 0 keyboard, bit 1 mouse, bit 2 touchpad, bit 3 touch. Cursor theme 0 is dark. Memory types: 1 free, 2 boot, 3 runtime, 4 ACPI, at most 48 ranges. `bootName` is `RoboticelaOS`. A bad magic prints `ROBO_KERNEL: bad handoff` and halts.

Splash ceilings, milli-percent: memory 97400, IDT 97800, CPUs 98200, processes 98600, input 98800, drivers 99000, filesystem 99200, syscalls 99400, desktop app 99600, runtime presents 100000.

### 3.2 Limits copied from RefCode

`ROBO_CPU_MAX` 16. PCI devices 64. Input queue 128. xHCI hosts 4, ports 32, HID devices 16. I2C HID devices 4, controllers 8. Cursor kinds 11, pack `RCUR` version 2. Icon pack `ROIC` version 1. CPU mailboxes: trampoline `0x8000`, CR3 `0x8120`, stack `0x8128`, entry `0x8130`, APIC id `0x8138`, ready `0x8160`. INIT IPI `0x4500`, SIPI `0x4608`. Syscall echo returns the argument plus `0x1337`. The first loaded path is `System/Desktop/Desktop.App`.

Serial is port `0x3F8`. Early gates may print one proof byte (`P` pages, `G` GDT, `M` memory, `V` cell, `K` kernel, `T` tick, `D` disk, `X` fail). The line logger replaces that byte in the same stage.

---

## 4. Tree

Every `.rx` file in this tree has a sibling `.test.rx`. The test files are not listed twice.

```text
RoboticelaOS/
  RoboticelaOSPlan.md
  RoboticelaOS.rxb
  Tests/NATIVE_ONLY.md
  Shared/          RoboHandoff.rx  RoboTypes.rx  RoboPacks.rx
  Boot/            RoboBootROM.s  RoboBootloader.rx  RoboBootloader.rxb
  Kernel/          RoboKernel.rx  RoboKernel.rxb
    Memory/        RoboMemoryManager.rx
    Interrupt/     RoboIDT.rx
    CPU/           RoboCPU.rx
    PCI/           RoboPCI.rx
    Input/         RoboInput.rx  RoboHID.rx  RoboVirtioMouse.rx  RoboVMMouse.rx  RoboI2CTouchpad.rx  RoboXHCI.rx
    Drivers/       RoboDriver.rx  RoboDriverManager.rx
      Basic/       one built-in driver per device class
      Intel/       the first vendor catalog and its drivers
      Vendors/     later catalogs, one folder per vendor milestone
    FileSystem/    RoboFS.rx
    Process/       RoboProcessManager.rx
    Syscall/       RoboSyscall.rx
    Loader/        RoboLoader.rx
    Accounts/      RoboAuth.rx  RoboACL.rx
    Devices/       RoboSerial.rx  RoboScreen.rx  RoboSplash.rx  RoboTimer.rx  RoboDisk.rx
                   RoboAudio.rx  RoboNet.rx  RoboBluetooth.rx  RoboPower.rx  RoboDisplay.rx  RoboPrint.rx
    Security/      RoboBootChain.rx  RoboDiskSeal.rx
  Runtime/
    RoboRuntime.rx  RoboRuntime.rxb
    GPU/           RoboGPU.rx  RoboCursor.rx  RoboText.rx
    UI/            RoboUI.rx  RoboHit.rx  RoboShell.rx  RoboWindow.rx
    Scene/         RoboSceneRaster.rx
    Services/      RoboSearch.rx  RoboNotify.rx  RoboClip.rx  RoboSettings.rx
                   RoboUpdate.rx  RoboPackage.rx  RoboLog.rx  RoboLaunch.rx
  Desktop/         RoboDesktop.rxu and the other shell documents
  Scenes/          RoboSplash.rx3d  RoboWallpaper.rx3d  RoboStudio.rx3d
  Apps/            one folder per program, each with a .rx and a .rxu or .rx3d
  Image/           RoboVolume.rx
```

`RoboticelaOS.rxb` is the Bedrock `OS` image: ROM, bootloader, kernel, runtime, cooked packs, volume. Until that kind links as one file, each part uses the `Firmware` or `Kernel` `.rxb` the toolchain already links, and one script concatenates them.

QEMU for every screen stage:

```text
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboKernel.elf -serial file:Build/RoboticelaOS/stage.serial -vga std
```

---

## 5. Stages A–E — the desktop on screen

### Stage A — First picture

Future programmers get a desktop on screen by filling a `Frame`, and the tests read those pixels on Windows. They do not boot QEMU to find out that a rectangle was one pixel off.

**Task A1, Robix feature.** Add `Frame.Create(width, height)`, `Frame.Fill`, `Frame.Pixel`, and `AssertPixel(frame, x, y, argb)` to the host test runtime. `robix ui frame` writes a PNG from the same buffer. The compiler test fills an 84×44 rectangle with `0E7AFF` and `AssertPixel` checks the origin and a pixel just outside the rectangle.

**Task A2.** `Runtime/GPU/RoboGPU.rx` draws into a `Frame` on the host and into the handoff back buffer on the image. Clear the desktop to `#080A10`, fill the bar, blit the cooked logo.

**Task A3.** `Kernel/RoboKernel.rx` calls that draw and presents. A stub handoff supplies a linear framebuffer so this stage does not wait for the bootloader.

| File | Test |
| :--- | :--- |
| `Runtime/GPU/RoboGPU.rx` | `RoboGPU.test.rx` asserts the bar origin pixel and the desktop color |
| `Kernel/RoboKernel.rx` | `RoboKernel.test.rx` asserts the present path reports the frame size |

**Exit.** Both tests pass on the interpreter and the native compiler. QEMU shows the dark desktop, the blue bar, and the logo. Serial contains `ROBO_GPU: present`.

### Stage B — The shell document

Future programmers design the desktop in `.rxu` and the image paints the cooked boxes. They do not parse XML in the kernel, and they do not keep a second layout in code.

**Task B1, Robix feature.** Add `robix ui cook File.rxu -o File.rxul`. The blob is a list of records: kind, x, y, width, height, text. `UI.LoadCooked` reads it. The compiler test cooks a button and checks an 84×44 box whose role is `Button`, matching the box the Windows host already records.

**Task B2.** `Desktop/RoboDesktop.rxu` is the bar, the icon row, and one window titled `RoboticelaOS`. `Runtime/UI/RoboUI.rx` paints the cooked records with `RoboGPU`.

**Task B3.** `robix ui validate` and `robix ui window` open the same document on Windows.

| File | Test |
| :--- | :--- |
| `Runtime/UI/RoboUI.rx` | `RoboUI.test.rx` loads a fixture `.rxul` and asserts each box |

**Exit.** The image shows the bar, the icons, and the window. Serial contains `ROBO_UI: paint`. The Windows preview shows the same document.

### Stage C — Cursor and clicks

Future programmers replay a script of pointer and key events in a test. A click bug shows up as a failed `Assert.Equal` on Windows, and the same script runs in the image.

**Task C1, Robix feature.** Add `Input.Play(script)` which pushes `RoboInputEvent` values into the queue from `RefCode/RoboInput.hpp`. The compiler test plays a move and a click and asserts the queue length, the coordinates, and the button bit.

**Task C2.** `Kernel/Input/RoboInput.rx` is that queue: init, push, pop, relative, absolute, key. On the image, Stage J will fill the same queue from hardware. This stage fills it from `Input.Play` and from a small PS/2 reader if the device is already answering.

**Task C3.** `Runtime/UI/RoboHit.rx` maps a point to a cooked box. A click on the window button increments a counter the test can see.

| File | Test |
| :--- | :--- |
| `Kernel/Input/RoboInput.rx` | `RoboInput.test.rx` plays the script and asserts order |
| `Runtime/UI/RoboHit.rx` | `RoboHit.test.rx` asserts the button id under the click point |

**Exit.** The cursor moves and the button changes the window. Serial contains `ROBO_UI: click`. Both tests pass on both engines.

### Stage D — Wallpaper scene

Future programmers render one `.rx3d` frame into a `Frame` and assert pixels. The kernel raster and the Windows preview share that call.

**Task D1, Robix feature.** Add `Scene.Render(scene, frame, camera)` for a perspective camera, one directional light, and flat-colored meshes. Units stay meters, Y stays up, rotation stays radians. The compiler test renders `Scenes/RoboSplash.rx3d` with a fixed camera and `AssertPixel` checks a body pixel and the background `#080A10`.

**Task D2.** `Scenes/RoboWallpaper.rx3d` is the desktop background. `Runtime/Scene/RoboSceneRaster.rx` draws it into the back buffer before `RoboUI` paints windows. `Desktop/RoboDesktop.rxu` references it with `View3D`. The scene file does not contain RobixUI elements.

| File | Test |
| :--- | :--- |
| `Runtime/Scene/RoboSceneRaster.rx` | `RoboSceneRaster.test.rx` asserts the body pixel and the background |

**Exit.** The wallpaper is visible behind the window. Serial contains `ROBO_SCENE: frame`.

### Stage E — A shell you can use

Future programmers move, focus, and snap windows by calling pure functions on layout records. The image only applies the record the test already checked.

**Task E1, Robix feature.** Add `UI.Place`, `UI.Hit`, `UI.Drag`, and `UI.Snap` that return a new layout and a cursor kind. Kinds match the `RCUR` list: default, pointer, grab, text, resize. The compiler test drags a window by a known delta and asserts the new origin, then snaps it to the left half and asserts the width.

**Task E2.** `Runtime/UI/RoboWindow.rx` and `Runtime/UI/RoboShell.rx` apply those results. `Runtime/GPU/RoboCursor.rx` blends the kind. `Runtime/GPU/RoboText.rx` draws the clock from a number the test supplies. Menus open from a bar hit.

| File | Test |
| :--- | :--- |
| `Runtime/UI/RoboWindow.rx` | `RoboWindow.test.rx` covers drag, focus, and snap |
| `Runtime/UI/RoboShell.rx` | `RoboShell.test.rx` asserts the menu opens on the bar hit |
| `Runtime/GPU/RoboCursor.rx` | `RoboCursor.test.rx` asserts kind 4 on a text box and kind 6 on an edge |
| `Runtime/GPU/RoboText.rx` | `RoboText.test.rx` asserts the clock glyphs for a fixed time |

**Exit.** Drag, snap, menus, the clock, and cursor shapes work in QEMU. Serial contains `ROBO_SHELL: bar`.

---

## 6. Stages F–J — the same picture, from a real machine

### Stage F — Bootloader and handoff

Future programmers declare the handoff as a packed struct and the compiler tells them when a field moves. Magic numbers are real constants, checked once, and every later file reuses them.

**Task F1, Robix feature.** Make `@Packed`, `@Align`, `Memory.SizeOf`, and `Memory.OffsetOf` native-complete, and make global `const` values native-complete. The compiler test builds a packed stand-in of `RoboHandoff` and asserts `magic` at offset 0 and `version` at offset 4. A mismatched size fails the build.

**Task F2.** `Shared/RoboTypes.rx`, `Shared/RoboHandoff.rx`, and `Shared/RoboPacks.rx` use those constants. Pack readers accept `RCUR`, `RICP`, and `ROIC` and reject a wrong magic.

**Task F3.** `Boot/RoboBootROM.s` is the 16-bit reset from `Examples/Machine/Bios.s`. `Boot/RoboBootloader.rx` builds the memory map, fills the handoff, calibrates `tscFrequency`, and calls `RoboKernelStart` with the pointer in `RDI`.

**Task F4.** `Kernel/Devices/RoboSerial.rx` and `Kernel/Devices/RoboSplash.rx` log lines and draw the progress bar up to the ceilings in section 3.1.

| File | Test |
| :--- | :--- |
| `Shared/RoboTypes.rx` | `RoboTypes.test.rx` asserts widths of `RoboU8` through `RoboU64` |
| `Shared/RoboHandoff.rx` | `RoboHandoff.test.rx` asserts magic, version, and offsets |
| `Shared/RoboPacks.rx` | `RoboPacks.test.rx` accepts `RCUR` and rejects a bad magic |
| `Boot/RoboBootloader.rx` | `RoboBootloader.test.rx` fills a map fixture and asserts type-1 and type-2 ranges |
| `Kernel/Devices/RoboSerial.rx` | `RoboSerial.test.rx` asserts the line bytes for `ROBO_BOOT: handoff` |
| `Kernel/Devices/RoboSplash.rx` | `RoboSplash.test.rx` asserts a ceiling never moves backward |

**Exit.** Serial contains `ROBO_BOOT: handoff` and `ROBO_KERNEL: start`, and the Stage E desktop is what the screen shows. A zeroed magic prints `ROBO_KERNEL: bad handoff` and halts. The ROM test image still prints byte `I` (73).

### Stage G — Memory

Future programmers ask the language for a frame and a page-table entry. The shift-and-mask formula lives in one function with one test, the same numbers the C file already prints.

**Task G1, Robix feature.** Add `Memory.Frame()` and `Page.Entry(phys, flags)` as intrinsics whose host versions allocate from a test pool and compute `(phys & ~0xFFF) | flags`. The compiler test expects the five values from `Examples/Machine/Bits.rx`: `8195`, `4096`, `240`, `127`, `-2`.

**Task G2.** `Kernel/Memory/RoboMemoryManager.rx` identity-maps the low 4 GiB, keeps boot, runtime, and ACPI ranges reserved, and hands out 4 KiB frames from type 1. `CPU.WriteCR3` then `CPU.ReadCR3` must return that root on the image.

| File | Test |
| :--- | :--- |
| `Kernel/Memory/RoboMemoryManager.rx` | `RoboMemoryManager.test.rx` covers `Page.Entry`, exhaustion returning 0, and reserved ranges staying reserved |

**Exit.** Serial contains `ROBO_MEMORY: pages` and `ROBO_MEMORY: map`. Splash ceiling 97400. The desktop still presents.

### Stage H — Interrupts and time

Future programmers build a gate with one call and arm a timer with one call. The descriptor bits are a host test. Loading the table stays in the image.

**Task H1, Robix feature.** Add `CPU.Gate(handler)` returning the low and high descriptor words from `Examples/Machine/Descriptors.rx`, and `Clock.Seconds(cmos)` as a pure decode of a CMOS byte. The compiler test asserts the gate bits for a fixed handler address and a seconds value of 42.

**Task H2.** `Kernel/Interrupt/RoboIDT.rx` loads the GDT and the IDT with `CPU.LGDT` and `CPU.LIDT`. Vectors 0 through 31 print `ROBO_IDT: trap` and the vector, then halt.

**Task H3.** `Kernel/Devices/RoboTimer.rx` uses `Timer.Arm`. The clock on the bar reads the CMOS seconds through the decode function. `Kernel/Devices/RoboScreen.rx` still read-backs a cell when the framebuffer is absent.

| File | Test |
| :--- | :--- |
| `Kernel/Interrupt/RoboIDT.rx` | `RoboIDT.test.rx` asserts `CPU.Gate` for vectors 3 and 14 |
| `Kernel/Devices/RoboTimer.rx` | `RoboTimer.test.rx` asserts the tick handler id the pure side records |
| `Kernel/Devices/RoboScreen.rx` | `RoboScreen.test.rx` asserts the cell byte `V` (86) in a fake buffer |

**Exit.** Serial contains `ROBO_IDT: gates`, `ROBO_CLOCK: seconds`, and `ROBO_TIMER: tick`. A test image's `int3` prints `ROBO_IDT: trap 3`. Splash ceiling 97800. The bar clock shows the seconds value.

### Stage I — More than one core

Future programmers write a 32-bit mailbox with a 32-bit volatile load. The test uses an array. The image uses the same addresses RefCode already chose, and a core that never answers does not stop the desktop.

**Task I1, Robix feature.** Add `Volatile.Load32` and `Volatile.Store32` beside the existing 64-bit pair. The compiler test stores 1 through a pointer and reads it back. `Atomic.FetchAdd` remains the counter for cores still in a job.

**Task I2.** `Kernel/CPU/RoboCPU.rx` walks RSDP and MADT, writes the trampoline at `0x8000`, and sends INIT-SIPI. Page tables above 4 GiB keep the boot processor alone and log that line. `RoboCPUParallel` splits `RoboGPU` rows when more than one core is online.

| File | Test |
| :--- | :--- |
| `Kernel/CPU/RoboCPU.rx` | `RoboCPU.test.rx` asserts the checksum of `RSD PTR `, the mailbox addresses, and a skipped core leaving `online` at 1 |

**Exit.** On `-smp 4`, serial contains `ROBO_CPU: cores online`. On `-smp 1`, the desktop still appears. Splash ceiling 98200.

### Stage J — Keyboards, pointers, and USB

Future programmers decode PCI and HID on Windows with fixture bytes. The image uses `Port.Read32` for config space, which the compiler tests as a freestanding call so nobody reaches for a raw `outl` again.

**Task J1, Robix feature.** Add `Port.Read32` and `Port.Write32`. The interpreter stops with the freestanding diagnostic, and that stop is the compiler test, listed as native-only for the port itself. Add `PCI.Decode(words)` and `HID.Parse(bytes)` as pure functions. Their compiler tests use a fixed config word and a 3-byte boot mouse report.

**Task J2.** Port the RefCode hosts into `RoboPCI.rx`, `RoboHID.rx`, `RoboVirtioMouse.rx`, `RoboVMMouse.rx`, `RoboI2CTouchpad.rx`, and `RoboXHCI.rx`. Absent hardware is skipped. `RoboDriverManager.rx` records each device and calls `RoboInputPoll` once. Stage JA chooses the driver that runs it.

**Task J3.** The Stage C queue is the one these drivers push into. `Input.Play` remains the test path.

| File | Test |
| :--- | :--- |
| `Kernel/PCI/RoboPCI.rx` | `RoboPCI.test.rx` decodes vendor, device, and class |
| `Kernel/Input/RoboHID.rx` | `RoboHID.test.rx` parses mouse, wheel, and a touch contact |
| `Kernel/Input/RoboVirtioMouse.rx` | `RoboVirtioMouse.test.rx` applies one absolute event |
| `Kernel/Input/RoboVMMouse.rx` | `RoboVMMouse.test.rx` applies one absolute event |
| `Kernel/Input/RoboI2CTouchpad.rx` | `RoboI2CTouchpad.test.rx` asserts ELAN skips the second wake |
| `Kernel/Input/RoboXHCI.rx` | `RoboXHCI.test.rx` classifies HID `0x03` and hub `0x09` |
| `Kernel/Drivers/RoboDriverManager.rx` | `RoboDriverManager.test.rx` asserts a missing device does not clear the queue |

**Exit.** Serial contains `ROBO_PCI: devices` and `ROBO_INPUT: ready`. The cursor still moves, now from the real device. Splash ceilings 98800 and 99000.

---

## 7. Stages JA–JI — built-in drivers, then vendor drivers

Every device the scan finds gets a built-in class driver at boot. That driver uses the standard path for its class, so the device runs as fast as that standard allows. A vendor driver is a package Device Manager downloads and installs. Installing it rebinds the device. Removing it returns the device to the built-in driver. Intel is the first vendor milestone. Each later vendor is the same milestone with its own catalog.

A built-in driver uses the class DMA or queue path when the class has one, 64-bit BARs when the device advertises them, and message signaled interrupts when the device advertises those. A polled PIO loop is the path only when the device has no queue. A device whose protocol is vendor-specific stays listed, and Device Manager shows that the built-in driver cannot run it until a vendor package is installed. The desktop, the disk, the keyboard, and the USB ports keep working through their class standards either way.

### Stage JA — A built-in driver for every class

Future programmers add a device class by adding one built-in driver and one row in the class table. The bind call prefers an installed vendor package, and the built-in driver when no package matches.

**Task JA1, Robix feature.** Add `Driver.Bind(device, builtIn, installed) -> Result`. An installed package whose vendor and device id match wins. Otherwise the built-in driver for that class wins. Two built-in drivers for one class fail the test. No match returns `Error`. The compiler test binds an Intel Ethernet id to a fake installed package, then binds the same id to `RoboBasicNet` after the package is removed.

**Task JA2.** `Kernel/Drivers/RoboDriver.rx` is that table. `RoboDriverManager.rx` binds every device from Stage J before the first poll returns. These built-in files ship in the image:

| Class | File | Efficient path |
| :--- | :--- | :--- |
| Display | `Kernel/Drivers/Basic/RoboBasicDisplay.rx` | Linear framebuffer, damage present, hardware scanout when the BAR is a framebuffer |
| Audio | `Kernel/Drivers/Basic/RoboBasicAudio.rx` | HDA CORB/RIRB and a buffer list |
| Ethernet | `Kernel/Drivers/Basic/RoboBasicNet.rx` | RX and TX descriptor rings |
| USB host | `Kernel/Drivers/Basic/RoboBasicUSB.rx` | xHCI rings. HID and hubs stay on this driver |
| USB storage | `Kernel/Drivers/Basic/RoboBasicUSBStore.rx` | Bulk queues |
| AHCI, NVMe, ATA, SD | `Kernel/Drivers/Basic/RoboBasicStorage.rx` | NCQ, NVMe queues, ATA DMA, SD ADMA2. ATA PIO only when DMA is absent |
| Camera | `Kernel/Drivers/Basic/RoboBasicCamera.rx` | UVC isochronous |
| Bluetooth | `Kernel/Drivers/Basic/RoboBasicBluetooth.rx` | HCI over USB |
| Power | `Kernel/Drivers/Basic/RoboBasicPower.rx` | ACPI battery and sleep |
| Printer | `Kernel/Drivers/Basic/RoboBasicPrint.rx` | USB bulk |
| Serial | `Kernel/Drivers/Basic/RoboBasicSerial.rx` | 16550 FIFO |
| SMBus | `Kernel/Drivers/Basic/RoboBasicSMBus.rx` | Block transfer |
| VirtIO | `Kernel/Drivers/Basic/RoboBasicVirtio.rx` | Virtqueues for QEMU net, block, and input |

Wi-Fi, vendor GPUs, and vendor audio DSPs are not given a fake class protocol. They appear in Device Manager as waiting for a package.

| File | Test |
| :--- | :--- |
| `Kernel/Drivers/RoboDriver.rx` | `RoboDriver.test.rx` covers vendor win, built-in fallback, a duplicate class, and no match |
| Each `RoboBasic*.rx` above | A sibling `.test.rx` asserts the queue or DMA path is selected for a fixture that advertises it, and PIO only for a fixture that does not |

**Exit.** Serial contains `ROBO_DRIVER: basic` and one line per bound class. The desktop, keyboard, and disk still work. Splash ceiling stays 99000.

### Stage JB — Device Manager installs the real driver

Future programmers install, replace, and remove a driver package with one call. The window is a list of the bind results from Stage JA.

**Task JB1, Robix feature.** Add `Driver.Package(bytes) -> Result` and `Driver.Newer(current, offered)`. The compiler test accepts a verified header, rejects a bad HMAC, replaces an older version, and keeps the previous package for rollback.

**Task JB2.** `Apps/Devices/RoboDevicesApp.rx` and `Desktop/RoboDevices.rxu` list every device: name, class, vendor, device id, the bound driver, and whether that driver is built in or installed. Install, update, and remove call `Driver.Package`. Remove rebinds through `Driver.Bind` so the built-in driver is back.

**Task JB3.** The first catalog lives on the volume at `System/Drivers/Catalog`. Install from that folder is enough for this stage. After Stage T answers the network, the same window downloads a package from the catalog address in Settings and then calls the same install path.

| File | Test |
| :--- | :--- |
| `Apps/Devices/RoboDevicesApp.rx` | `RoboDevicesApp.test.rx` covers the list, install, a bad HMAC, a newer version, rollback, and the return to the built-in driver |

**Exit.** Device Manager is on screen. Installing a package from the volume prints `ROBO_DRIVER: install`, and removing it prints `ROBO_DRIVER: basic`. A bad package prints `ROBO_DRIVER: reject` and leaves the built-in driver bound.

### Stage JC — Intel, the first vendor milestone

Future programmers add an Intel device by adding one id to the Intel catalog. The match is a test. The package installs through Device Manager, and the built-in driver remains the boot path when the package is absent.

**Task JC1, Robix feature.** Add `Vendor.Match(catalog, vendor, device, class)`. The compiler test uses vendor `0x8086` and the LPSS id `0x9D60` from `RefCode/RoboInputRules.hpp` and expects an Intel LPSS match. Vendor `0x1022` returns no Intel match. That id stays free for the AMD stage.

**Task JC2.** `Kernel/Drivers/Intel/RoboIntelCatalog.rx` is the Intel id table. Each family is a package Device Manager can install. The built-in driver for that class stays bound until the package is installed.

| Package | File | What the Intel driver adds beyond the built-in one |
| :--- | :--- | :--- |
| Graphics | `Kernel/Drivers/Intel/RoboIntelGraphics.rx` | Intel display ids, extra modes, planes, and power states past the linear framebuffer |
| Audio | `Kernel/Drivers/Intel/RoboIntelAudio.rx` | SST, SOF, and digital microphones when HDA is not the whole device |
| Ethernet | `Kernel/Drivers/Intel/RoboIntelEthernet.rx` | Device ids for the e1000e, igc, igb, ixgbe, and i40e families, plus checksum offload and larger rings |
| Wi-Fi | `Kernel/Drivers/Intel/RoboIntelWiFi.rx` | Intel Wi-Fi families, including AX200 and AX201. This is the driver that can run the device |
| Bluetooth | `Kernel/Drivers/Intel/RoboIntelBluetooth.rx` | Intel USB and CNVi Bluetooth |
| LPSS | `Kernel/Drivers/Intel/RoboIntelLPSS.rx` | The `0x8086` I2C ids already listed in `RoboInputRules.hpp`, with the clock and timing helpers in that file |
| USB and Thunderbolt | `Kernel/Drivers/Intel/RoboIntelUSB.rx` | Intel xHCI quirks, USB4, and Thunderbolt |
| Storage | `Kernel/Drivers/Intel/RoboIntelStorage.rx` | VMD and RST on top of the built-in AHCI and NVMe queues |
| Camera | `Kernel/Drivers/Intel/RoboIntelCamera.rx` | Intel IPU, when the camera is not UVC |
| Chipset | `Kernel/Drivers/Intel/RoboIntelChipset.rx` | PCH GPIO, the thermal sensor, and the Intel SMBus the built-in SMBus driver does not cover |

**Task JC3.** Each package is signed with the Stage JB HMAC, stored under `System/Drivers/Intel`, and installed from Device Manager. A QEMU device with vendor `0x8086` and id `0x9D60` binds `RoboBasic` at boot, then `RoboIntelLPSS` after install, then `RoboBasic` again after remove.

| File | Test |
| :--- | :--- |
| `Kernel/Drivers/Intel/RoboIntelCatalog.rx` | `RoboIntelCatalog.test.rx` covers `0x8086` / `0x9D60`, an unknown Intel id, and vendor `0x1022` |
| Each `RoboIntel*.rx` package file | A sibling `.test.rx` asserts its own id matches, a foreign vendor does not, and the efficient path (rings, offload, or the LPSS divider rule) is the one the fixture selects |

**Exit.** Device Manager shows the Intel package on the LPSS device. Serial contains `ROBO_DRIVER: intel` after install and `ROBO_DRIVER: basic` after remove. This recorded boot is the Intel milestone. Later vendors do not start before it.

### Stage JD — AMD

Future programmers add AMD the way Intel was added: a catalog, then packages, installed only through Device Manager.

**Task JD1, Robix feature.** The feature is `Vendor.Match` from Stage JC. This stage's compiler test adds an AMD catalog fixture: vendor `0x1022` matches a chipset row, vendor `0x1002` matches a graphics row, and `0x8086` does not match the AMD catalog.

**Task JD2.** `Kernel/Drivers/Vendors/AMD/RoboAMDCatalog.rx` and packages for chipset (`0x1022`, including the I2C ids `0x1579`, `0x15E6`, `0x1634`, and `0x164C` already in `RoboInputRules.hpp`), graphics (`0x1002`), and Ethernet. Each package rebinds through Device Manager and removes back to the built-in driver.

| File | Test |
| :--- | :--- |
| `Kernel/Drivers/Vendors/AMD/RoboAMDCatalog.rx` | `RoboAMDCatalog.test.rx` covers both vendor ids and an Intel id that must not match |

**Exit.** Serial contains `ROBO_DRIVER: amd` after install. The Intel catalog tests still pass.

### Stage JE — NVIDIA

Future programmers match NVIDIA by vendor `0x10DE` and install the graphics package from Device Manager. The built-in display driver keeps the linear framebuffer until that package is installed.

**Task JE1, Robix feature.** A compiler test catalog matches `0x10DE` to `RoboNVIDIAGraphics` and rejects `0x8086`.

**Task JE2.** `Kernel/Drivers/Vendors/NVIDIA/RoboNVIDIACatalog.rx` and `RoboNVIDIAGraphics.rx`. Modes and the scanout hook come from the package. Remove returns the display to `RoboBasicDisplay`.

| File | Test |
| :--- | :--- |
| `Kernel/Drivers/Vendors/NVIDIA/RoboNVIDIACatalog.rx` | `RoboNVIDIACatalog.test.rx` covers `0x10DE` and a foreign vendor |

**Exit.** Serial contains `ROBO_DRIVER: nvidia` after install and `ROBO_DRIVER: basic` after remove.

### Stage JF — Realtek

Future programmers match Realtek audio and LAN by vendor `0x10EC`. The HDA and Ethernet built-in drivers run those devices until the Realtek package is installed.

**Task JF1, Robix feature.** A compiler test catalog matches `0x10EC` audio and `0x10EC` Ethernet to two package names, and rejects `0x8086`.

**Task JF2.** `Kernel/Drivers/Vendors/Realtek/RoboRealtekCatalog.rx`, `RoboRealtekAudio.rx`, and `RoboRealtekNet.rx`.

| File | Test |
| :--- | :--- |
| `Kernel/Drivers/Vendors/Realtek/RoboRealtekCatalog.rx` | `RoboRealtekCatalog.test.rx` covers both class rows and a foreign vendor |

**Exit.** Serial contains `ROBO_DRIVER: realtek` after install.

### Stage JG — Broadcom

Future programmers match Broadcom Wi-Fi and Ethernet by vendor `0x14E4`. Wi-Fi stays on the Device Manager waiting row until this package is installed, because Stage JA does not invent a Wi-Fi class protocol.

**Task JG1, Robix feature.** A compiler test catalog matches `0x14E4` to the Wi-Fi package and the Ethernet package.

**Task JG2.** `Kernel/Drivers/Vendors/Broadcom/RoboBroadcomCatalog.rx`, `RoboBroadcomWiFi.rx`, and `RoboBroadcomNet.rx`.

| File | Test |
| :--- | :--- |
| `Kernel/Drivers/Vendors/Broadcom/RoboBroadcomCatalog.rx` | `RoboBroadcomCatalog.test.rx` covers both rows |

**Exit.** Serial contains `ROBO_DRIVER: broadcom` after install.

### Stage JH — Qualcomm

Future programmers match Qualcomm Wi-Fi by vendor `0x17CB` and install that package from Device Manager.

**Task JH1, Robix feature.** A compiler test catalog matches `0x17CB` and rejects `0x8086`.

**Task JH2.** `Kernel/Drivers/Vendors/Qualcomm/RoboQualcommCatalog.rx` and `RoboQualcommWiFi.rx`.

| File | Test |
| :--- | :--- |
| `Kernel/Drivers/Vendors/Qualcomm/RoboQualcommCatalog.rx` | `RoboQualcommCatalog.test.rx` covers `0x17CB` and a foreign vendor |

**Exit.** Serial contains `ROBO_DRIVER: qualcomm` after install.

### Stage JI — The next vendor

Future programmers add a vendor by adding a catalog file and packages. The kernel bind path stays the one from Stage JA.

**Task JI1, Robix feature.** Add `Vendor.Catalog(name, vendorId)` which registers a catalog the matcher already tests. The compiler test registers a fictional vendor id `0x0001`, matches it, and a missing catalog returns `Error`.

**Task JI2.** A new folder under `Kernel/Drivers/Vendors/<Name>/` with a catalog and one package per family, installed and removed through Device Manager. The stage record names the vendor, the id, and the families. Intel through Qualcomm stay in their own stages.

| File | Test |
| :--- | :--- |
| The new `Robo<Name>Catalog.rx` | `Robo<Name>Catalog.test.rx` covers that vendor id, a foreign id, install, and remove back to the built-in driver |

**Exit.** Serial contains `ROBO_DRIVER:` plus the vendor name after install, and `ROBO_DRIVER: basic` after remove.

---

## 8. Stages K–N — programs, files, and a login window

### Stage K — Processes that contain a fault

Future programmers test a page-table split and a fault decision without crashing the host. A bad app in the image takes down that app. The bar stays.

**Task K1, Robix feature.** Add `Page.Split(root, userStart)` which marks the kernel half supervisor and the user half user, and `Fault.Handle(fromUser) -> Int` which returns continue or halt. The compiler test asserts the flag bits and asserts a user fault returns continue while a kernel fault returns halt.

**Task K2.** `Kernel/Process/RoboProcessManager.rx` creates `DesktopApp` with its own root and stack. A user fault prints `ROBO_PROCESS: fault` and returns to the shell loop.

| File | Test |
| :--- | :--- |
| `Kernel/Process/RoboProcessManager.rx` | `RoboProcessManager.test.rx` covers split flags, the fault decision, and a second process that stays alive |

**Exit.** The test image faults one process, prints `ROBO_PROCESS: alive`, and the bar is still on screen. Splash ceiling 98600.

### Stage L — Syscalls and the app loader

Future programmers add a syscall by adding one row to a table. The echo equation is a test. The loader's accept and reject paths are tests on byte arrays.

**Task L1, Robix feature.** Add `Syscall.Dispatch(table, number, arg) -> Result` and `Image.Entry(bytes) -> Result`. The compiler test asserts the echo returns `arg + 0x1337`, an unknown number is `Error`, a valid header returns an entry, and a truncated blob is `Error`.

**Task L2.** `Kernel/Syscall/RoboSyscall.rx` installs that table: echo, exit, log bytes, fill-rect, pop-input. `Kernel/Loader/RoboLoader.rx` maps `System/Desktop/Desktop.App` into the process from Stage K. `Runtime/RoboRuntime.rx` is the raw entry `RoboRuntimeStart(handoff, input)`.

| File | Test |
| :--- | :--- |
| `Kernel/Syscall/RoboSyscall.rx` | `RoboSyscall.test.rx` covers echo, reject, and fill-rect bounds |
| `Kernel/Loader/RoboLoader.rx` | `RoboLoader.test.rx` covers a valid header and a truncated file |
| `Runtime/RoboRuntime.rx` | `RoboRuntime.test.rx` asserts the present call and the splash ceiling 100000 |

**Exit.** Serial contains `ROBO_SYSCALL: verified syscall round-trip dispatch`, `ROBO_LOADER: System/Desktop/Desktop.App`, and `ROBO_KERNEL: handoff to runtime`. The desktop is now that loaded app.

### Stage M — Files

Future programmers test a directory as a table of names and bytes. Create, read, and a missing path are asserts. The disk driver is the image proof of the same table stored in sectors.

**Task M1, Robix feature.** Add `FS.Lookup`, `FS.Create`, `FS.Read`, and `FS.List` over an in-memory volume. The compiler test creates `Home/Note.txt`, reads it back, and gets `Error` for a missing path.

**Task M2.** `Image/RoboVolume.rx` and `Kernel/FileSystem/RoboFS.rx` use those calls. The bootloader's packed directory is the first mount, so the desktop appears before the disk driver finishes. `Kernel/Devices/RoboDisk.rx` reads sector 0 the way `Examples/Machine/Disk.rx` does, then reads and writes later sectors.

**Task M3.** After a reboot the note is still there. A full disk returns `Error` to the app and the shell keeps running.

| File | Test |
| :--- | :--- |
| `Image/RoboVolume.rx` | `RoboVolume.test.rx` asserts `System/Desktop/Desktop.App` is listed |
| `Kernel/FileSystem/RoboFS.rx` | `RoboFS.test.rx` covers create, read, missing, and full |
| `Kernel/Devices/RoboDisk.rx` | `RoboDisk.test.rx` asserts the sector-0 command sequence and the byte `D` (68) |

**Exit.** Serial contains `ROBO_DISK: sector`, `ROBO_FS: mount`, and, after reboot, `ROBO_FS: Note.txt`. Splash ceiling 99200.

### Stage N — Login, lock, and accounts

Future programmers see the login window in Stage N because the shell from Stage E is already on screen. Password check and path permission are pure tests. The window is the cooked `.rxu` from Stage B.

**Task N1, Robix feature.** Add `Auth.Check(hash, password) -> Result` and `ACL.Allow(user, path, write) -> Bool`. The compiler test accepts a matching pair, rejects a wrong password, lets user 0 write `System/`, and denies that write for any other user. Reads of `System/Desktop` stay allowed.

**Task N2.** `Kernel/Accounts/RoboAuth.rx` and `Kernel/Accounts/RoboACL.rx` are those functions plus the user list on the volume. `Desktop/RoboLogin.rxu` and `Desktop/RoboLock.rxu` paint in front of the desktop. A successful login shows the Stage E shell. Lock puts the login window back. Fast user switching keeps each user's processes in their own address space from Stage K.

**Task N3.** Guest is a user id that `ACL.Allow` denies for every write outside `Home/Guest`.

| File | Test |
| :--- | :--- |
| `Kernel/Accounts/RoboAuth.rx` | `RoboAuth.test.rx` covers match, mismatch, and lock |
| `Kernel/Accounts/RoboACL.rx` | `RoboACL.test.rx` covers user 0, a normal user, and guest |

**Exit.** The login window is the first thing on screen, the password path reaches the desktop, and a denied write prints `ROBO_FS: deny` while `ROBO_PROCESS: alive` follows.

---

## 9. Stages O–AG — the rest of a desktop OS

The screen from Stage E stays up. Each stage adds one part of a Windows-class and macOS-class desktop, and each one still starts by adding a Robix feature.

### Stage O — Dock, launchpad, notifications, control center, spaces

Future programmers lay out stacks of panels with one call, so a notification list and a control center are tests of y positions rather than hand-placed pixels.

**Task O1, Robix feature.** Add `UI.Stack(items, origin, gap)` and `UI.Grid(items, columns, cell)`. The compiler test stacks three notifications and asserts each y, then places eight launchpad icons and asserts the grid.

**Task O2.** `Runtime/Services/RoboNotify.rx` and `Runtime/Services/RoboLaunch.rx`. Documents: `Desktop/RoboDock.rxu`, `Desktop/RoboLaunchpad.rxu`, `Desktop/RoboControlCenter.rxu`, `Desktop/RoboWidgets.rxu`. Spaces are desktops the shell switches by changing the visible window list. Snap from Stage E gains quarters and maximize.

| File | Test |
| :--- | :--- |
| `Runtime/Services/RoboNotify.rx` | `RoboNotify.test.rx` asserts stack order and dismiss |
| `Runtime/Services/RoboLaunch.rx` | `RoboLaunch.test.rx` asserts the grid and the focused space |

**Exit.** Dock, launchpad, a notification, the control center, and a second space are visible. Serial contains `ROBO_SHELL: spaces`.

### Stage P — Search

Future programmers query a name index in a test and get the same hits the search field shows.

**Task P1, Robix feature.** Add `Search.Query(index, text)` with prefix match and a score. The compiler test indexes three app names and asserts the order for a two-letter prefix.

**Task P2.** `Runtime/Services/RoboSearch.rx` indexes app names and file names from `RoboFS`. `Desktop/RoboSearch.rxu` is the field on the bar.

| File | Test |
| :--- | :--- |
| `Runtime/Services/RoboSearch.rx` | `RoboSearch.test.rx` covers prefix, empty query, and a name that is absent |

**Exit.** Typing in the bar lists the desktop app and `Home/Note.txt`. Serial contains `ROBO_SEARCH: hit`.

### Stage Q — Files

Future programmers move, trash, and restore entries in the Stage M table. The Files window is a view of that table.

**Task Q1, Robix feature.** Add `FS.Move`, `FS.Trash`, and `FS.Restore`. The compiler test moves a file, trashes it, lists the trash, and restores it to the old path.

**Task Q2.** `Apps/Files/RoboFilesApp.rx` and `Desktop/RoboFiles.rxu`: sidebar, folder list, preview pane, rename, duplicate, and external disks once Stage J's USB storage and Stage M's disk agree on the block cache. Tags are a string list on the directory entry.

| File | Test |
| :--- | :--- |
| `Apps/Files/RoboFilesApp.rx` | `RoboFilesApp.test.rx` covers move, trash, restore, rename, and a preview name |

**Exit.** The Files window shows `Home` and `System`, and a trashed file comes back. Serial contains `ROBO_FILES: restore`.

### Stage R — Settings

Future programmers read and write typed settings in a test. The Settings window is a document bound to those keys.

**Task R1, Robix feature.** Add `Settings.Get`, `Settings.Set`, and `Settings.Reset` with a key, a type, and a default. The compiler test sets a display width, reads it, and reset returns the default.

**Task R2.** `Runtime/Services/RoboSettings.rx` stores the table in `RoboFS` under the current user. `Apps/Settings/RoboSettingsApp.rx` and `Desktop/RoboSettings.rxu` cover display, sound, network, bluetooth, users, privacy, language, date, power, and updates. Each pane reads the keys this stage defines. Later stages fill the device behind the key.

| File | Test |
| :--- | :--- |
| `Runtime/Services/RoboSettings.rx` | `RoboSettings.test.rx` covers get, set, reset, and an unknown key |
| `Apps/Settings/RoboSettingsApp.rx` | `RoboSettingsApp.test.rx` asserts each pane's key list |

**Exit.** Settings opens from the dock and the display pane shows the handoff width and height. Serial contains `ROBO_SETTINGS: display`.

### Stage S — Sound and camera

Future programmers mix sample frames in a test, including silence and a clipped peak. The device driver only copies the buffer the mix already produced.

**Task S1, Robix feature.** Add `Audio.Mix(frames, volume)` and `Audio.Peak(frames)`. The compiler test mixes silence to silence, applies a half volume, and reports a clipped peak as the maximum sample.

**Task S2.** `Kernel/Devices/RoboAudio.rx` plays the mixed buffer through the bound driver from Stage JA, or through `RoboIntelAudio` when Device Manager has installed it. Per-app volume is a `Settings` key. The camera opens through `RoboBasicCamera` or the installed vendor camera package. Privacy keys from Stage R gate both devices, and a denied open returns `Error`.

| File | Test |
| :--- | :--- |
| `Kernel/Devices/RoboAudio.rx` | `RoboAudio.test.rx` covers mix, peak, and a denied privacy key |

**Exit.** The control center changes the volume and serial contains `ROBO_AUDIO: mix`. A denied camera open prints `ROBO_CAMERA: deny`.

### Stage T — Network

Future programmers build an ICMP packet and its checksum in a test. The VirtIO driver sends the bytes the test already fixed.

**Task T1, Robix feature.** Add `Net.Checksum(bytes)` and `ICMP.Build(id, seq, payload)`. The compiler test checks the checksum of a known header and the echo layout.

**Task T2.** `Kernel/Devices/RoboNet.rx` brings up the bound Ethernet driver, then DHCP, ARP, ICMP, UDP, TCP, and DNS. On QEMU that driver is `RoboBasicVirtio`. On Intel Ethernet it is `RoboBasicNet` until Device Manager installs `RoboIntelEthernet`. Wi-Fi runs when the installed vendor package from Stages JC, JG, or JH is bound. The firewall is an allow table in front of dispatch. A VPN is one extra interface that owns a route. Settings shows the address. Device Manager uses this stack to download a driver package from the catalog address.

| File | Test |
| :--- | :--- |
| `Kernel/Devices/RoboNet.rx` | `RoboNet.test.rx` covers checksum, ICMP bytes, an allow-table reject, and a DNS name lookup from a fixture |

**Exit.** QEMU user-net answers the echo. Serial contains `ROBO_NET: echo`. A blocked port prints `ROBO_NET: deny`.

### Stage U — Bluetooth

Future programmers parse an advertisement in a test and read the device name. The controller driver feeds the same byte layout.

**Task U1, Robix feature.** Add `BT.Parse(advertisement) -> Result` for the name and the address bytes. The compiler test uses a fixed advertisement and asserts the name, and a short buffer is `Error`.

**Task U2.** `Kernel/Devices/RoboBluetooth.rx` scans and pairs through the bound driver, `RoboBasicBluetooth` or an installed Intel, Broadcom, or other vendor package. A keyboard and a headset join the Stage C queue and the Stage S mixer. Settings lists the pair.

| File | Test |
| :--- | :--- |
| `Kernel/Devices/RoboBluetooth.rx` | `RoboBluetooth.test.rx` covers a valid advertisement, a short buffer, and pair state |

**Exit.** A QEMU or test double advertisement prints `ROBO_BT: pair`.

### Stage V — Power

Future programmers decide sleep, warning, and shutdown from a battery percent in a test. The firmware call is the image side of a decision the test already made.

**Task V1, Robix feature.** Add `Power.Decide(percent, onAC) -> Int` with the values run, warn, sleep, and shutdown. The compiler test covers 100 on AC, 15 on battery, and 3 on battery.

**Task V2.** `Kernel/Devices/RoboPower.rx` reads the battery through `RoboBasicPower` and applies that decision. Sleep saves the volume and the process list and restores the Stage N lock screen. Hibernate writes the image to the volume. Restart and shutdown go through the same path the shell menu calls.

| File | Test |
| :--- | :--- |
| `Kernel/Devices/RoboPower.rx` | `RoboPower.test.rx` covers the four decisions and the lock-on-wake flag |

**Exit.** The control center shows a percent. Sleep then wake shows the lock screen. Serial contains `ROBO_POWER: wake`.

### Stage W — Displays

Future programmers convert a pixel and a scale factor in a test. Arranging two monitors is a test of two rectangles that share an edge.

**Task W1, Robix feature.** Add `Display.Scale(px, percent)` and `Display.Arrange(a, b, edge)`. The compiler test scales 84 by 200 percent to 168, and places the second display to the right of the first.

**Task W2.** `Kernel/Devices/RoboDisplay.rx` presents through the bound display driver. That is `RoboBasicDisplay` until Device Manager installs an Intel or NVIDIA package. Night color is a settings key that tints the present. HiDPI uses `Display.Scale` on the cooked layout before paint. The wallpaper and the windows stay on the display that owns them.

| File | Test |
| :--- | :--- |
| `Kernel/Devices/RoboDisplay.rx` | `RoboDisplay.test.rx` covers scale, arrangement, and which display owns a window |

**Exit.** One QEMU display still shows the desktop. A two-display test image prints `ROBO_DISPLAY: 2` and the bar stays on the primary.

### Stage X — Clipboard, drag, and screenshots

Future programmers set and get clipboard bytes in a test, and a drag is a hit plus a payload. A screenshot is a `Frame` the Stage A asserts can read.

**Task X1, Robix feature.** Add `Clip.Set`, `Clip.Get`, and `Drag.Payload(hit, bytes)`. The compiler test sets a string, reads it, replaces it, and asserts a drag payload names the source box.

**Task X2.** `Runtime/Services/RoboClip.rx`. Drag between Files and the editor uses `Drag.Payload`. Screenshot writes a `Frame` into `Home/Pictures`. Screen recording writes a sequence of frames into the wallpaper video container from Stage AD once that header exists, and until then it writes a numbered set of frames.

| File | Test |
| :--- | :--- |
| `Runtime/Services/RoboClip.rx` | `RoboClip.test.rx` covers set, get, replace, drag payload, and a screenshot size |

**Exit.** A copy in Files pastes in the editor. Serial contains `ROBO_CLIP: paste`. The picture file is on the volume.

### Stage Y — Fonts, languages, accessibility, printing

Future programmers shape text and move focus in tests. A printer job is a list of pages the test can count. The devices consume those results.

**Task Y1, Robix feature.** Add `Font.Measure(text, size)`, `Focus.Next(order, current)`, and `Print.Paginate(frame, pageHeight)`. The compiler test measures a known string, tabs from the first field to the second, and splits a tall frame into two pages.

**Task Y2.** `Runtime/GPU/RoboText.rx` uses `Font.Measure` and the cooked font pack. Keyboard layouts and a right-to-left flag are settings. Accessibility adds full keyboard reach, a larger scale through `Display.Scale`, and a high-contrast theme that swaps the cooked colors. `Kernel/Devices/RoboPrint.rx` sends `Print.Paginate` output through `RoboBasicPrint`, or to a PDF file on the volume.

| File | Test |
| :--- | :--- |
| `Runtime/GPU/RoboText.rx` | the Stage E test gains measure and focus cases |
| `Kernel/Devices/RoboPrint.rx` | `RoboPrint.test.rx` asserts page count and a high-contrast color swap |

**Exit.** Tab reaches every control on the login window. A printed note produces two pages. Serial contains `ROBO_PRINT: pages`.

### Stage Z — Install, update, recovery, backup

Future programmers compare versions and plan a backup in tests. The installer is a boot path that copies a volume the test already described.

**Task Z1, Robix feature.** Add `Update.Newer(current, offered)`, `Backup.Plan(paths)`, and `Recovery.Select(entries)`. The compiler test says `1.2` is newer than `1.1`, plans `Home` without `Trash`, and selects the last good boot entry.

**Task Z2.** `Runtime/Services/RoboUpdate.rx` downloads through Stage T into a volume slot and boots it on the next start. `Apps/Install/RoboInstallApp.rx` is the first-run window: user, disk, and copy. Recovery is a boot menu from `Recovery.Select`. Backup copies the plan onto a second disk.

| File | Test |
| :--- | :--- |
| `Runtime/Services/RoboUpdate.rx` | `RoboUpdate.test.rx` covers newer, plan, and recovery select |
| `Apps/Install/RoboInstallApp.rx` | `RoboInstallApp.test.rx` asserts the steps user, disk, and copy |

**Exit.** An install image reaches the login window. A newer build prints `ROBO_UPDATE: staged`. Recovery prints `ROBO_RECOVERY: last-good`.

### Stage AA — Packages, the store, and permission prompts

Future programmers verify a package header in a test before the loader maps it. A permission prompt is a result the test can deny.

**Task AA1, Robix feature.** Add `Package.Verify(header) -> Result` using the HMAC-SHA256 check the toolchain already uses for `robix add`, and `Permit.Ask(app, right) -> Result`. The compiler test rejects a bad magic and a bad mac, and a denied camera right returns `Error`.

**Task AA2.** `Runtime/Services/RoboPackage.rx` installs into `System/Apps` and `RoboLoader` refuses an unverified image. `Apps/Store/RoboStoreApp.rx` lists packages. The first launch of a right shows `Desktop/RoboPermit.rxu`. Rights are files, network, camera, and microphone, enforced by `RoboACL` and the Stage S and Stage T denials.

| File | Test |
| :--- | :--- |
| `Runtime/Services/RoboPackage.rx` | `RoboPackage.test.rx` covers verify, a bad mac, and each right |
| `Apps/Store/RoboStoreApp.rx` | `RoboStoreApp.test.rx` asserts the installed list |

**Exit.** A bad package prints `ROBO_LOADER: reject`. A denied camera right prints `ROBO_CAMERA: deny`. The store window lists `Desktop.App`.

### Stage AB — Terminal, task manager, logs, and disks

Future programmers push log lines into a fixed ring and sample a process table in tests. The windows show those tables.

**Task AB1, Robix feature.** Add `Log.Ring(capacity)` with push and list, and `Task.Sample(processes)` with name, user, and state. The compiler test fills the ring past capacity and asserts the oldest line is gone, and samples two processes.

**Task AB2.** `Runtime/Services/RoboLog.rx`. Apps: `Apps/Terminal/RoboTerminal.rx`, `Apps/Tasks/RoboTasksApp.rx`, and `Apps/Disks/RoboDisksApp.rx`. Device Manager is the Stage JB window, opened from this same launchpad group. The terminal's typed line is the Stage C key path. Disks shows the Stage M volumes and erase returns `Error` for the system volume.

| File | Test |
| :--- | :--- |
| `Runtime/Services/RoboLog.rx` | `RoboLog.test.rx` covers the ring and a sample |
| `Apps/Terminal/RoboTerminal.rx` | `RoboTerminal.test.rx` asserts a typed line |
| `Apps/Tasks/RoboTasksApp.rx` | `RoboTasksApp.test.rx` asserts both process names |
| `Apps/Disks/RoboDisksApp.rx` | `RoboDisksApp.test.rx` asserts the system volume cannot be erased |

**Exit.** Terminal, task manager, disks, and Device Manager open from the launchpad. Serial contains `ROBO_TERMINAL: line` and `ROBO_LOG: ring`.

### Stage AC — Inbox apps

Future programmers test an editor buffer, a calculator value, an image header, and a browser location as ordinary functions. The windows are `.rxu` documents on top of the shell.

**Task AC1, Robix feature.** Add `Text.Edit(buffer, op)`, `Calc.Eval(expr)`, `Image.Header(bytes)`, and `Web.Location(url)`. The compiler test inserts a character, evaluates `20+22` to 42, rejects a short image header, and rejects an empty URL.

**Task AC2.** `Apps/Editor/RoboEditorApp.rx`, `Apps/Calculator/RoboCalculatorApp.rx`, `Apps/Pictures/RoboPicturesApp.rx`, `Apps/Browser/RoboBrowserApp.rx`, each with its `.rxu`. The browser uses Stage T to load bytes and Stage B's painter for the page it understands. A page it does not understand shows the location and the byte count.

| File | Test |
| :--- | :--- |
| `Apps/Editor/RoboEditorApp.rx` | `RoboEditorApp.test.rx` covers insert and delete |
| `Apps/Calculator/RoboCalculatorApp.rx` | `RoboCalculatorApp.test.rx` asserts 42 |
| `Apps/Pictures/RoboPicturesApp.rx` | `RoboPicturesApp.test.rx` covers a valid header and a short one |
| `Apps/Browser/RoboBrowserApp.rx` | `RoboBrowserApp.test.rx` covers a location and an empty one |

**Exit.** Each app opens from the dock and its test action is visible. Serial contains `ROBO_APP: editor`, `calculator`, `pictures`, and `browser`.

### Stage AD — Motion wallpaper and studio

Future programmers step a cooked video by frame index in a test. The studio scene stays a `.rx3d` file rendered by the Stage D call.

**Task AD1, Robix feature.** Add `Video.Frame(blob, index) -> Result` for the `.robovid` container the handoff already names. The compiler test reads frame 0 and gets `Error` for an index past the end.

**Task AD2.** The wallpaper can be `Scenes/RoboWallpaper.rx3d` or the video. `Apps/Studio/RoboStudioApp.rx` and `Scenes/RoboStudio.rx3d` orbit a mesh with the Stage C drag path. `UI Source` inside the scene is the only bridge to its `.rxu` panel.

| File | Test |
| :--- | :--- |
| `Apps/Studio/RoboStudioApp.rx` | `RoboStudioApp.test.rx` asserts frame 0, a past-end error, and an orbit delta |

**Exit.** The desktop shows a moving wallpaper frame. The studio window shows the mesh. Serial contains `ROBO_VIDEO: frame`.

### Stage AE — Secure boot and disk sealing

Future programmers hash a boot blob and open a sealed block in tests. The hash is the HMAC-SHA256 the package tool already trusts. The boot chain and the disk use that result.

**Task AE1, Robix feature.** Add `Crypto.SHA256(bytes)` and `Crypto.HMAC(key, bytes)` as Robix calls over the security code the toolchain already runs for package keys, plus `Crypto.Seal` and `Crypto.Open` with the same key schedule. The compiler test checks a known SHA256 vector, a known HMAC, and a seal/open round trip. A wrong key returns `Error`.

**Task AE2.** `Kernel/Security/RoboBootChain.rx` checks the bootloader, kernel, and runtime hashes before `RoboKernelStart` draws anything. A mismatch halts with `ROBO_BOOT: chain`. `Kernel/Security/RoboDiskSeal.rx` seals user-volume sectors. The system volume the desktop boots from stays readable by the bootloader.

| File | Test |
| :--- | :--- |
| `Kernel/Security/RoboBootChain.rx` | `RoboBootChain.test.rx` covers a matching chain and a broken one |
| `Kernel/Security/RoboDiskSeal.rx` | `RoboDiskSeal.test.rx` covers seal, open, and a wrong key |

**Exit.** A good image still reaches the login window. A flipped kernel byte prints `ROBO_BOOT: chain` and does not present. Serial for the good boot contains `ROBO_BOOT: chain ok`.

### Stage AF — A hardware GPU and the other CPUs

Future programmers keep one present call. A board either implements it with the soft GPU or with a hardware scanout hook. The desktop stages stay on `pc` until that board has a framebuffer handoff.

**Task AF1, Robix feature.** Add `GPU.Present(frame, rect)` as the single scanout call, with a soft implementation and a hook. The compiler test presents a `Frame` and `AssertPixel` reads it back. `ASM` for a non-pc target keeps the existing check: a mnemonic that board does not have fails the build.

**Task AF2.** The same `RoboKernelStart` links for `x86_64-unknown-none`, `aarch64-unknown-none`, `arm-unknown-none`, and `riscv64-unknown-none`, using the board pattern in `Examples/Machine`. Each writes `ROBO_KERNEL: start` on its UART. `CPU.Halt` is `hlt` on x86-64 and `wfi` on the other three. PCI, PS/2, xHCI, and the desktop require the `pc` framebuffer. A desk machine is recorded only when that machine's serial capture exists.

| File | Test |
| :--- | :--- |
| `Runtime/GPU/RoboGPU.rx` | the Stage A test gains a `GPU.Present` round trip |

**Exit.** Four QEMU serial files contain `ROBO_KERNEL: start`. The `pc` image still shows the desktop.

### Stage AG — The release image

Future programmers run one test command and one boot command. The checklist is a test that names every earlier exit line, so a missing stage fails on Windows before a long QEMU run.

**Task AG1, Robix feature.** Add `robix test RoboticelaOS`, which runs every `TestFile` under this tree on the interpreter and the native compiler, and add `OS.Checklist(lines)` which asserts the required serial lines are present in order. The compiler test feeds a full log and a log with one line removed.

**Task AG2.** `robixc RoboticelaOS/RoboticelaOS.rxb -O3 -o Build/RoboticelaOS/RoboticelaOS.img` builds the ROM, bootloader, kernel, runtime, packs, and volume. The checked-in QEMU line boots that image to the login window and then the desktop.

| File | Test |
| :--- | :--- |
| `Image/RoboVolume.rx` | `RoboVolume.test.rx` already covers the required paths; this stage adds the checklist fixture next to it |

**Exit.** `robix test RoboticelaOS` passes. From power-on the serial file contains, in order:

```text
ROBO_BOOT: chain ok
ROBO_BOOT: handoff
ROBO_KERNEL: start
ROBO_MEMORY: pages
ROBO_IDT: gates
ROBO_CPU: cores online
ROBO_INPUT: ready
ROBO_FS: mount
ROBO_SYSCALL: verified syscall round-trip dispatch
ROBO_KERNEL: handoff to runtime
ROBO_RUNTIME: present
ROBO_SHELL: bar
```

The screen shows the login window, then the desktop: bar, dock, wallpaper, and windows. That boot is RoboticelaOS.

---

## 10. Order

```text
A picture  -> B rxu shell -> C clicks -> D wallpaper -> E windows
E -> F bootloader -> G memory -> H time -> I cores -> J buses
J -> JA built-in drivers -> JB Device Manager -> JC Intel
JC -> JD AMD -> JE NVIDIA -> JF Realtek -> JG Broadcom -> JH Qualcomm -> JI next vendor
JI -> K processes -> L syscalls -> M files -> N login
N -> O dock and spaces -> P search -> Q files -> R settings
R -> S sound -> T network -> U bluetooth -> V power -> W displays
W -> X clipboard -> Y fonts and print -> Z install and backup
Z -> AA packages -> AB tools -> AC inbox apps -> AD video
AD -> AE chain and seal -> AF other CPUs -> AG one image
```

A through E are the path that puts the OS on screen. F through J find the machine. JA binds a built-in driver for every class, JB installs a real driver from Device Manager, and JC is the Intel milestone. JD through JI repeat that milestone for later vendors. K through N make the screen a real computer. O through AG are the rest of the desktop. Each stage is a Robix feature first, then the OS files, then a test per file.
