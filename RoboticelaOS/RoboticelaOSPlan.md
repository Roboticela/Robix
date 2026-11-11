# RoboticelaOS

RoboticelaOS is a desktop operating system in the same shape as macOS. The kernel runs the machine. A launch service starts the userland. A window server owns the screen. Applications are separate processes with their own address spaces. The kernel does not draw the Dock, the Finder, or Settings.

The image is Robix. The bootloader, the kernel, and the drivers are `.rx`. Desktop windows are `.rxu`. The machine contract stays the one in `RefCode` and `RobixUI/RoboShared`.

The proof target is QEMU `pc` on `x86_64-unknown-none`.

Use the names as GPU not Gpu, CPU not Cpu, PCI not Pci, and the same for every other abbreviation.

---

## 1. The shape of the system

macOS is three layers that stay apart. RoboticelaOS uses the same split, with Robo names.

| Layer | macOS name | RoboticelaOS name | Owns |
| :--- | :--- | :--- | :--- |
| Kernel | XNU: Mach, BSD, IOKit | `RoboKernel`: core, POSIX, IO | CPUs, pages, threads, ports, files, drivers, syscalls |
| Boot service | launchd | `RoboLaunch` | First user process. Starts every later service |
| Window server | WindowServer | `RoboWindowServer` | Framebuffer, cursor, compositing, input routing |
| Session | loginwindow | `RoboLogin` | Accounts, lock, the session |
| Shell apps | Finder, Dock, System Settings | `RoboFinder`, `RoboDock`, `RoboSettings` | Files, launching, preferences |

A program draws by sending a command to `RoboWindowServer`. The server composites and presents. A driver maps a device and raises an interrupt. The scheduler runs that work on every core that came online.

RAM, cores, and the framebuffer follow the machine. The bootloader reads the installed size, keeps the kernel reserve, and publishes the rest as one free range. The identity map covers that RAM, plus the 1 GiB slot that holds the local APIC. A core that is online gets its own stack from the top of that RAM, paints its own rows of the full framebuffer, and then waits. A core that never answers does not stop the desktop: the boot processor finishes the missing rows.

### 1.1 What the current image already proves

These rows are the bootloader, the kernel, and the first desktop documents. Each box is filled on its own.

| | Stage | What it proves |
| :--- | :--- | :--- |
| [x] | A | A `Frame` can be filled and read back on the host, and QEMU can show those pixels |
| [x] | B | A cooked `.rxu` can be painted into that frame |
| [x] | C | A scripted pointer event can hit a cooked box |
| [x] | E | Drag, snap, a menu, a clock, and a cursor can be applied to those boxes |
| [x] | F | The bootloader writes a real handoff, the splash is on screen, a bad magic halts |
| [x] | G | Installed RAM is measured, mapped, and published as the free range above the kernel reserve |
| [x] | H | The IDT is loaded, `int3` prints `ROBO_IDT: trap 3`, the timer ticks, CMOS seconds decode |
| [x] | I | On `-smp 4`, four cores enter 64-bit mode and the serial line is `ROBO_CPU: cores online4`. Each application processor paints its own rows of the full framebuffer |

Stages A, B, C, and E are desktop documents. The kernel may paint them after the splash until `RoboWindowServer` takes the framebuffer. Stage I puts every online core to work on its own slice of that framebuffer. The next kernel stage replaces that one job with the scheduler.

### 1.2 What the screen shows, in order

| | When | On the screen |
| :--- | :--- | :--- |
| [x] | F | The splash, then the desktop documents from A, B, C, and E |
| [x] | J | The same screen. The serial line shows a thread running on every online core |
| [ ] | S | The splash, then a blank desktop owned by `RoboWindowServer` |
| [ ] | T | The login window, drawn by `RoboLogin` through the window server |
| [ ] | U | Menu bar, Dock, Finder windows, notifications, Control Center, Spaces |
| [ ] | V–Z | Settings, files, Device Manager, sound, network, and displays |
| [ ] | AG | The window server presents through the bound display driver |

---

## 2. Rules for every stage

**The first task is a Robix feature.** Each stage opens with the language or toolchain feature that makes that stage easier for every later programmer. That feature is implemented and tested in the Robix compiler before any OS file for the stage is written. The OS code then calls the feature.

**Every `.rx` file has a `.test.rx` beside it.** The test uses `Test native` and `Assert.Equal`, `Assert.True`, `Assert.False`, or `AssertPixel`. `robix test` runs the interpreter and the native compiler. A test that touches a port, a control register, or an interrupt keeps the pure part on the host and lists the machine call in `RoboticelaOS/Tests/NATIVE_ONLY.md`. The image boot is the second proof, recorded in `Documentation/Status.md`.

**A checkmark is one line.** Every stage heading, every task, and every exit starts as `[ ]`. Change that one box to `[x]` after that line’s own test or QEMU capture is recorded in `Documentation/Status.md`. The stage heading is checked only after every task and the exit under it are checked. A stage still needs all three proofs: the Robix feature test, every new `.test.rx` on both engines, and the screen pixel or serial line from QEMU.

**Calls stay on their side of the line.** Hosted calls (`Print`, `File`, `Thread`, `Network`, `Args`) stay out of the image. Machine calls (`CPU`, `Port`, `Memory.Pages`, `Memory.Address`, `Memory.At`, `Timer`, `System.Call`, `Volatile`, `ASM`) stay out of window documents and application code. An application asks for a page, a file, or a drawing through a syscall. Pure functions are the ones the tests call.

Kernel tables are structs. There is no garbage collector. `Unsafe` owns frames the kernel allocates. `@Packed`, `@Align`, `Memory.SizeOf`, `Memory.OffsetOf`, and global `const` values are already native-complete.

---

## 3. Sources that stay as they are

| Source | What it already decides |
| :--- | :--- |
| `Examples/Machine` | Pages, GDT, memory map, VGA cell `0xB8000`, CMOS, two stacks, timer, ATA sector 0. `Output: Kernel`, `Target: x86_64-unknown-none`, `Entry: Start`, `Profile: embedded`. |
| `Examples/Machine/Bios.s` | 16-bit reset. Robix starts after long mode. |
| `RefCode/RoboKernelMain.cpp` | Bring-up order and splash ceilings. |
| `RefCode/RoboCPU.cpp` | MADT, 16 cores, trampoline at `0x8000`, mailboxes, INIT-SIPI. |
| `RefCode/RoboPCI.hpp` and the input headers | Bus scan and the input hosts. |
| `RobixUI/RoboShared/RoboHandoff.hpp` | Magic `0x444E4852` (`RHND`), version 4. |
| `RobixUI/RoboShared` | Cursor pack `RCUR`, icon catalog `RICP`, cooked icons `ROIC`, font pack. |

The AP trampoline passes the first argument in `RDI` and keeps `RSP` 8 bytes off a 16-byte boundary. Mailbox addresses stay the ones in `RoboCPU.cpp`: trampoline `0x8000`, CR3 `0x8120`, stack `0x8128`, entry `0x8130`, APIC id `0x8138`, ready `0x8160`. INIT IPI `0x4500`, SIPI `0x4608`. `ROBO_CPU_MAX` is 16.

### 3.1 RoboHandoff

`Shared/RoboHandoff.rx` matches `RoboHandoff.hpp` field for field. Magic `0x444E4852`, version `4`. Framebuffer pitch is `pixelsPerScanLine * bytesPerPixel`, 4 bytes per pixel, XRGB8888. Memory types: 1 free, 2 boot, 3 runtime, 4 ACPI, at most 48 ranges. `bootName` is `RoboticelaOS`. A bad magic prints `ROBO_KERNEL: bad handoff` and halts.

Splash ceilings, milli-percent: memory 97400, IDT 97800, CPUs 98200, scheduler 98400, processes 98600, input 98800, drivers 99000, filesystem 99200, syscalls 99400, window server 99600, runtime 100000. `Advance` never moves backward.

### 3.2 The syscall line

User code enters the kernel through one gate. The first table is small and grows by a row each time a stage needs a new call. Numbers stay stable once they ship.

| Number | Call | Result |
| :--- | :--- | :--- |
| 0 | `Echo` | The argument plus `0x1337` |
| 1 | `Exit` | The process ends |
| 2 | `Log` | Bytes on the serial port |
| 3 | `Yield` | The scheduler picks another thread |
| 4 | `Port` | Send or receive one IPC message |
| 5 | `Map` | A page in the caller’s address space |
| 6 | `File` | Lookup, read, write, or list |
| 7 | `Draw` | A command buffer for `RoboWindowServer` only |

`Draw` is rejected for every process except `RoboWindowServer`. An application that wants a pixel sends a message to the window server, which is the process allowed to call `Draw`.

---

## 4. Tree

Every `.rx` file in this tree has a sibling `.test.rx`.

```text
RoboticelaOS/
  RoboticelaOSPlan.md
  RoboticelaOS.rxb
  Tests/NATIVE_ONLY.md
  Shared/          RoboHandoff.rx  RoboTypes.rx  RoboPacks.rx
  Boot/            RoboBootROM.s  RoboBootloader.rx
  Kernel/          RoboKernel.rx
    Memory/        RoboMemoryManager.rx  RoboAddressSpace.rx
    Interrupt/     RoboIDT.rx
    CPU/           RoboCPU.rx  RoboScheduler.rx
    IPC/           RoboPort.rx
    POSIX/         RoboProcess.rx  RoboCredential.rx
    Syscall/       RoboSyscall.rx
    FileSystem/    RoboVFS.rx  RoboVolume.rx
    PCI/           RoboPCI.rx
    IO/            RoboRegistry.rx  RoboDriver.rx
      Basic/       one built-in driver per device class
      Intel/       the first vendor catalog
      Vendors/     one folder per later vendor
    Devices/       RoboSerial.rx  RoboSplash.rx  RoboTimer.rx  RoboDisk.rx
  Desktop/         .rxu documents for the windows
  User/
    RoboLaunch/    the first process
    RoboWindowServer/
    RoboLogin/
    RoboFinder/  RoboDock/  RoboSettings/  RoboFiles/  RoboDevices/
  Image/           the boot volume
```

`Desktop/` holds the `.rxu` windows. `RoboKernel` may paint those documents after the splash and stops when `RoboWindowServer` has mapped the framebuffer. Finder, the Dock, Settings, and Device Manager live under `User/`.

QEMU for a kernel proof:

```text
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -serial file:Build/RoboticelaOS/stage.serial -vga std
```

`RoboticelaOS/RunQEMU.ps1` is that command with a window.

---

## 5. Desktop documents

These documents are the windows. The window server becomes their owner in Stage S.

### [x] Stage A — A frame the tests can read

- [x] **Task A1, Robix feature.** `Frame.Create`, `Frame.Fill`, `Frame.Pixel`, and `AssertPixel`. The compiler test fills an 84×44 rectangle with `0E7AFF` and checks the origin and a pixel outside the rectangle.

- [x] **Task A2.** `Desktop/RoboGPU.rx` clears `#080A10`, fills the bar `#0E7AFF`, and blits the logo.

- [x] **Exit.** Both engines pass. QEMU shows the frame. Serial contains `ROBO_GPU: present`.

### [x] Stage B — A cooked document

- [x] **Task B1, Robix feature.** `robix ui cook File.rxu -o File.rxul`, and `UI.LoadCooked`. The compiler test cooks a button and checks an 84×44 box whose role is `Button`.

- [x] **Task B2.** The cooked bar, icons, and one window are painted.

- [x] **Exit.** Serial contains `ROBO_UI: paint`.

### [x] Stage C — A scripted pointer

- [x] **Task C1, Robix feature.** `Input.Play(script)` pushes pointer and key events. The compiler test plays a move and a click and asserts the queue.

- [x] **Task C2.** A hit test maps a point to a cooked box.

- [x] **Exit.** Serial contains `ROBO_UI: click`.

### [x] Stage E — Layout records

- [x] **Task E1, Robix feature.** `UI.Place`, `UI.Hit`, `UI.Drag`, and `UI.Snap` return a new layout and a cursor kind. The compiler test drags a window and snaps it to the left half.

- [x] **Exit.** Serial contains `ROBO_SHELL: bar`. The Dock and the menu bar in Stage U are documents painted by the window server.

---

## 6. Bootloader

### [x] Stage F — Bootloader and handoff

- [x] **Task F1, Robix feature.** `@Packed`, `@Align`, `Memory.SizeOf`, `Memory.OffsetOf`, and global `const` are native-complete. The compiler test puts `magic` at offset 0 and `version` at offset 4. A mismatched size fails the build.

- [x] **Task F2.** The bootloader fills the handoff. A bad magic prints `ROBO_KERNEL: bad handoff` and halts. The splash uses the RefCode layout at 1920×1080 and a constant-speed loader.

- [x] **Exit.** Serial contains `ROBO_BOOT: handoff` and `ROBO_KERNEL: start`. The ROM test image prints byte `I` (73).

### [ ] Stage AA — Install, update, recovery, and backup

- [ ] **Task AA1, Robix feature.** Add `Update.Newer`, `Backup.Plan`, and `Recovery.Select`. The compiler test says `1.2` is newer than `1.1`, plans `Home` without `Trash`, and selects the last good boot entry.

- [ ] **Task AA2.** The installer is a boot path: user, disk, and copy. Updates download through Stage Y into a volume slot and boot on the next start. Recovery is a boot menu. Backup copies the plan onto a second disk.

- [ ] **Exit.** An install image reaches the login window. Serial contains `ROBO_UPDATE: staged` and `ROBO_RECOVERY: last-good`.

### [ ] Stage AF — Secure boot and disk sealing

- [ ] **Task AF1, Robix feature.** Add `Crypto.SHA256`, `Crypto.HMAC`, `Crypto.Seal`, and `Crypto.Open` over the HMAC-SHA256 the toolchain already trusts. The compiler test checks a known vector, a known HMAC, and a round trip. A wrong key returns `Error`.

- [ ] **Task AF2.** The boot chain checks the bootloader, the kernel, and `RoboLaunch` before the splash finishes. A mismatch halts with `ROBO_BOOT: chain`. User-volume sectors can be sealed. The system volume stays readable by the bootloader.

- [ ] **Exit.** A good image still reaches the login window. A flipped kernel byte prints `ROBO_BOOT: chain` and does not present. The good boot prints `ROBO_BOOT: chain ok`.


---

## 7. Kernel

### [x] Stage G — Identity map

- [x] **Task G1, Robix feature.** `Memory.Frame()` and `Page.Entry(phys, flags)`. The compiler test expects `8195`, `4096`, `240`, `127`, and `-2`.

- [x] **Task G2.** `Kernel/Memory/RoboMemoryManager.rx` reads the installed byte count from the handoff. It identity-maps one 1 GiB slot for each started gibibyte of that RAM, and it also maps the local APIC slot. The free pool is every page above the 16 MiB kernel reserve, and it stops at the measured end. The page allocator does not hand out the unbacked part of a huge page or the APIC slot.

- [x] **Exit.** Serial contains `ROBO_MEMORY: pages` and `ROBO_MEMORY: map`.

### [x] Stage H — Interrupts and time

- [x] **Task H1, Robix feature.** `CPU.Gate` returns the low and high descriptor words. `Clock.Seconds` decodes a CMOS BCD byte. The compiler test uses handler `305419896` and seconds `42` from byte `66`.

- [x] **Task H2.** Vectors 0–31 print `ROBO_IDT: trap` and the vector, then halt. The timer prints `ROBO_TIMER: tick` and returns, so the boot continues.

- [x] **Exit.** Serial contains `ROBO_IDT: gates`, `ROBO_CLOCK: seconds`, and `ROBO_TIMER: tick`. The trap image prints `ROBO_IDT: trap 3`.

### [x] Stage I — Cores enter 64-bit mode

- [x] **Task I1, Robix feature.** `Volatile.Load32` and `Volatile.Store32`. The compiler test stores `1` through a pointer and reads it back.

- [x] **Task I2.** `Kernel/CPU/RoboCPU.rx` finds the RSDP and the MADT, writes the trampoline at `0x8000`, and sends INIT-SIPI. Each application processor stores its index, waits for the paint job, fills only its rows of the full framebuffer, and then halts. A core that never answers leaves the count unchanged. The boot processor paints the missing rows, so the desktop still appears. Page tables above 4 GiB keep the boot processor alone.

- [x] **Exit.** On `-smp 4`, serial contains `ROBO_CPU: cores online4`. On `-smp 1`, serial contains `ROBO_CPU: cores online1` and the desktop documents still appear.

The one paint job is the end of this stage. Stage J replaces it with the idle thread and a real run queue.

### [x] Stage J — A scheduler on every core

Future programmers enqueue a thread and ask which core should run it. The image runs that queue on every core that Stage I brought online.

- [x] **Task J1, Robix feature.** Add `Sched.Enqueue`, `Sched.Pick`, and `Sched.Block`. The compiler test enqueues three threads, picks them in priority order, and a blocked thread stays off the pick until it is woken. `Atomic.FetchAdd` remains the counter for a queue length.

- [x] **Task J2.** `Kernel/CPU/RoboScheduler.rx` is that queue. The timer interrupt from Stage H preempts the running thread. `ApEntry` enters the idle loop on its own core. The idle loop halts only until the next interrupt, then picks again. A thread can be pinned or can move to another online core.

| File | Test |
| :--- | :--- |
| `Kernel/CPU/RoboScheduler.rx` | `RoboScheduler.test.rx` covers priority, block, wake, and a pick that skips a halted core |

- [x] **Exit.** On `-smp 4`, serial contains `ROBO_SCHED: core 0`, `core 1`, `core 2`, and `core 3`, then `ROBO_SCHED: migrate`. On `-smp 1`, serial contains `ROBO_SCHED: core 0` and the desktop documents still appear. Splash ceiling 98400.

### [x] Stage K — An address space per process

Future programmers split a page table into a kernel half and a user half, and decide a fault, without crashing the host.

- [x] **Task K1, Robix feature.** Add `Page.Split(root, userStart)` and `Fault.Decide(fromUser) -> Int`. The compiler test checks the supervisor and user flag bits. A user fault returns continue. A kernel fault returns halt.

- [x] **Task K2.** `Kernel/Memory/RoboAddressSpace.rx` builds one root per process. The kernel half is shared and supervisor-only. A user store into a kernel page faults that process. The desktop and the other processes keep running.

| File | Test |
| :--- | :--- |
| `Kernel/Memory/RoboAddressSpace.rx` | `RoboAddressSpace.test.rx` covers the split flags, a user fault, and a second space that stays mapped |

- [x] **Exit.** A test process prints `ROBO_VM: fault` and serial then contains `ROBO_PROCESS: alive`. A kernel fault still halts.

### [ ] Stage L — Ports, the way processes talk

Future programmers send a message through a port and receive it on the other end. The window server, the drivers, and the desktop will all use this, the way Mach ports connect macOS processes.

- [ ] **Task L1, Robix feature.** Add `Port.Create`, `Port.Send`, and `Port.Receive`. The compiler test sends a 4-byte message, receives those bytes, and a receive on an empty port returns a block result that Stage J can wait on. A send to a closed port returns `Error`.

- [ ] **Task L2.** `Kernel/IPC/RoboPort.rx` is that queue, bound to a process. The kernel is the only creator of the well-known ports: `Window`, `Launch`, `Driver`, and `Log`.

| File | Test |
| :--- | :--- |
| `Kernel/IPC/RoboPort.rx` | `RoboPort.test.rx` covers send, receive, empty, and closed |

- [ ] **Exit.** Two kernel threads exchange a message and serial contains `ROBO_PORT: round`.

### [ ] Stage M — Processes, credentials, and sessions

Future programmers create a process, exit it, and wait for it. A user id is a number the file and device checks will trust.

- [ ] **Task M1, Robix feature.** Add `Proc.Create`, `Proc.Exit`, `Proc.Wait`, and `Cred.Allow(user, right)`. The compiler test creates a child, waits for its status, and denies a guest the `SystemWrite` right while allowing the root user.

- [ ] **Task M2.** `Kernel/POSIX/RoboProcess.rx` and `Kernel/POSIX/RoboCredential.rx` use those calls. Each process has one address space from Stage K, one credential, and a thread the Stage J scheduler runs. `RoboLaunch` will be process 1. The splash painter is not a process. It is kernel code that runs until process 1 starts.

| File | Test |
| :--- | :--- |
| `Kernel/POSIX/RoboProcess.rx` | `RoboProcess.test.rx` covers create, exit, wait, and a second child that is still alive |
| `Kernel/POSIX/RoboCredential.rx` | `RoboCredential.test.rx` covers root, a normal user, and guest |

- [ ] **Exit.** Serial contains `ROBO_PROC: child` and `ROBO_PROC: wait`. Splash ceiling 98600.

### [ ] Stage N — The syscall gate

Future programmers add a kernel service by adding one row. User code cannot call `Port`, `Frame`, or `CPU` directly.

- [ ] **Task N1, Robix feature.** Add `Syscall.Dispatch(table, number, arg) -> Result`. The compiler test asserts echo returns `arg + 0x1337`, an unknown number is `Error`, and `Draw` is `Error` unless the caller is the window-server id.

- [ ] **Task N2.** `Kernel/Syscall/RoboSyscall.rx` installs the table in section 3.2. The trap frame for the syscall vector saves the user registers and returns to the same thread. A bad pointer argument faults the caller through Stage K and the kernel continues.

| File | Test |
| :--- | :--- |
| `Kernel/Syscall/RoboSyscall.rx` | `RoboSyscall.test.rx` covers echo, reject, a bad pointer, and `Draw` denied to a normal process |

- [ ] **Exit.** A user thread prints `ROBO_SYSCALL: echo` and serial contains `ROBO_SYSCALL: verified syscall round-trip dispatch`. Splash ceiling 99400.

### [ ] Stage O — A real file system

Future programmers look up, create, read, and list names in a test. The disk driver stores that same table in sectors.

- [ ] **Task O1, Robix feature.** Add `FS.Lookup`, `FS.Create`, `FS.Read`, `FS.Write`, and `FS.List` over an in-memory volume. The compiler test creates `Home/Note.txt`, reads it back, and gets `Error` for a missing path.

- [ ] **Task O2.** `Kernel/FileSystem/RoboVFS.rx` is that table behind syscall `File`. `Kernel/Devices/RoboDisk.rx` reads sector 0 the way `Examples/Machine/Disk.rx` does, then reads and writes later sectors through the built-in storage driver once Stage Q binds it. Until that driver is bound, the boot volume is the packed image the bootloader already handed over, so the kernel can open `System/Launch/RoboLaunch.rxb`.

- [ ] **Task O3.** After a reboot the note is still there. A full disk returns `Error` to the caller. The kernel keeps running.

| File | Test |
| :--- | :--- |
| `Kernel/FileSystem/RoboVFS.rx` | `RoboVFS.test.rx` covers create, read, missing, and full |
| `Kernel/Devices/RoboDisk.rx` | `RoboDisk.test.rx` asserts the sector-0 command sequence |

- [ ] **Exit.** Serial contains `ROBO_DISK: sector`, `ROBO_FS: mount`, and, after reboot, `ROBO_FS: Note.txt`. Splash ceiling 99200.

---

## 8. Drivers

### [ ] Stage P — Buses and input devices

Future programmers decode PCI and HID from fixture bytes. The image uses `Port.Read32` for config space.

- [ ] **Task P1, Robix feature.** Add `Port.Read32` and `Port.Write32`. The interpreter stops with the freestanding diagnostic. Add `PCI.Decode(words)` and `HID.Parse(bytes)` as pure functions. Their compiler tests use a fixed config word and a 3-byte boot mouse report.

- [ ] **Task P2.** Port the RefCode hosts into `RoboPCI.rx`, `RoboHID.rx`, `RoboVirtioMouse.rx`, `RoboVMMouse.rx`, `RoboI2CTouchpad.rx`, and `RoboXHCI.rx`. Absent hardware is skipped. Events go to the pointer queue, and into the `Window` port once `RoboWindowServer` is alive.

| File | Test |
| :--- | :--- |
| `Kernel/PCI/RoboPCI.rx` | `RoboPCI.test.rx` decodes vendor, device, and class |
| `Kernel/Input/RoboHID.rx` | `RoboHID.test.rx` parses mouse, wheel, and a touch contact |
| `Kernel/Input/RoboVirtioMouse.rx` | `RoboVirtioMouse.test.rx` applies one absolute event |
| `Kernel/Input/RoboVMMouse.rx` | `RoboVMMouse.test.rx` applies one absolute event |
| `Kernel/Input/RoboI2CTouchpad.rx` | `RoboI2CTouchpad.test.rx` asserts ELAN skips the second wake |
| `Kernel/Input/RoboXHCI.rx` | `RoboXHCI.test.rx` classifies HID `0x03` and hub `0x09` |

- [ ] **Exit.** Serial contains `ROBO_PCI: devices` and `ROBO_INPUT: ready`. Splash ceilings 98800 and 99000.

### [ ] Stage Q — A driver registry, then a built-in driver for every class

Future programmers add a device class by adding one built-in driver and one row. The registry matches the way a real desktop OS matches drivers: a published device, a personality, and one bound driver.

- [ ] **Task Q1, Robix feature.** Add `Driver.Match(device, personalities) -> Result` and `Driver.Bind(device, builtIn, installed) -> Result`. An installed package whose vendor and device id match wins. Otherwise the built-in driver for that class wins. Two built-in drivers for one class fail the test. No match returns `Error`. The compiler test binds an Intel Ethernet id to a fake installed package, then binds the same id to `RoboBasicNet` after the package is removed.

- [ ] **Task Q2.** `Kernel/IO/RoboRegistry.rx` publishes every device Stage P found. `Kernel/IO/RoboDriver.rx` binds them before the first user process starts. These built-in files ship in the image:

| Class | File | Path the built-in driver uses |
| :--- | :--- | :--- |
| Display | `Kernel/IO/Basic/RoboBasicDisplay.rx` | The linear framebuffer from the handoff. Scanout when a BAR is a framebuffer |
| Audio | `Kernel/IO/Basic/RoboBasicAudio.rx` | HDA CORB/RIRB and a buffer list |
| Ethernet | `Kernel/IO/Basic/RoboBasicNet.rx` | RX and TX descriptor rings |
| USB host | `Kernel/IO/Basic/RoboBasicUSB.rx` | xHCI rings |
| USB storage | `Kernel/IO/Basic/RoboBasicUSBStore.rx` | Bulk queues |
| Storage | `Kernel/IO/Basic/RoboBasicStorage.rx` | NCQ, NVMe queues, ATA DMA, SD ADMA2. ATA PIO only when DMA is absent |
| Camera | `Kernel/IO/Basic/RoboBasicCamera.rx` | UVC isochronous |
| Bluetooth | `Kernel/IO/Basic/RoboBasicBluetooth.rx` | HCI over USB |
| Power | `Kernel/IO/Basic/RoboBasicPower.rx` | ACPI battery and sleep |
| Printer | `Kernel/IO/Basic/RoboBasicPrint.rx` | USB bulk |
| Serial | `Kernel/IO/Basic/RoboBasicSerial.rx` | 16550 FIFO |
| SMBus | `Kernel/IO/Basic/RoboBasicSMBus.rx` | Block transfer |
| VirtIO | `Kernel/IO/Basic/RoboBasicVirtio.rx` | Virtqueues for QEMU net, block, and input |

Wi-Fi and a vendor GPU have no invented class protocol. They stay published and unmatched until a package is installed. The display keeps working through `RoboBasicDisplay`.

| File | Test |
| :--- | :--- |
| `Kernel/IO/RoboDriver.rx` | `RoboDriver.test.rx` covers an installed winner, the built-in fallback, a duplicate class, and no match |
| Each `RoboBasic*.rx` | A sibling `.test.rx` asserts the queue or DMA path for a fixture that advertises it, and PIO only for a fixture that does not |

- [ ] **Exit.** Serial contains `ROBO_DRIVER: basic` and one line per bound class. The keyboard still reaches the desktop.

### [ ] Stage R — Vendor packages, Intel first

Future programmers install, replace, and remove a driver package with one call. Intel is the first catalog. Later vendors use the same call and a new catalog. Device Manager, the window that does this, is the Stage W app. This stage is the kernel side: the package format, the catalog match, and the rebind.

- [ ] **Task R1, Robix feature.** Add `Driver.Package(bytes) -> Result`, `Driver.Newer(current, offered)`, and `Vendor.Match(catalog, vendor, device, class)`. The compiler test accepts a verified header, rejects a bad HMAC, replaces an older version, and keeps the previous package for rollback. Vendor `0x8086` and the LPSS id `0x9D60` match the Intel catalog. Vendor `0x1022` does not.

- [ ] **Task R2.** `Kernel/IO/Intel/RoboIntelCatalog.rx` is the Intel id table. Each family is a package. The built-in driver stays bound until the package is installed. Remove rebinds the built-in driver.

| Package | File | What it adds past the built-in driver |
| :--- | :--- | :--- |
| Graphics | `RoboIntelGraphics.rx` | Intel display ids, planes, and power states |
| Audio | `RoboIntelAudio.rx` | SST, SOF, and digital microphones |
| Ethernet | `RoboIntelEthernet.rx` | e1000e, igc, igb, ixgbe, and i40e ids, checksum offload, larger rings |
| Wi-Fi | `RoboIntelWiFi.rx` | The families that can run the device, including AX200 and AX201 |
| Bluetooth | `RoboIntelBluetooth.rx` | Intel USB and CNVi Bluetooth |
| LPSS | `RoboIntelLPSS.rx` | The `0x8086` I2C ids and the clock rules from `RoboInputRules.hpp` |
| USB | `RoboIntelUSB.rx` | Intel xHCI quirks, USB4, and Thunderbolt |
| Storage | `RoboIntelStorage.rx` | VMD and RST on top of the built-in AHCI and NVMe queues |
| Camera | `RoboIntelCamera.rx` | Intel IPU when the camera is not UVC |
| Chipset | `RoboIntelChipset.rx` | PCH GPIO, the thermal sensor, and the Intel SMBus |

- [ ] **Task R3.** The same milestone, one stage record each, for the catalogs that follow. Each one is a folder under `Kernel/IO/Vendors/<Name>/`, a compiler test that matches its vendor id and rejects `0x8086`, and an install that rebinds through `Driver.Bind`.

| Vendor | Id | Families |
| :--- | :--- | :--- |
| AMD | `0x1022` chipset, `0x1002` graphics | Chipset, graphics, Ethernet |
| NVIDIA | `0x10DE` | Graphics. `RoboBasicDisplay` returns on remove |
| Realtek | `0x10EC` | Audio and Ethernet |
| Broadcom | `0x14E4` | Wi-Fi and Ethernet |
| Qualcomm | `0x17CB` | Wi-Fi |

A later vendor is `Vendor.Catalog(name, vendorId)`, a new folder, and the same install and remove proof.

| File | Test |
| :--- | :--- |
| `Kernel/IO/Intel/RoboIntelCatalog.rx` | `RoboIntelCatalog.test.rx` covers `0x8086` / `0x9D60`, an unknown Intel id, and vendor `0x1022` |
| Each package file | A sibling `.test.rx` asserts its id, a foreign vendor, and the path the fixture selects |
| Each later catalog | The same three asserts for that vendor |

- [ ] **Exit.** A QEMU device with vendor `0x8086` and id `0x9D60` binds the built-in driver, then `RoboIntelLPSS` after install, then the built-in driver after remove. Serial contains `ROBO_DRIVER: intel` and then `ROBO_DRIVER: basic`. Later vendors do not start before this Intel boot is recorded.

### [ ] Stage AG — A GPU command buffer

This is the graphics stage. It is a command buffer the window server submits to the bound display driver. It is not a kernel loop that splits rows across cores.

- [ ] **Task AG1, Robix feature.** Add `GPU.Command(op, rect, color)` and `GPU.Submit(buffer) -> Result`. The compiler test builds a fill and a blit, submits them, and `AssertPixel` reads the result from a software queue. A command from a process other than the window server is `Error`.

- [ ] **Task AG2.** `RoboBasicDisplay` executes the buffer on the CPU when the device has only a linear framebuffer, using one slice per online core from the Stage J scheduler, with each row in exactly one slice. `RoboIntelGraphics` and `RoboNVIDIAGraphics`, after Device Manager installs them, execute the same buffer on their command hardware. The window server’s present call stays `GPU.Submit`.

| File | Test |
| :--- | :--- |
| `User/RoboWindowServer/RoboGPUQueue.rx` | `RoboGPUQueue.test.rx` covers fill, blit, a foreign process, and a two-core slice that covers every row once |

- [ ] **Exit.** The desktop is still the window server’s desktop. Serial contains `ROBO_GPU: submit`. With the built-in display driver, a test buffer’s origin pixel matches the command color. With an installed Intel or NVIDIA package, serial also contains `ROBO_GPU: device`.


---

## 9. Desktop

### [ ] Stage S — launchd, then the window server

Future programmers start process 1 from a file on the volume, and give one process the right to present. That process is the window server. This is the stage where the product desktop begins.

- [ ] **Task S1, Robix feature.** Add `Launch.Plan(services) -> Result` which orders services by their `after` list and rejects a cycle. The compiler test starts `Window` after `Login` is listed as waiting on `Window`, and a cycle returns `Error`.

- [ ] **Task S2.** `User/RoboLaunch/RoboLaunch.rx` is process 1. The kernel loads it from `System/Launch/RoboLaunch.rxb` with the Stage M process call and the Stage O file call. It opens the `Launch` port and starts the services in the plan. The first service is `RoboWindowServer`.

- [ ] **Task S3.** `User/RoboWindowServer/RoboWindowServer.rx` maps the framebuffer through syscall `Map`, takes the `Window` port, and is the only process whose `Draw` syscall succeeds. It clears the desktop to `#080A10` in the dark theme. The light theme uses `#F5F5F7` for the desktop and `#0E7AFF` for the menu bar. The server reads the theme from a setting once Stage V exists. Until then the dark theme is the boot theme. The kernel stops painting when the server sends `ready` on the `Launch` port.

Input from Stage P is delivered to the window under the cursor. The server composites back to front and presents the damaged rectangles. A client that has not been granted a window receives `Error`.

| File | Test |
| :--- | :--- |
| `User/RoboLaunch/RoboLaunch.rx` | `RoboLaunch.test.rx` covers order, a cycle, and a missing file |
| `User/RoboWindowServer/RoboWindowServer.rx` | `RoboWindowServer.test.rx` covers a client command, a denied `Draw` from another id, damage, and both theme colors |

- [ ] **Exit.** Serial contains `ROBO_LAUNCH: pid 1`, `ROBO_WINDOW: ready`, and `ROBO_KERNEL: console yield`. The screen is the server’s desktop, full size for the mode the handoff selected, with the menu bar color of the active theme. Splash ceiling 99600.

### [ ] Stage T — Login and the session

Future programmers check a password and a path right in a test. The login window is a client of the window server.

- [ ] **Task T1, Robix feature.** Add `Auth.Check(hash, password) -> Result` and `ACL.Allow(user, path, write) -> Bool`. The compiler test accepts a matching pair, rejects a wrong password, lets user 0 write `System/`, and denies that write for any other user.

- [ ] **Task T2.** `User/RoboLogin/RoboLogin.rx` and `Desktop/RoboLogin.rxu` are drawn by sending commands to `RoboWindowServer`. A successful login starts the session services: Finder, Dock, and the menu bar extras. Lock puts the login window back and keeps each user’s processes in their own address space. Guest is a user id that `ACL.Allow` denies for every write outside `Home/Guest`.

| File | Test |
| :--- | :--- |
| `User/RoboLogin/RoboLogin.rx` | `RoboLogin.test.rx` covers match, mismatch, lock, and guest |

- [ ] **Exit.** The login window is the first window on the server’s desktop. The password path reaches an empty session. A denied write prints `ROBO_FS: deny` while `ROBO_PROCESS: alive` follows.

### [ ] Stage U — Finder, Dock, menu bar, Spaces

Future programmers lay out a stack and a grid in a test. Those layouts are the Dock, Launchpad, notifications, and Control Center. They are clients. They are not kernel fills.

- [ ] **Task U1, Robix feature.** Add `UI.Stack(items, origin, gap)` and `UI.Grid(items, columns, cell)`. The compiler test stacks three notifications and asserts each y, then places eight Launchpad icons and asserts the grid.

- [ ] **Task U2.** `User/RoboDock/RoboDock.rx`, `User/RoboFinder/RoboFinder.rx`, and their `.rxu` documents. Spaces are window lists the window server shows or hides. Snap, quarters, and maximize are commands the server applies to a window id. The menu bar holds the clock from `Clock.Seconds` and the Roboticela menu.

Light and dark are both real themes on every window. The documents use the colors the server publishes. Layout is written for a narrow window first and grows to the display width.

| File | Test |
| :--- | :--- |
| `User/RoboDock/RoboDock.rx` | `RoboDock.test.rx` asserts the grid, the stack, and dismiss |
| `User/RoboFinder/RoboFinder.rx` | `RoboFinder.test.rx` asserts the focused space and a window id |

- [ ] **Exit.** The menu bar, the Dock, a Finder window, a notification, Control Center, and a second Space are visible. Serial contains `ROBO_SESSION: spaces`. Dragging a window sends one command and the server moves that window.

### [ ] Stage V — Settings and search

- [ ] **Task V1, Robix feature.** Add `Settings.Get`, `Settings.Set`, `Settings.Reset`, and `Search.Query(index, text)`. The compiler test sets a display width, reset returns the default, and a two-letter prefix returns three app names in order.

- [ ] **Task V2.** `User/RoboSettings` stores keys in the current user’s directory. Panes: display, sound, network, Bluetooth, users, privacy, language, date, power, updates, and appearance. Appearance writes the theme the window server already understands. `User/RoboSearch` indexes Finder, Settings, Files, and file names from `RoboVFS`.

- [ ] **Exit.** Settings opens from the Dock and the display pane shows the handoff width and height. Search lists the Finder and `Home/Note.txt`. Serial contains `ROBO_SETTINGS: display` and `ROBO_SEARCH: hit`.

### [ ] Stage W — Files and Device Manager

- [ ] **Task W1, Robix feature.** Add `FS.Move`, `FS.Trash`, and `FS.Restore`. The compiler test moves a file, trashes it, lists the trash, and restores it.

- [ ] **Task W2.** `User/RoboFiles` is the Finder’s folder view: sidebar, list, preview, rename, duplicate, tags. `User/RoboDevices` lists every registry entry: name, class, vendor, device id, the bound driver, and whether it is built in or installed. Install, update, and remove call `Driver.Package` from Stage R. After Stage Y has a network, the same window can download a package and then calls the same install path.

- [ ] **Exit.** A trashed file comes back. Installing the Intel LPSS package from the volume prints `ROBO_DRIVER: intel`, and removing it prints `ROBO_DRIVER: basic`. A bad package prints `ROBO_DRIVER: reject` and leaves the built-in driver bound.

### [ ] Stage X — Sound, camera, Bluetooth, and power

- [ ] **Task X1, Robix feature.** Add `Audio.Mix`, `Audio.Peak`, `BT.Parse`, and `Power.Decide(percent, onAC)`. The compiler test mixes silence to silence, parses a fixed advertisement, and covers run, warn, sleep, and shutdown.

- [ ] **Task X2.** The audio server mixes into the buffer the bound audio driver plays. The camera and the microphone open through their bound drivers. Privacy keys deny an open with `Error`. Bluetooth keyboards and headsets join the input queue and the mixer. Sleep saves the session and wakes onto the lock screen. Hibernate writes the image to the volume.

- [ ] **Exit.** Control Center changes the volume. Serial contains `ROBO_AUDIO: mix`. A denied camera open prints `ROBO_CAMERA: deny`. Sleep then wake shows the lock screen and serial contains `ROBO_POWER: wake`.

### [ ] Stage Y — Network

- [ ] **Task Y1, Robix feature.** Add `Net.Checksum(bytes)` and `ICMP.Build(id, seq, payload)`. The compiler test checks a known header and the echo layout.

- [ ] **Task Y2.** The network service brings up the bound Ethernet driver, then DHCP, ARP, ICMP, UDP, TCP, and DNS. On QEMU the driver is `RoboBasicVirtio`. On Intel Ethernet it is `RoboBasicNet` until Device Manager installs `RoboIntelEthernet`. Wi-Fi runs when an installed vendor package is bound. The firewall is an allow table. A VPN is one extra interface that owns a route.

- [ ] **Exit.** QEMU user-net answers the echo. Serial contains `ROBO_NET: echo`. A blocked port prints `ROBO_NET: deny`.

### [ ] Stage Z — Displays, clipboard, fonts, and printing

- [ ] **Task Z1, Robix feature.** Add `Display.Scale`, `Display.Arrange`, `Clip.Set`, `Clip.Get`, `Font.Measure`, `Focus.Next`, and `Print.Paginate`. The compiler test scales 84 by 200 percent to 168, places a second display to the right, measures a known string, and splits a tall frame into two pages.

- [ ] **Task Z2.** The window server presents through the bound display driver and uses `Display.Scale` before layout. Night shift is a tint on present. Clipboard and drag are services. Screenshot writes a frame into `Home/Pictures`. Printing sends pages through `RoboBasicPrint` or to a PDF on the volume. Full keyboard reach, a larger scale, and a high-contrast theme are Settings keys the window server honors.

- [ ] **Exit.** Tab reaches every control on the login window. A copy in Files pastes into Finder. Serial contains `ROBO_CLIP: paste` and `ROBO_PRINT: pages`.

---

## 10. The image

### [ ] Stage AI — The release image

- [ ] **Task AI1, Robix feature.** Add `robix test RoboticelaOS`, which runs every test under this tree on the interpreter and the native compiler, and add `OS.Checklist(lines)` which asserts the required serial lines are present in order. The compiler test feeds a full log and a log with one line removed.

- [ ] **Task AI2.** `robixc RoboticelaOS/RoboticelaOS.rxb -O3 -o Build/RoboticelaOS/RoboticelaOS.img` builds the ROM, bootloader, kernel, launch service, window server, and the boot volume.

- [ ] **Exit.** `robix test RoboticelaOS` passes. From power-on the serial file contains, in order:

```text
ROBO_BOOT: chain ok
ROBO_BOOT: handoff
ROBO_KERNEL: start
ROBO_MEMORY: pages
ROBO_IDT: gates
ROBO_CPU: cores online
ROBO_SCHED: core 0
ROBO_FS: mount
ROBO_DRIVER: basic
ROBO_LAUNCH: pid 1
ROBO_WINDOW: ready
ROBO_KERNEL: console yield
```

The screen shows the splash, then the login window, then the session: menu bar, Dock, desktop, and Finder. That boot is RoboticelaOS.

---

## 11. Order

```text
Bootloader: F handoff -> AA install and recovery -> AF secure boot
Kernel: G pages -> H interrupts -> I cores -> J scheduler -> K address spaces -> L ports -> M processes -> N syscalls -> O files
Drivers: P buses -> Q built-in drivers -> R Intel, then other vendors -> AG display command buffer
Desktop: A frame -> B cooked window -> C pointer -> E layout -> S window server -> T login -> U Finder, Dock, menu bar -> V settings -> W files and Device Manager -> X sound and power -> Y network -> Z displays
```

The image is those four parts. Stage AI is the boot that runs them together: splash, login, menu bar, Dock, and Finder.
