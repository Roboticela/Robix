# RoboticelaOS gaps

Check an item only after it runs on hardware or in QEMU and its tests cover the cases in that file.

Already in the tree, so they are not listed again: boot handoff, serial log, identity page map, frame claim, address-space split, exception gates 0–31, AP trampoline, a pick/migrate scheduler proof, port round-trip, process create/exit/wait proof, credential allow/deny, syscall echo, two VFS paths, ATA PIO of one sector, PCI config read on bus 0, PS/2 poll, HID report parse, virtio and VM mouse helpers, I2C touchpad helper, xHCI class numbers, CMOS seconds, splash, and software framebuffer paint.

The files under `Kernel/IO/Basic` return a driver id. They do not program the device. `Choose` also skips a basic display for Intel, NVIDIA, and AMD, and it skips bridges, input controllers, wireless, and most serial-bus subclasses. The Intel, NVIDIA, AMD, Realtek, Broadcom, and Qualcomm packages also only match an id. Their device work is listed under Vendor drivers.

Whatever you implement, first make sure that the OS has that much easy features that it can make its implementation simple, if not then implement those features in the OS.

## Kernel features

### Interrupts and time

- [x] IRQ gates above vector 31, including the timer vector, with a return path that resumes the interrupted thread
- [x] Separate handlers for page fault, general protection, double fault, and invalid opcode, each reporting the fault address instead of halting every trap the same way
- [x] Legacy INTx delivery through the I/O APIC, with a mask, an acknowledge, and a shared-line handler list
- [x] MSI and MSI-X allocation, message write, and teardown
- [x] Spurious IRQ and NMI handling that does not kill the machine
- [x] Local APIC timer as the scheduler tick on every core
- [x] TSC calibration against a known timer, plus a monotonic clock and a wall clock
- [x] One-shot kernel timers and a sleep that wakes on the next tick or sooner
- [x] HPET when the firmware publishes it, used when the APIC timer is unavailable

### Memory

- [x] Kernel heap with allocate, free, and alignment, backed by the frame allocator
- [x] Page-fault handler that maps an anonymous page and returns to the faulting instruction
- [x] Per-process address space loaded on every context switch
- [x] Copy-on-write pages for a forked address space
- [x] `mmap` and `munmap` for anonymous memory, with guard gaps the fault handler refuses to fill
- [x] Shared memory object that two processes can map
- [x] Physical-page pin and unpin for drivers that program DMA
- [x] IOMMU (or a bounce buffer when no IOMMU is present) so a device cannot write arbitrary RAM
- [x] Firmware framebuffer and ACPI regions reserved so the frame allocator never hands them out
- [x] Kernel stack guard page per thread

### CPU and scheduling

- [x] Full register save and restore on switch, including the FPU and SSE state, switched lazily
- [x] Per-CPU data: current thread, kernel stack, and IRQ nesting count
- [x] Wait queues: a thread sleeps on an object and one wake or wake-all resumes it
- [x] Blocking locks that sleep, and spin locks that are IRQ-safe
- [x] Priority levels and a preemption point from the timer tick
- [x] Idle loop per core that halts until the next interrupt
- [x] Deferred work that runs after interrupt return, for driver bottom halves
- [x] CPU bring-up that leaves application processors in the idle loop instead of a one-shot proof

### Processes

- [x] Stable pid allocation, parent links, and zombie reaping beyond the single proof child
- [x] `exec` of a Robo executable from the VFS into a fresh user address space
- [x] Threads that share an address space and have private stacks
- [x] Signals that a process can block, catch, or default, delivered on return to user mode
- [x] Process groups and sessions so a shell can own a foreground job
- [x] File-descriptor table per process: dup, close-on-exec, and a shared open-file object
- [x] User and kernel copies that reject pointers aimed at kernel memory
- [x] Credential checks on open, kill, and device ioctl, using the existing allow table

### Syscalls and IPC

- [x] Syscall table with a number, an argument check, and a return, replacing the single echo path
- [x] `read`, `write`, `open`, `close`, `lseek`, `ioctl`, `mmap`, `brk`
- [x] `fork`, `exec`, `exit`, `wait`, `getpid`, `kill`
- [x] `pipe` and a poll or select that sleeps until a descriptor is ready
- [x] Message ports that block the sender or receiver instead of only completing a boot round-trip
- [x] Futex-style wait and wake on a user word so user locks do not spin

### Files

- [x] Path lookup with directories, `.`, `..`, and a mount table
- [x] Directory create, remove, rename, and read
- [x] Permission bits checked against the caller credential
- [x] Partition scan for MBR and GPT, then a mount of the chosen volume
- [x] Robo volume format with files larger than one sector and a free-block map
- [x] FAT32 read and write so USB sticks and firmware disks work before a vendor storage stack
- [x] Device filesystem where each bound driver appears as a node
- [x] Process and kernel status filesystem for live inspection

### Device model

- [x] Bus scan past bus 0, walking PCI-to-PCI bridges and secondary buses
- [x] BAR sizing, allocation, and MMIO or port mapping for every function
- [x] Driver object with probe, start, stop, and interrupt hooks, not only an integer id
- [x] Character-device operations: open, read, write, ioctl
- [x] Block-device operations: read sectors, write sectors, flush, and a request queue
- [x] Hotplug and unplug that stop the driver and release the BARs and IRQs
- [x] Package load of a later vendor driver that replaces the basic driver without a reboot
- [x] Kernel log with severity, readable from user space, not only port 1016 bytes

### Platform

- [ ] ACPI table walk: RSDP, RSDT or XSDT, FADT, MADT, MCFG, and DSDT
- [x] AML interpreter enough to evaluate `_STA`, `_HID`, `_CRS`, `_PRT`, and power methods
- [x] PCI interrupt routing from `_PRT` when the MADT has no IOAPIC override
- [x] Machine shutdown and reboot through the ACPI reset register or keyboard controller
- [x] Suspend to RAM and resume, restoring BARs, IRQs, and the timer
- [x] EC and lid, sleep-button, and power-button events delivered to user space

### Network stack

- [ ] Ethernet frame alloc, parse, and a transmit queue a NIC driver can fill
- [ ] ARP, IPv4, ICMP echo, UDP, and TCP with a socket syscall
- [ ] DHCP client so a link comes up without a stored address
- [ ] DNS resolver used by user programs
- [ ] Packet filter hook with a default allow, so a later firewall can attach

## Basic drivers

These are the class drivers that should make a device usable before any vendor package is installed. Vendor catalogs stay optional and install on top.

### Platform and buses

- [x] PCI host bridge and PCI-to-PCI bridge, so devices behind a bridge are enumerated and routed
- [x] LPC or ISA bridge, so the legacy keyboard, RTC, timer, and serial ports stay reachable
- [ ] ACPI embedded controller
- [x] Generic DMA controller for ISA-style devices that still need it
- [x] SD and eMMC host (SDHCI) so laptop cards and onboard flash work without a vendor host driver
- [ ] I2C and SPI controller class, enough to reach HID-over-I2C touchpads and sensor chips
- [x] SMBus class that completes the existing id with real block read and write
- [x] Virtio transport that programs notification, queues, and MSI, covering net, block, gpu, input, and console

### Display

- [ ] Firmware framebuffer driver (GOP or VESA) that binds every display, including Intel, NVIDIA, and AMD, and sets a mode from the boot handoff
- [ ] Software cursor plane on that framebuffer
- [ ] Modeset to the panel’s native size when the firmware reports more than one mode
- [ ] Backlight through ACPI `_BCL` / `_BCM` so a laptop panel is visible

### Input

- [ ] PS/2 keyboard with scan-code set 1 and 2, key up, key down, and lock LEDs
- [ ] PS/2 mouse with packet sync, buttons, and wheel
- [ ] PCI input-controller class (class 9), which `Choose` currently does not bind
- [ ] HID keyboard, mouse, and touch collections turned into the existing input queue
- [ ] HID-over-I2C touchpad that reports absolute contact from the real descriptor, not only a parsed sample
- [ ] Power, sleep, and lid buttons as input events

### USB

- [ ] xHCI host that runs command, event, and transfer rings and recovers a halted endpoint
- [ ] EHCI and UHCI or OHCI fallback for machines without xHCI
- [ ] USB hub driver, including transaction translator, so a device behind a hub enumerates
- [ ] USB HID boot keyboard and boot mouse, plus full report devices
- [ ] USB mass-storage bulk-only transport, with a block device on top
- [ ] USB audio class, playback and capture at one standard rate
- [ ] USB video class, one uncompressed frame format
- [ ] USB printer class, bulk out
- [ ] USB CDC-ACM serial
- [ ] USB CDC-ECM, NCM, and RNDIS, presented as an Ethernet device
- [ ] USB Bluetooth HCI transport that attaches to the Bluetooth class driver
- [ ] USB serial-bus class binding for every host controller subclass, not only subclass 3

### Storage

- [ ] AHCI that identifies a disk, reads and writes by LBA, and flushes the cache
- [ ] NVMe admin and I/O queues, identify, read, write, and flush
- [ ] ATA and ATAPI through the existing PIO path, extended from one sector to any LBA
- [ ] Generic disk object and partition reader used by AHCI, NVMe, USB storage, virtio-blk, and SD
- [ ] Optical ATAPI read so an installer disc works before a vendor chipset driver

### Audio

- [ ] Intel HD Audio bus: reset the codec, read the widget graph, open one playback stream and one capture stream
- [ ] Speaker and headphone mute and volume through the codec, before any Realtek or vendor package
- [ ] HDMI or DisplayPort audio endpoint when the display device exposes it

### Network

- [ ] Virtio network device on the generic virtio transport
- [ ] USB Ethernet class devices listed under USB above
- [ ] Loopback interface that the socket layer can use with no NIC present
- [ ] Link-state reporting so user space sees cable and speed

There is no single PCI Ethernet class driver that can run every vendor chip. Unknown PCI NICs stay unbound until their package is installed. USB Ethernet, virtio-net, and loopback are the paths that work before that.

### Wireless and Bluetooth

- [ ] Bluetooth HCI core: reset, read BD address, inquiry, connect, and ACL data
- [ ] HID-over-GATT and classic HID so a paired keyboard and mouse work before a vendor radio package
- [ ] Wireless LAN class binding (PCI class 13), which `Choose` currently does not bind
- [ ] A minimum 802.11 path only where the device speaks a class interface; vendor radios stay on their own packages

### Power, sensors, and laptop parts

- [ ] AC adapter and battery through ACPI, reporting present, charging, and percent
- [ ] Thermal zone read and a passive trip that notifies user space
- [ ] Lid switch
- [ ] Ambient light sensor on the I2C or ACPI path, enough for one backlight policy
- [ ] CMOS RTC read and set, separate from the one-second boot sample

### Ports and other class devices

- [ ] 16550 serial on the standard ports, with interrupt-driven read and write
- [ ] Parallel or USB printer already covered above, plus a class bind for PCI class 7 simple-communication devices other than serial
- [ ] Camera class for PCI multimedia video, distinct from the USB video class
- [ ] Generic printer class for PCI class 7 subclass 1 when it appears

## Vendor drivers

These install on top of the basic driver for that device. Removing the package must leave the basic driver bound. Each file below only returns a match today.

### Intel

- [ ] `RoboIntelGraphics`: modeset, stolen memory, and a scanout plane for the integrated GPU
- [ ] `RoboIntelAudio`: HDMI and DisplayPort codec endpoints on the Intel display audio function
- [ ] `RoboIntelNet`: Intel Ethernet link, receive, and transmit
- [ ] `RoboIntelWiFi`: Intel wireless scan, associate, and data for the matched device ids
- [ ] `RoboIntelBluetooth`: Intel Bluetooth HCI for the matched device ids
- [ ] `RoboIntelUSB`: Intel xHCI quirks for the matched host controllers, on top of the basic xHCI driver
- [ ] `RoboIntelStorage`: Intel AHCI and RAID-passthrough disks for the matched device ids, on top of the basic AHCI driver
- [ ] `RoboIntelLPSS`: Intel I2C, SPI, and UART hidden behind the LPSS function
- [ ] `RoboIntelCamera`: Intel MIPI camera bridge and one captured frame
- [ ] `RoboIntelChipset`: Intel host, ISA, and SMBus bridges for the matched device ids, including GPIO used by laptop buttons

### NVIDIA

- [ ] `RoboNVIDIAGraphics`: modeset and a framebuffer on the NVIDIA GPU that `Choose` currently leaves unbound
- [ ] NVIDIA HDMI and DisplayPort audio endpoint on that GPU

### AMD

- [ ] AMD graphics: modeset and a framebuffer on the AMD GPU that `Choose` currently leaves unbound
- [ ] AMD chipset: host bridge and IOMMU setup for the class-6 match the catalog already claims
- [ ] AMD audio: HDMI and DisplayPort codec on the AMD display audio function
- [ ] AMD storage: chipset AHCI or NVMe quirks where the basic storage driver is not enough

### Realtek

- [ ] `RoboRealtek` Ethernet: link, receive, and transmit for the class-2 match the catalog already claims
- [ ] Realtek HD Audio codec: speaker, headphone, and microphone widgets behind the basic HD Audio bus
- [ ] Realtek card reader, when the PCI function is a separate storage device

### Broadcom

- [ ] `RoboBroadcom` network: link, receive, and transmit for the class-2 match the catalog already claims
- [ ] Broadcom wireless: scan and associate when the function is a wireless LAN device rather than Ethernet

### Qualcomm

- [ ] `RoboQualcomm` network: link, receive, and transmit for the class-2 match the catalog already claims
- [ ] Qualcomm wireless: scan and associate when the function is a wireless LAN device rather than Ethernet
- [ ] Qualcomm Atheros Bluetooth, when the same module exposes an HCI function

## System gaps

`Start` paints the splash, desktop, login, settings, and search, then halts. Launch, login, and the settings panes are id tables. Text is a 5-row digit. These items are the rest of the OS around that boot path.

### Boot and lifetime

- [ ] A boot menu with the normal system, a recovery shell, and the last known good package set
- [ ] Kernel command line and an init ramdisk so the root volume can be mounted after the basic storage driver starts
- [ ] PID 1 that stays running, starts the session, and restarts a service that exits
- [ ] Panic screen with the fault address and a stack, instead of a silent halt
- [ ] Watchdog that reboots the machine when the kernel stops acknowledging it
- [ ] Crash dump written to the swap or a reserved partition, readable from recovery

### Memory and multiprocessor holes

- [ ] Inter-processor interrupt so one core can preempt or shoot down another
- [ ] TLB shootdown when a mapping changes on a core that did not fault
- [ ] Write-combining page type for the framebuffer so present does not crawl through uncached stores
- [ ] File-backed `mmap`, distinct from the anonymous mapping already listed
- [ ] Swap file that the page fault handler can read back
- [ ] Journal on the Robo volume so a crash does not lose the free-block map
- [ ] TRIM for SSDs after a file is deleted
- [ ] TTY and a pseudo-terminal pair so a terminal application has a keyboard and a scrollback

### Security

- [ ] NX on user and kernel pages, with SMEP and SMAP when the CPU reports them
- [ ] Address-space layout randomization for user processes and the kernel image
- [ ] Password store that is not the current XOR, with a per-user salt and a verify that runs in constant time
- [ ] Signed boot: the bootloader checks the kernel, and the kernel checks a driver or app package before it replaces the basic driver
- [ ] Home-volume encryption unlocked by the login password
- [ ] App sandbox with a file, device, and network grant list
- [ ] Audit log of login, package install, and failed credential checks

### Session and desktop

- [ ] Window server as its own process: z-order, key focus, damage, and a present that waits for vertical blank
- [ ] Hardware cursor when the display driver exposes one, with the software cursor as the fallback
- [ ] More than one monitor, each with its own framebuffer, origin, and scale
- [ ] Font files and shaped text, replacing the digit rows used by the clock
- [ ] Clipboard shared by processes, with a plain-text format
- [ ] Drag of a file from Finder onto another window
- [ ] Notification queue a process can post to
- [ ] Keyboard layout and an input method that commits characters, not only scan codes
- [ ] Global shortcuts for lock, search, and volume
- [ ] Logout, screen lock, and switch-user without rebooting

### Accounts, files, and apps

- [ ] Account create, disable, and guest, each with a home directory on the volume
- [ ] Finder that lists, opens, renames, and deletes real directory entries
- [ ] Search over file names on the mounted volume, not the four hardcoded names
- [ ] A text editor that reads and writes the note file
- [ ] A terminal that uses the pseudo-terminal and a shell
- [ ] App package install, launch by `exec`, and removal, separate from a vendor driver package
- [ ] Permission prompt the first time an app opens the camera, microphone, or location
- [ ] Software update that downloads a signed set, installs it, and rolls back if the next boot fails

### Sound, time, and network services

- [ ] Audio server that mixes clients and selects the output device the settings pane names
- [ ] Per-app and master volume, mute, and the input device for capture
- [ ] Time zone table and an NTP client so the menu clock is not the raw CMOS second
- [ ] WPA supplicant that associates with the wireless driver and stores a network
- [ ] TLS and an HTTP client used by software update
- [ ] IPv6 address configuration on the same socket layer as IPv4
- [ ] Settings panes that read and write the live display, sound, network, Bluetooth, users, privacy, language, date, power, updates, and appearance state

### Power policy

- [ ] CPU idle states and a frequency cap chosen from the power pane
- [ ] Device runtime power so an unused controller can suspend
- [ ] Lid-close action: sleep, lock, or stay awake, chosen in settings and honored by the session

## Further gaps

### Firmware, faults, and CPU state

- [ ] PCI ECAM from the MCFG table, so buses that port `0xCF8` cannot see still answer config reads
- [ ] UEFI runtime services: variables, and reset when the ACPI reset register is absent
- [ ] SMBIOS read so the session can show the product name, serial, and firmware version
- [ ] CPU microcode load before application processors start
- [ ] `CPUID` feature mask the kernel and the loader actually consult
- [ ] XSAVE and XRSTOR so AVX state survives a context switch, beyond the SSE save
- [ ] Machine-check handler that reports the bank and continues when the CPU marks the error corrected
- [ ] PCIe advanced error reporting that logs a faulty device and recovers the link
- [ ] Legacy PIC masked once the I/O APIC owns the lines
- [ ] Entropy pool seeded from the CPU random instruction, jitter, and virtio-rng, exposed as a random device
- [ ] TPM unseal of the home-volume key, with a password fallback when no TPM is present
- [ ] Out-of-memory killer that picks a user process, records it, and frees enough pages for the kernel to continue
- [ ] Priority inheritance on blocking locks so a low-priority holder cannot stall a high-priority waiter forever
- [ ] Syscall restart after a signal, so a slow `read` resumes instead of returning a false error
- [ ] DMA map and unmap that flushes the cache line before the device reads it
- [ ] Clock syscall: monotonic and wall time readable from user space

### Files, loader, and process interface

- [ ] Symbolic links and hard links, with a lookup that refuses a loop
- [ ] Named pipes, distinct from the anonymous pipe between a parent and a child
- [ ] Advisory file locks
- [ ] Directory watch that wakes a waiter when an entry is created, removed, or renamed
- [ ] `stat`, `chmod`, `chown`, and timestamp update
- [ ] `exec` that passes an argument list and an environment
- [ ] Shared libraries and a dynamic linker the `exec` path runs before `main`
- [ ] Set-uid start and supplementary groups, checked by the credential table
- [ ] Resource limits on CPU time, open files, and address-space size
- [ ] Unix-domain sockets and a service directory so a client can open the window server by name
- [ ] `ptrace` and a user-process core file the crash reporter can read
- [ ] exFAT for large removable disks and SD cards
- [ ] ISO9660 and UDF so an installer disc mounts, beyond the ATAPI sector read
- [ ] Hosts file the resolver consults before DNS
- [ ] Trash directory Finder uses instead of deleting immediately
- [ ] First-boot installer: choose a disk, write the partition table, and copy the system volume

### Devices and power the class list does not cover

- [ ] Touchscreen with more than one contact
- [ ] Pen with pressure and tilt
- [ ] Gamepad buttons and axes through HID
- [ ] Keyboard auto-repeat while a key stays down
- [ ] Fingerprint sensor that unlocks the existing login
- [ ] GNSS location fix the permission prompt can grant
- [ ] Accelerometer and lid-angle sensor
- [ ] USB-C and Thunderbolt hotplug, including a dock that brings another display
- [ ] DisplayPort multi-stream, so two panels behind one connector both light
- [ ] EDID read and display connect and disconnect while the session is running
- [ ] GPU timeout that resets the engine and redraws the desktop
- [ ] MTP so a phone or camera exposes its files without a vendor package
- [ ] USB serial adapters that are not CDC-ACM
- [ ] Virtio entropy and virtio memory balloon
- [ ] Bluetooth audio: music playback and a headset call on top of the HCI core
- [ ] Print spooler that queues a job to the basic printer driver
- [ ] Hibernate to the swap area and resume, distinct from suspend to RAM
- [ ] Wake sources: lid, keyboard, RTC alarm, and the network

### Desktop and network services

- [ ] Virtual desktops the session can switch, replacing the constant two-space table
- [ ] Screenshot of one window or the whole screen
- [ ] Night light that shifts the display color on a schedule
- [ ] Magnifier, sticky keys, and a screen reader attached to the focused control
- [ ] On-screen keyboard
- [ ] Locale and translated strings for the language pane
- [ ] mDNS so a machine can advertise and find a service on the local link
- [ ] Firewall rules attached to the packet-filter hook, editable from settings
- [ ] File sharing of one folder on the local network
- [ ] Secure shell server and client
- [ ] Scheduled tasks that survive a reboot
- [ ] Window snap and restore that the session applies to every window, not only the Finder proof

## Functional gaps

Behavior only. No windows, panes, or drawing.

### Memory, cache, and block layer

- [ ] Page cache so repeated reads of a file do not go to the device again
- [ ] Writeback of dirty pages in the background, with `fsync` forcing them out
- [ ] Dentry cache for path lookup, dropped when a directory changes
- [ ] Block request merge and an I/O scheduler in front of AHCI, NVMe, and USB storage
- [ ] `fallocate`, sparse files, and a hole that reads back as zeros
- [ ] Direct I/O that bypasses the page cache when the caller asks
- [ ] Loop device that presents a file as a block device
- [ ] Filesystem check that repairs the Robo volume journal after an unclean shutdown
- [ ] Extended attributes and access-control lists beyond the owner permission bits
- [ ] Disk quotas enforced before a write commits
- [ ] Block-layer encryption under the filesystem, plus encrypted swap
- [ ] `mlock` and `mprotect` so a process can pin pages and change them between read and execute
- [ ] Memory advice: sequential readahead and a drop of cached pages the caller will not reuse

### Scheduling and synchronization

- [ ] CPU affinity and a periodic load balance across cores
- [ ] A realtime scheduling class that runs before the normal priority levels
- [ ] Tickless idle: no periodic tick on a core that has nothing runnable
- [ ] POSIX semaphores and process-shared mutexes, including a robust mutex that recovers after its owner dies
- [ ] Passing a file descriptor over a Unix-domain socket
- [ ] Kernel event socket for hotplug, so a waiting process learns that a device appeared

### Network behavior

- [ ] Routing table with more than one address and more than one interface
- [ ] IP fragmentation and reassembly, and path MTU discovery
- [ ] IPv6 neighbor discovery, not only an address on the interface
- [ ] TCP retransmission, congestion control, keepalive, and `TIME_WAIT`
- [ ] ICMP errors delivered to the socket that sent the packet
- [ ] Multicast join and leave
- [ ] Raw sockets and a TUN device a tunnel process can read and write
- [ ] Bridge and VLAN tagging
- [ ] Connection tracking and NAT on the packet-filter hook
- [ ] Checksum and segmentation offload negotiated with the NIC driver
- [ ] Socket receive and send buffers accounted against the process memory limit
- [ ] Wi-Fi roaming, regulatory domain, and 802.1X on top of associate
- [ ] Bluetooth pairing record, L2CAP, RFCOMM, and SDP, stored across reboot
- [ ] Certificate store the TLS client uses to accept or reject a server

### Power, platform, and devices

- [ ] Kernel thermal throttle that lowers CPU frequency when a zone crosses its trip, without waiting for a user process
- [ ] Hardware watchdog device, armed by PID 1, separate from the software watchdog
- [ ] GPIO controller a driver can claim for buttons and resets
- [ ] Fan and voltage sensors readable as numbers
- [ ] PCI device power states, down to D3 and back, across suspend
- [ ] USB autosuspend of an idle device and resume on the next transfer
- [ ] Full HID report parser, so a device is not limited to the fixed mouse and touch sample layout
- [ ] USB attached SCSI, not only bulk-only mass storage
- [ ] SMART health read from AHCI and NVMe
- [ ] Charge limit the battery driver enforces in firmware
- [ ] Hypervisor clock source for a virtual machine, plus virtio console, virtio filesystem, and virtio socket

### Process and security mechanics

- [ ] Thread-local storage the dynamic linker sets up before `main`
- [ ] A virtual syscall page so `clock_gettime` does not enter the kernel
- [ ] Asynchronous I/O: submit a read or write and wait for completion without one thread per request
- [ ] PID, mount, and network namespaces
- [ ] A syscall filter a sandboxed process cannot escape
- [ ] Capabilities narrower than root, user, and guest
- [ ] Kernel keyring and the hash and cipher operations the volume encryption and TLS share
- [ ] Clock discipline: slew the wall clock from NTP instead of stepping it
- [ ] Swap encryption key discarded on shutdown
- [ ] Soft and hard lockup detection that panics or logs when a CPU stops scheduling

## Deeper functional gaps

Behavior only. No windows, panes, or drawing.

### Paging and CPU

- [ ] Five-level page tables when the CPU reports them
- [ ] Process-context identifiers so a switch does not flush the whole TLB
- [ ] Memory protection keys a process can flip without a new page table
- [ ] Shadow stack when the CPU offers it, checked on every return
- [ ] Kernel page-table isolation and an indirect-branch barrier around kernel entry
- [ ] Device-not-available fault that restores the lazy FPU state and returns
- [ ] Debug registers for a hardware breakpoint and a watchpoint
- [ ] Performance counters that a process can program and read
- [ ] Memory encryption for RAM when the CPU offers SME or TME
- [ ] CPU and memory hotplug: a core or a range of RAM arrives after boot and joins the allocator
- [ ] NUMA: allocations prefer the node nearest the running core
- [ ] Huge pages the loader and the GPU can request
- [ ] User-space page-fault handling for a region the kernel does not fill itself
- [ ] Read-copy update so a reader walks a structure while another core updates it
- [ ] Threaded interrupt handlers that sleep outside the hard IRQ
- [ ] A boot-time clock that keeps counting across suspend

### Files and block devices

- [ ] Device mapper: linear, stripe, and RAID assembled from whole disks
- [ ] Multipath so two paths to one disk fail over
- [ ] SCSI mid-layer shared by USB storage, ATAPI, and virtio SCSI
- [ ] NVMe controller reset, more than one namespace, and autonomous power states
- [ ] Self-encrypting drive unlock before the volume mount
- [ ] Secure erase of a discarded range
- [ ] Volume snapshots and a file clone that shares extents until a write
- [ ] Compression of file data in the Robo volume
- [ ] Data checksums verified on read
- [ ] A user filesystem server that serves a mount through a file descriptor
- [ ] Network filesystem client for one remote export
- [ ] `splice` and `sendfile` between two descriptors without a user buffer
- [ ] Anonymous memory that lives in a file descriptor and can be sealed
- [ ] Bind mounts and `pivot_root` from the init ramdisk onto the real root
- [ ] Online grow of a mounted volume

### Network mechanics

- [ ] TCP window scaling, selective acknowledgment, and timestamps
- [ ] SYN cookies when the accept queue is full
- [ ] Explicit congestion notification
- [ ] DHCPv6 and router advertisements that install a default route
- [ ] An encrypted tunnel that carries IP inside UDP
- [ ] Hardware packet timestamps from a NIC that offers them
- [ ] Transmit interrupt coalescing and a poll budget so a flood does not pin a core
- [ ] Ethernet pause frames
- [ ] A DNS cache with a negative entry and a time-to-live
- [ ] 802.11 power save between beacons
- [ ] Cellular broadband through MBIM, presented as another network interface
- [ ] Policy routing: pick a route from the source address, not only the destination

### Devices, boot, and measurement

- [ ] Resizable BAR so a GPU can claim a larger mapping
- [ ] Native PCIe hotplug and downstream-port containment after a link fault
- [ ] Thunderbolt authorization: the new device gets no DMA window until it is allowed
- [ ] VGA arbitration when two display controllers both claim the legacy range
- [ ] Audio sample-rate conversion and a jack-detect interrupt
- [ ] Page flip on vertical blank, reported as an event a waiter can sleep on
- [ ] A shared buffer and a fence between the camera and the display
- [ ] A GPU command queue with preemption and a timeout
- [ ] USB isochronous bandwidth reserved before an audio or video stream starts
- [ ] A USB quirk table and a switch that refuses a device until it is authorized
- [ ] Device firmware update over USB that can be rolled back
- [ ] An early console that prints before PCI exists
- [ ] Kernel exec: boot a new kernel without a firmware reset
- [ ] Trace points and a serial debugger that stops one core
- [ ] Measured boot: extend TPM registers for the bootloader, kernel, and command line
- [ ] Verified system volume: every block checked against a signed tree before use
- [ ] `timerfd`, `eventfd`, and `signalfd`
- [ ] A pid file descriptor that stays valid after the pid number is reused
- [ ] Control groups for CPU time, memory, block I/O, and process count
- [ ] A freezer that stops user processes before hibernate writes the image

## Remaining functional gaps

Behavior only. No windows, panes, or drawing.

### Memory and interrupts

- [ ] Contiguous-memory reserve for devices that cannot scatter a buffer
- [ ] Memory compaction and page migration when a huge page or a device buffer needs a run of frames
- [ ] Same-page merging of identical anonymous pages
- [ ] Compressed RAM in front of swap, with readahead of swapped pages
- [ ] A dirty-page limit that stalls a writer before the cache fills the machine
- [ ] I/O priority so a background write loses to a foreground read
- [ ] IRQ affinity: an interrupt stays on a chosen core, and an idle core can take a share
- [ ] APIC timer in deadline mode, with the legacy programmable interval timer as the fallback
- [ ] Memory-type range registers when the CPU has no page-attribute table
- [ ] IOMMU groups, plus PASID so one device can use more than one address space
- [ ] Single-root I/O virtualization: a physical function hands a virtual function its own BAR and IRQ
- [ ] Peer-to-peer DMA between two devices that does not bounce through the CPU
- [ ] User, UTS, IPC, and time namespaces, with a way to enter an existing namespace
- [ ] Simultaneous-multithreading isolation so two security domains do not share a core
- [ ] Speculative-execution flushes for store-buffer and cache-fill leaks, beyond the entry barrier already listed

### Driver core

- [ ] A kernel symbol table and relocation so a loaded driver can call a published function
- [ ] Probe deferral: a device waits when its supplier has not started, then probes again
- [ ] Device links that suspend and resume suppliers before their consumers
- [ ] A clock enable and a reset line a driver can claim
- [ ] Pin mux so a pad is chosen as GPIO, I2C, or UART before the function driver binds
- [ ] A GPIO line that raises an interrupt
- [ ] Kernel image and init-ramdisk decompression before the first instruction of the uncompressed kernel
- [ ] DisplayPort link training and panel self-refresh
- [ ] HDCP handshake on a digital output
- [ ] A video decode and encode queue that shares the GPU fence
- [ ] MIDI byte stream on the audio device
- [ ] Bluetooth low-energy audio and a rotating device address
- [ ] WPA3 simultaneous authentication of equals, and protected management frames
- [ ] Wireless MAC randomization while scanning
- [ ] Precision time protocol tied to the NIC hardware clock
- [ ] TAI clock and a leap-second table the wall clock consults
- [ ] Zoned block devices and persistent reservations
- [ ] Per-process CPU-time clock and an interval timer

### Services that are not a screen

- [ ] Userspace lock, condition variable, and once-flag built on the futex
- [ ] Stack unwind tables the crash path can walk
- [ ] Structured log with rate limiting, separate from the byte log on the serial port
- [ ] Process accounting records and a login record that survives reboot
- [ ] Socket activation: a listening socket starts its service on the first connection
- [ ] Restart backoff so a crashing service is not spawned in a tight loop
- [ ] Internet printing protocol in front of the print spooler
- [ ] A scanner path that pulls a page from a USB or network scanner
- [ ] An authorization check a privileged action must pass, distinct from the sandbox grant list
- [ ] A security-key challenge for login, beside the password and the fingerprint
- [ ] File-level integrity checks a process can request, distinct from the signed system volume
- [ ] Kernel lockdown once booted from a verified volume: no later write to kernel memory
- [ ] Audit rules that record selected syscalls, not only login and package events
- [ ] Hostname and domain stored in the UTS namespace and read back after reboot

## Further functional gaps

Behavior only. No windows, panes, or drawing.

### Scheduling, faults, and boot state

- [ ] Deadline scheduling: a cyclic thread with a runtime, a deadline, and a period
- [ ] Full kernel preemption, including sleeping inside a driver lock that is not in interrupt context
- [ ] An isolated core that takes no timer tick and no unbound interrupt
- [ ] `vfork` that shares the address space until the child calls `exec` or exits
- [ ] An alternate signal stack, and queued realtime signals that are not dropped
- [ ] `mremap` and `msync` of a file-backed mapping
- [ ] `chroot` and a process mount and unmount
- [ ] `getrandom` that blocks until the entropy pool is initialized
- [ ] A kernel stack canary checked on return from every kernel function
- [ ] Control-flow integrity on indirect kernel calls
- [ ] Freed kernel memory wiped before the next allocation
- [ ] CPU-feature patching: boot replaces slow instruction sequences with the ones this CPU supports
- [ ] A hung-task warning and an RCU stall warning, each with the blocked stack
- [ ] A persistent console buffer that survives a reset
- [ ] Live replacement of a kernel function without a reboot
- [ ] Steal-time accounting when a hypervisor pauses this virtual CPU

### Storage and network behavior

- [ ] SCSI abort and device reset when a command times out
- [ ] Remap of a sector the device reports as medium error
- [ ] Thin provisioning that allocates a block on the first write
- [ ] A network block device and an iSCSI session
- [ ] Read-only mount of a foreign NTFS volume
- [ ] File leases that break when another opener writes
- [ ] `copy_file_range` that clones on the same volume and copies across volumes
- [ ] Generic receive and segmentation offload in the host stack
- [ ] Reverse-path filtering of packets whose source is not reachable
- [ ] Duplicate-address detection before an address is used
- [ ] Multipath TCP
- [ ] DNSSEC validation in the resolver
- [ ] Certificate revocation checked during the TLS handshake
- [ ] A remote syslog sink
- [ ] File transfer over the secure shell channel
- [ ] A packet program the filter hook can load and unload

### Power, motion, and a guest machine

- [ ] Suspend-to-idle that keeps the CPU in a deep idle state without a full S3 cycle
- [ ] Hardware-managed CPU performance states, including turbo, with an energy reading and a power cap
- [ ] Fan control from the thermal zone, not only a sensor read
- [ ] USB power-delivery contract: voltage and role, and a dual-role port that can become a host or a device
- [ ] PWM, an analog input, and a quadrature encoder
- [ ] Gyroscope and magnetometer samples beside the accelerometer
- [ ] CAN and EtherCAT so a motion device has a cyclic field bus
- [ ] Force feedback on a gamepad
- [ ] Adaptive sync, display-stream compression, and a per-process GPU address space
- [ ] A compute dispatch that waits on the same fence as the video queue
- [ ] Authenticated encryption in the keyring
- [ ] A hypervisor: create a guest, assign it memory and a virtual CPU, and give it a virtio device
- [ ] Virtio IOMMU and virtio memory hotplug for that guest
- [ ] The guest clock reports steal time

## Later functional gaps

Behavior only. No windows, panes, or drawing.

### Process and protocol primitives

- [ ] Restartable sequences and a memory barrier that waits until every core has left a critical section
- [ ] POSIX message queues with priorities
- [ ] `mincore`, `process_vm_readv`, and a signal sent through a pid file descriptor
- [ ] `close_range`, `statx`, and a file handle that survives rename
- [ ] Fanotify that reports the writing process, not only the directory event
- [ ] `execveat`, `waitid` that does not block, `uname`, and `sysinfo`
- [ ] `reboot` and `swapon` system calls a process can invoke
- [ ] A syscall-filter notification so a supervisor can allow one blocked call
- [ ] Path rules a sandbox cannot widen after they are installed
- [ ] Account lockout after repeated failures, password aging, and a temporary privilege grant that expires
- [ ] TLS session resumption and DTLS for datagrams
- [ ] TCP fast open and several listeners sharing one port
- [ ] ICMP rate limiting, IPv6 extension headers, and neighbor unreachability detection
- [ ] DHCP renewal and IPv6 prefix delegation
- [ ] SCTP associations
- [ ] SNMP and an out-of-band management controller
- [ ] Entropy reseed on a timer and after a resume

### Block, bus, and guest mechanics

- [ ] RAID scrub and rebuild onto a spare
- [ ] Write-zeroes and a data-integrity tag on each sector
- [ ] Write support for an optical disc and for UDF
- [ ] Read-only mount of an ext4 volume
- [ ] NVMe over a network fabric
- [ ] USB device mode: serial, network, and storage gadgets on a dual-role port
- [ ] USB4 tunneling beside Thunderbolt
- [ ] SoundWire and an AC97 fallback where HD Audio is absent
- [ ] MIPI display bus and the embedded-DisplayPort power sequence
- [ ] HDR metadata and a hardware color lookup table programmed by a driver
- [ ] Debounce on a GPIO input
- [ ] RS-485 and Modbus
- [ ] 1-Wire for a temperature probe
- [ ] An emergency-stop input that forces every PWM output off
- [ ] A stepper pulse train with a maximum rate
- [ ] Second-level address translation, posted interrupts, and instruction emulation for a guest exit
- [ ] Nested virtualization and live migration of a guest
- [ ] A detector for leaked kernel allocations and for lock-order cycles
- [ ] An NMI backtrace of every core

## Still-open functional gaps

Behavior only. No windows, panes, or drawing.

### Failures, memory, and files

- [ ] A poisoned RAM page taken offline, with only the process that touched it killed
- [ ] Corrected DRAM error counts reported by the memory controller
- [ ] A critical thermal or critical battery level that cuts power after the journal flushes
- [ ] Pressure-stall counters for CPU, memory, and block I/O
- [ ] A choice to panic or to continue after a non-fatal kernel fault
- [ ] A filesystem freeze that drains writers, and an online shrink of a mounted volume
- [ ] Persistent memory and CXL memory, mapped directly and surviving power loss
- [ ] An overlay mount and an id-mapped mount
- [ ] Per-file encryption with a key from the keyring, separate from full-volume encryption
- [ ] An automounter that attaches a network export on first lookup
- [ ] NVMe completion by polling when the queue is busy
- [ ] Transparent collapse of anonymous pages into huge pages
- [ ] Checkpoint and restore of a running process, including its file descriptors and address space

### Links, scheduling, and trust

- [ ] Traffic control: a fair queue and a rate limit on an interface
- [ ] Link aggregation that negotiates LACP
- [ ] Spanning tree on a bridge, and IGMP snooping
- [ ] Remote DMA that a process can register and wait on
- [ ] Fibre Channel name service and a remote disk behind it
- [ ] I2S and a LIN transceiver
- [ ] An 802.15.4 radio with a mesh route
- [ ] Hybrid-core scheduling that prefers a performance core for a waking thread
- [ ] Split-lock detection that logs or faults an unaligned atomic
- [ ] An idle governor that picks a C-state from the expected sleep
- [ ] A confidential guest whose memory the hypervisor cannot read, plus remote attestation of that guest
- [ ] Cache and memory-bandwidth limits a control group can set
- [ ] A control-group namespace

## Additional functional gaps

Behavior only. No windows, panes, or drawing.

### ABI, placement, and firmware records

- [ ] A 32-bit system-call layer on the 64-bit kernel, with matching pointer and structure widths
- [ ] `clone3` and `posix_spawn` that build the child without a following `exec` race
- [ ] A process memory policy: bind, prefer, or interleave pages across NUMA nodes, and migrate pages already allocated
- [ ] A CPU set that confines a control group to named cores and nodes
- [ ] Secret memory that cannot be read by a core dump or by the hypervisor
- [ ] Port I/O permission for one privileged process
- [ ] Read-only relocations and immediate binding applied by the dynamic linker
- [ ] A deny list a kernel module cannot be loaded past
- [ ] Integrity appraisal of a file before execution, using a key separate from the volume signature
- [ ] A firmware capsule applied on the next boot, with the previous firmware kept for rollback
- [ ] Firmware error records from the last boot, and a reserved region a crash kernel can boot into

### Packets, storage targets, and small devices

- [ ] A virtual Ethernet pair, a MACVLAN endpoint, and a virtual routing table
- [ ] A VXLAN overlay
- [ ] Internet-key exchange that installs the encrypted-tunnel keys
- [ ] NAT64 so an IPv6 caller reaches an IPv4 server
- [ ] Receive-side scaling across NIC queues, a jumbo MTU, and energy-efficient Ethernet
- [ ] A socket busy-poll and a SYN proxy in front of a full accept queue
- [ ] A time-aware shaper on a bridge port
- [ ] An iSCSI target and an NFS server
- [ ] A DHCP server and router advertisements this machine originates
- [ ] Near-field communication and a smart-card reader
- [ ] Proximity, barometric pressure, and humidity
- [ ] A digital-to-analog output and SPI NOR flash
- [ ] A scanned GPIO key matrix
- [ ] A hinge switch that reports tablet posture

## Next functional gaps

Behavior only. No windows, panes, or drawing.

### CPU, pages, and dumps

- [ ] User mode blocked from reading kernel segment registers
- [ ] A thread that writes its TLS base with FSGSBASE and does not enter the kernel
- [ ] Linear address masking checked on user pointers
- [ ] Idle-page tracking that demotes cold pages onto a slower memory tier
- [ ] `MAP_SYNC` so a store to persistent memory is durable at the instruction
- [ ] A pipe tee that duplicates a byte stream into a second pipe
- [ ] A watchdog pretimeout that runs a handler before the reset
- [ ] Firmware error injection that reports a synthetic machine check
- [ ] Collaborative processor performance as the interface behind hardware P-states
- [ ] Cluster scheduling that keeps a waking thread on the cluster where its cache still lives
- [ ] A core-dump filter for which mappings are written
- [ ] A wait on several futex words in one call
- [ ] User-space probes that run a handler at an instruction without stopping the process

### Services, links, and guest assist

- [ ] Kerberos tickets an NFS client presents, and NFSv4 delegations recalled on a conflicting open
- [ ] A directory lookup for accounts and groups
- [ ] A virtual router address that fails over, and multicast routing
- [ ] Priority flow control on a data-center bridge
- [ ] A GRE tunnel and an IPVLAN endpoint
- [ ] Case-insensitive directory lookup where the volume marks it
- [ ] Online defragmentation, forced unit access, and write-lifetime hints passed to the device
- [ ] An ACPI docking-station notify, a color sensor, and a HID sensor hub
- [ ] Virtio GPIO and virtio persistent memory
- [ ] A host accelerator that serves a guest virtio queue without exiting on every buffer
- [ ] Paravirtual end-of-interrupt and paravirtual spin locks for a guest
- [ ] A guest debugger that stops one virtual CPU and reads its registers

## Open functional gaps

Behavior only. No windows, panes, or drawing.

### Memory, mounts, and privileges

- [ ] Object caches and a virtually contiguous kernel mapping, backed by a per-CPU allocator
- [ ] An expedited read-copy-update grace period
- [ ] `mseal` of an address range, and advice that frees or collapses pages in another process
- [ ] A new mount API: create a mount tree and attach it without a path race
- [ ] Soft-dirty bits so a checkpoint writes only pages changed since the last pass
- [ ] A parent-death signal, a subreaper, and a no-new-privileges bit the process cannot clear
- [ ] Distinct process, user, and session keyrings, each with an expiry
- [ ] A device control group that hides selected device nodes from its members

### Devices, USB, and guests

- [ ] Shared virtual addressing: a device page fault is resolved through the process page tables
- [ ] PCIe access-control services that block unwanted peer-to-peer traffic
- [ ] PCIe precision time measurement
- [ ] Assignment of a physical device to one guest, with its IOMMU window and interrupts
- [ ] A mediated GPU the guest sees as a display device
- [ ] xHCI stream IDs and USB 3 link power states U1 and U2
- [ ] DisplayPort alternate mode on a USB-C port
- [ ] A USB debug channel that stays up across reset
- [ ] An SMB file client and a 9P mount
- [ ] A zero-copy socket send that completes when the NIC frees the pages
- [ ] A userspace suspend blocker, and a record of which device last woke the machine
- [ ] Display power states, the sink’s audio layout, a hardware writeback of one plane, and a lease of that plane to one process
- [ ] A render node that can submit GPU work with no permission to change the mode

## Further open gaps

Behavior only. No windows, panes, or drawing.

### Memory policy and guests

- [ ] Swap preference so anonymous pages are reclaimed before file cache, or the reverse
- [ ] An out-of-memory kill that covers every process in one control group
- [ ] File capabilities stored on an executable, plus ambient capabilities that survive `exec`
- [ ] Extended verification of attribute values, so a changed attribute fails appraisal
- [ ] A memory-latency table the NUMA allocator consults
- [ ] CXL memory that can be added and removed while the system runs, and pooled across hosts
- [ ] Replay-protected memory on an eMMC device
- [ ] Post-copy live migration: the guest runs while missing pages are fetched
- [ ] Guest free-page reporting so the host reclaims pages the guest is not using
- [ ] TSC scaling, and an allow-list of CPUID leaves and model-specific registers the guest may see
- [ ] A Hyper-V-style virtual bus and Xen grant-table sharing

### Radios, names, and disks

- [ ] Wi-Fi multi-link operation, 6 GHz, target wake time, and opportunistic wireless encryption
- [ ] DNS over TLS
- [ ] An XDP socket that receives frames without a full network stack
- [ ] Geneve, IP-in-IP, and L2TP
- [ ] NFS parallel layout and an SMB session with several connections and encryption
- [ ] A hall sensor and a human-presence sensor
- [ ] Fast initial link setup for a wireless network

## Remaining open gaps

Behavior only. No windows, panes, or drawing.

### CPU, images, and keys

- [ ] A process enclave whose memory the kernel cannot read after it is entered
- [ ] User interrupts delivered without a normal kernel entry
- [ ] Processor trace that records taken branches for the crash dump
- [ ] XTS and a hardware inline-encryption engine on the block path
- [ ] A read-only compressed system image, and a cpio parser for the init ramdisk
- [ ] Long names on FAT volumes
- [ ] Unicode normalization where directory lookup is case-insensitive
- [ ] Byte-range locks that cover only part of a file
- [ ] A firmware file cache a driver can request by name
- [ ] Machine-owner keys, a secure-boot revocation list, and a TPM event log
- [ ] Device attestation and link encryption negotiated with a PCIe device
- [ ] A data-movement accelerator and a crypto accelerator the block and TLS paths can offload to

### Buses and names

- [ ] SDIO, eMMC command queueing, and NVMe sanitize
- [ ] SCSI extended copy between two disks without a host buffer
- [ ] Bluetooth host traffic over a UART, and a one-pulse-per-second input
- [ ] A point-to-point line discipline for a dial-up or direct serial link
- [ ] Wi-Fi Direct and a Bluetooth mesh route
- [ ] A secure element that answers an application-protocol data unit
- [ ] RADIUS for the existing 802.1X path
- [ ] A directory-services referral when the account is not on this machine

## Later open gaps

Behavior only. No windows, panes, or drawing.

### Calls, logs, and migration

- [ ] Futex requeue, so a wake can move a waiter from one word to another
- [ ] `pidfd_getfd` and a spawn that places the child in a control group
- [ ] `cachestat` and a mount-information call that does not walk a path
- [ ] An IMA measurement log, distinct from the TPM event log
- [ ] TPM non-volatile storage and a policy that ties a key to PCR values
- [ ] A unified kernel image the firmware boots as one signed file
- [ ] IOMMU dirty-bit logging so an assigned device can move with a guest
- [ ] Encrypted virtual-CPU state that the host sees only while the guest is exited
- [ ] Hardware feedback for the scheduler, precise performance samples, and a userspace enqueue to a shared accelerator queue

### Names, radio, and device commands

- [ ] DNS64, a NetBIOS name, and an NFS lock manager with its port mapper
- [ ] A distributed-file-system referral
- [ ] A regulatory database the wireless stack consults by country code
- [ ] Tunneled direct link setup, neighbor awareness, and fine timing measurement
- [ ] Bluetooth broadcast audio and direction finding
- [ ] A security-key PIN and a resident key stored on the authenticator
- [ ] NVMe boot partitions, a device self-test, and an NVMe copy command
- [ ] An I2C bus bit-banged on GPIO when no controller is present

## Still later gaps

Behavior only. No windows, panes, or drawing.

### Paths, packets, and traces

- [ ] Path lookup that stays inside a directory tree and refuses magic links
- [ ] A registered buffer ring for asynchronous I/O
- [ ] Kernel TLS that encrypts a TCP stream on the socket
- [ ] A remote console that sends kernel log lines over UDP
- [ ] Kernel probes on a function entry and return
- [ ] A timeline of explicit sync points a device can wait on
- [ ] Several kernel virtual terminals switched from the keyboard
- [ ] An audio DSP that runs a loaded firmware graph
- [ ] HDMI consumer-electronics control and enhanced audio return

### Platform buses and sensors

- [ ] I3C, SPMI, and SLIMbus
- [ ] Management-component transport and a platform-level data model
- [ ] An IPMB link to a baseboard controller
- [ ] An ACPI watchdog description the hardware watchdog driver uses
- [ ] A unified sensor interface with a hardware FIFO and a watermark
- [ ] A smart-battery pack that reports cycles and chemistry
- [ ] An electronic privacy shutter the camera driver honors
- [ ] ACPI notifications when a video output appears or disappears

## Next open gaps

Behavior only. No windows, panes, or drawing.

### Disks, names, and cameras

- [ ] A SAS expander and a SATA port multiplier
- [ ] Fibre Channel over Ethernet
- [ ] SCSI enclosure services that report slot power and a locate light
- [ ] A local cache of file data fetched from a network volume
- [ ] DNS over HTTPS
- [ ] EAP-TLS on the existing 802.1X path
- [ ] An image-signal processor, a lens focus motor, and a camera flash
- [ ] One media request that binds a buffer to a single captured frame
- [ ] Compressed and tiled framebuffer storage
- [ ] A user pointer the GPU can take a fault on
- [ ] An HDMI picture infoframe and local dimming of the panel
- [ ] A function filesystem a USB gadget uses to exchange buffers

### Guests and limits

- [ ] A confidential guest that accepts each page before it is used
- [ ] Huge-page charges accounted inside a control group
- [ ] A high memory threshold that starts reclaim before the out-of-memory killer
- [ ] Write-protect faults delivered to a user handler
- [ ] Serial over LAN through the baseboard controller

## Further remaining gaps

Behavior only. No windows, panes, or drawing.

### Memory, traces, and pixels

- [ ] A compressed-RAM block device that can back swap
- [ ] A minimum memory reserve the reclaim code cannot take from a control group
- [ ] Tracing limited to a process and its descendants
- [ ] Stacked security modules, plus a program attached to a trace point and to a control-group socket
- [ ] The init ramdisk extended into a PCR, and a boot-configuration block read before the command line
- [ ] Function-graph tracing with more than one instance
- [ ] A lockless kernel log ring that a panic can flush
- [ ] An asynchronous modeset that returns before the hardware finishes
- [ ] A color transform matrix on a display plane
- [ ] Multiplanar video buffers and compressed audio over HDMI
- [ ] Panel replay that stops the link while the image is unchanged
- [ ] Several CSI-2 virtual channels from one camera link

### Load, hotplug, and guests

- [ ] Compressed kernel modules
- [ ] A modalias coldplug that binds drivers already present at boot
- [ ] A filesystem of EFI variables and a config filesystem for gadget and storage targets
- [ ] An NVMe host memory buffer, and namespace create and delete
- [ ] A PCIe attention button and surprise removal
- [ ] Migration of a confidential guest, including its accepted pages

## Still unnamed gaps

Behavior only. No windows, panes, or drawing.

### Links and field buses

- [ ] CAN FD, ISO-TP, and J1939
- [ ] FlexRay
- [ ] A credit-based shaper and packet pacing from a socket timestamp
- [ ] LLDP and an SFP cage driven over MDIO
- [ ] Ethernet forward-error correction and a flow-director rule
- [ ] Synchronous Ethernet locked to the same clock as the precision-time hardware
- [ ] TCP authentication options
- [ ] A multishot accept that re-arms without another submit

### Devices and guest accelerators

- [ ] Guest halt-poll, a virtio sound device, and a virtio GPU that accepts a command stream
- [ ] A vDPA device a guest can use without a software exit on each buffer
- [ ] Scalable I/O virtualization
- [ ] USB gadget audio and gadget HID
- [ ] USB-PD vendor-defined messages and a Type-C retimer
- [ ] A haptic motor and speaker current sense
- [ ] Camera metadata beside the image, and D-PHY or C-PHY on the camera link
- [ ] A keyboard backlight and an indicator LED
- [ ] NVMe zone append and weighted round-robin queues
- [ ] A dirty-log ring the hypervisor shares with its user process
- [ ] Securebits that keep or clear capabilities across `exec`
- [ ] Embedded DisplayPort advanced link power management

## Additional unnamed gaps

Behavior only. No windows, panes, or drawing.

### Time, packets, and industrial links

- [ ] Frame replication so one stream leaves on two ports and the receiver keeps a single copy
- [ ] A network-emulator queue that adds delay, loss, and reordering
- [ ] One-step precision-time stamps and a hardware pin for an external pulse
- [ ] An ACPI power-management timer and a nonstop time-stamp counter
- [ ] A wake alarm that fires while the machine is suspended
- [ ] PROFINET and EtherNet/IP cyclic I/O
- [ ] An NVMe compare command and dataset-management hints

### Accelerators, USB, and launch

- [ ] Virtualized performance counters inside a guest
- [ ] A software vDPA device and an IOMMU file descriptor a user process can program
- [ ] Virtio power-management and a zoned virtio disk
- [ ] A USB gadget camera
- [ ] A USB billboard that advertises alternate modes
- [ ] A USB4 host router and a preboot access list for tunneled PCIe
- [ ] Enclave launch control and a quote of that enclave
- [ ] Tile-register state saved with the rest of the extended processor state
- [ ] A microcode reload while the system is running
- [ ] A PCIe enclosure LED controlled without a SCSI enclosure

## Next unnamed gaps

Behavior only. No windows, panes, or drawing.

### Field I/O and PCIe power

- [ ] CANopen and Modbus TCP
- [ ] IO-Link
- [ ] GOOSE and sampled-value streams on an IEC 61850 network
- [ ] Diagnostics over IP and a unified diagnostic session
- [ ] PCIe active-state power management and latency-tolerance reporting
- [ ] CXL dynamic capacity that grows or shrinks a memory region
- [ ] A DRAM page repair that retires a bad row
- [ ] A corrected machine-check interrupt, with a storm limit that stops the reports
- [ ] A PCI option ROM mapped only while it is being read
- [ ] A manually operated retention latch and a PCIe slot power controller

### Locks, scheduling, and USB power

- [ ] Open-file-description locks that are not shared across `dup`
- [ ] An extensible scheduler a loaded program can run
- [ ] A low memory threshold that warns before the minimum reserve
- [ ] Polled completion of asynchronous I/O
- [ ] A USB4 time-management unit
- [ ] Programmable and extended-range USB power delivery
- [ ] An electromechanical interlock that reports the slot is safe to power

## Field and link gaps

Behavior only. No windows, panes, or drawing.

### Protocols

- [ ] MACsec on an Ethernet link, with a key agreement tied to 802.1X
- [ ] CAN XL
- [ ] SENT and PSI5 sensor frames
- [ ] BACnet, DNP3, and HART
- [ ] DeviceNet and AS-Interface
- [ ] POWERLINK
- [ ] SOME/IP service discovery and remote procedure calls
- [ ] SecOC so a field-bus payload carries an authenticator
- [ ] Diagnostics over XCP
- [ ] Hotspot 2.0 and EAP-TTLS
- [ ] Bluetooth enhanced credit-based flow control and periodic advertising with responses

### Memory, control, and PCIe mailboxes

- [ ] CXL.cache so a device can coherently use host memory
- [ ] A control-group kill that signals every member
- [ ] A low-water memory line below the high threshold and above the reserve
- [ ] A live kernel update that keeps assigned devices across the new image
- [ ] A PCIe data-object exchange mailbox
- [ ] An RDMA charge inside a control group
- [ ] A threaded control group whose threads can be placed independently

## Transport and clock gaps

Behavior only. No windows, panes, or drawing.

### Transports

- [ ] QUIC, and HTTP/3 on top of it
- [ ] MPLS and IPv6 segment routing
- [ ] Bidirectional forwarding detection
- [ ] RoCEv2, iWARP, IPoIB, and iSCSI extensions for RDMA
- [ ] Shared-memory communications over RDMA and over an internal link
- [ ] 464XLAT so an IPv6-only host reaches an IPv4 peer
- [ ] Secure RTP and a port-control mapping
- [ ] Receive-flow steering and transmit-queue selection by CPU
- [ ] Enhanced transmission selection and a data-center bridging exchange

### Industrial publish and device control

- [ ] OPC UA publish and subscribe
- [ ] A data-distribution service with realtime publication
- [ ] CC-Link IE and Sercos
- [ ] A device-link health report, a driver reload that keeps the port, and a firmware coredump region
- [ ] A digital phase-locked loop that holds its output when the reference is lost
- [ ] An InfiniBand subnet manager
- [ ] A clock holdover interval reported to the precision-time client

## Media and control gaps

Behavior only. No windows, panes, or drawing.

### Media, buildings, and vehicles

- [ ] AES67 audio and SMPTE ST 2110 essence over the precision-time clock
- [ ] IEEE 1722 audio-video transport
- [ ] A genlock input that locks a media clock
- [ ] NMOS registration and discovery of a media sender
- [ ] DALI lighting, DMX512, and KNX
- [ ] MQTT and CoAP for constrained devices
- [ ] A shared-memory publish path that does not pass through a broker
- [ ] On-board diagnostics on top of the unified diagnostic session

### Scheduling and link power

- [ ] Edge-triggered readiness, distinct from level-triggered poll
- [ ] A BPF memory arena shared with a loaded program
- [ ] A realtime bandwidth cap so deadline and realtime threads cannot consume every core
- [ ] Last-branch records captured with a performance sample
- [ ] A control group that asks for memory reclaim without waiting for pressure
- [ ] PCIe L1 substates and optimized buffer flush and fill
- [ ] A continuous camera torch, separate from the flash pulse

## Session and fabric gaps

Behavior only. No windows, panes, or drawing.

### Sessions and lighting

- [ ] RTSP, RTCP, and a SIP session that negotiates a media stream
- [ ] Secure Reliable Transport for a contribution feed
- [ ] JPEG XS as an ST 2110 video essence
- [ ] The SMPTE 2059 profile on the precision-time clock
- [ ] A linear timecode input
- [ ] Art-Net and streaming ACN on top of DMX512
- [ ] Lightweight M2M device management over CoAP
- [ ] Sparkplug birth and death certificates on MQTT
- [ ] A WebSocket, an AMQP broker connection, and a gRPC call

### Fabrics and placement

- [ ] An NVMe controller memory buffer
- [ ] Asymmetric namespace access with states a multipath layer can use
- [ ] An NVMe telemetry log
- [ ] Flexible data placement that picks an endurance group
- [ ] NVMe over TCP protected by TLS
- [ ] A SCSI RDMA protocol path beside iSCSI extensions for RDMA

## Redundancy and management gaps

Behavior only. No windows, panes, or drawing.

### Redundant links and sessions

- [ ] Parallel redundancy and high-availability seamless redundancy
- [ ] A media-redundancy protocol ring and a device-level ring
- [ ] IEC 60870-5-104 and MMS for an IEC 61850 server
- [ ] NETCONF with a YANG model
- [ ] IPFIX and sFlow exports
- [ ] Two-way active measurement
- [ ] An EVPN route and an MPLS pseudowire
- [ ] STUN, TURN, and connectivity checks that pick a media path
- [ ] NMOS connection management that activates a registered sender
- [ ] ST 2110-40 ancillary data
- [ ] A compressed contribution stream in the NDI format

### NVMe commands still open

- [ ] An NVMe persistent-memory region
- [ ] Write-uncorrectable and an NVMe format
- [ ] A zone random-write area
- [ ] In-band NVMe authentication with a challenge response

## Routing and enrollment gaps

Behavior only. No windows, panes, or drawing.

### Routes

- [ ] OSPF, IS-IS, and BGP
- [ ] MPLS label distribution
- [ ] Bit-indexed explicit replication
- [ ] Multicast source discovery and source-specific multicast
- [ ] Equal-cost multipath and fast reroute
- [ ] A multi-chassis link aggregation
- [ ] Graceful restart of a routing adjacency
- [ ] Route-origin validation
- [ ] A BGP flow specification installed in the packet filter
- [ ] A path-computation element

### Identity and checks

- [ ] TACACS and Diameter
- [ ] A certificate-transparency log and automatic certificate enrollment
- [ ] SMB over QUIC
- [ ] An NFS server-side copy
- [ ] `SEEK_DATA` and `SEEK_HOLE`
- [ ] A kernel thread that polls an asynchronous submission queue
- [ ] A kernel address sanitizer and a data-race detector
- [ ] Timer slack and a clocksource watchdog
- [ ] A paravisor for a confidential guest, and assignment of a device into that guest
- [ ] gNMI streaming and RESTCONF

## Enrollment and multicast gaps

Behavior only. No windows, panes, or drawing.

### Enrollment, files, and detectors

- [ ] An uninitialized-memory detector and a sampled page-fault detector
- [ ] Simple certificate enrollment, enrollment over secure transport, and the certificate management protocol
- [ ] DANE records that bind a certificate to a DNS name
- [ ] SSH host and user certificates
- [ ] An SMB witness and a persistent handle that survives a short disconnect
- [ ] NFS over RDMA
- [ ] A registered file table for asynchronous I/O
- [ ] HTTP/2 streams over one connection
- [ ] An end-to-end counter and CRC on a field-bus payload

### Multicast and tunnels

- [ ] RSVP traffic engineering
- [ ] Unequal-cost multipath
- [ ] A multicast VPN
- [ ] A BGP monitoring session
- [ ] Nonstop routing so a restart does not drop the forwarding table
- [ ] Babel
- [ ] IGMPv3 and MLDv2 source filters
- [ ] In-situ operations, administration, and maintenance
- [ ] A route-based IPsec tunnel and NAT traversal for its handshake
- [ ] Seamless forwarding detection
- [ ] Microloop avoidance after a routing change

## Tunnel and steering gaps

Behavior only. No windows, panes, or drawing.

### Tunnels and names

- [ ] A policy-based IPsec tunnel, an authentication header, and a handshake that survives a NAT address change
- [ ] A handshake cookie on the encrypted UDP tunnel
- [ ] Zenoh discovery and a ROS 2 graph over the data-distribution service
- [ ] An AUTOSAR protocol-data-unit gateway
- [ ] The 802.1AS profile on the precision-time clock
- [ ] A simple two-way active measurement session and a one-way delay measurement
- [ ] Alternate marking of packets and postcard telemetry
- [ ] A traceroute that reports each hop
- [ ] SMB directory leases, continuous availability, and SMB Direct
- [ ] NTLM and SPNEGO
- [ ] NFS id mapping and an NFS referral

### Steering and samples

- [ ] Receive packet steering onto the core that will consume the packet
- [ ] A flower classifier offloaded to the NIC
- [ ] An anycast gateway and integrated routing and bridging
- [ ] Multipoint label distribution and topology-independent loop-free alternates
- [ ] A HID program written in BPF
- [ ] A BPF token that delegates permission to load a program
- [ ] An undefined-behavior sanitizer in the kernel
- [ ] Instruction-based sampling on the performance counters

## Order to implement

1. IRQ return path, APIC timer, monotonic clock, heap, and wait queues. Later drivers block and interrupt on these.
2. Page fault, address-space switch, user copies, and the syscall table.
3. PCI bridge walk, BAR map, and a real probe/start/stop driver object.
4. Firmware display, PS/2 keyboard and mouse, AHCI or NVMe, and the partition plus FAT32 mount.
5. xHCI, hub, HID, and USB mass storage.
6. HD Audio, ACPI power buttons, battery, shutdown, virtio-net or USB Ethernet, then the socket stack.
7. Vendor drivers, one package at a time, each one replacing only the basic driver for its own device.
8. PID 1, panic and crash dump, then login against a real password store and a session that keeps running.
9. Window server, fonts, clipboard, Finder on the real volume, the terminal, and the audio server.
10. Signed packages, home encryption, updates with rollback, time zone and NTP, and the WPA supplicant.
11. Entropy, ECAM, microcode, machine check, XSAVE, the clock syscall, links, `exec` arguments, and the dynamic linker.
12. Touch, pen, docks, EDID hotplug, hibernate, Bluetooth audio, the installer, and the remaining desktop and network services.
13. Page cache, writeback, the block I/O scheduler, routing, TCP recovery, and NAT.
14. Namespaces, the syscall filter, the keyring, thermal throttle, and device power states.
15. Huge pages, NUMA, PCID, mitigations, the device mapper, TCP scaling, measured boot, and control groups.
16. Contiguous memory, driver probe order, IOMMU groups, link training, WPA3, and the non-visual services.
17. Deadline scheduling, storage recovery, suspend-to-idle, field buses, and a guest hypervisor.
18. Restartable sequences, account lockout, RAID rebuild, USB device mode, motion stop, and guest migration.
19. Memory poisoning, persistent memory, traffic control, link aggregation, and confidential guests.
20. The 32-bit system-call layer, NUMA policy, firmware capsules, virtual networks, and storage targets.
21. Idle-page demotion, Kerberos, priority flow control, and guest virtio acceleration.
22. Device assignment to a guest, shared virtual addressing, suspend blockers, and sealed memory.
23. Swap preference, file capabilities, CXL hot-add, post-copy migration, and Wi-Fi multi-link.
24. Enclaves, inline encryption, device attestation, and the remaining buses.
25. Futex requeue, measurement logs, IOMMU dirty logging, DNS64, and NVMe self-test.
26. Path confinement, kernel TLS, sensor FIFOs, and the management buses.
27. Storage expanders, the camera pipeline, confidential page accept, and memory thresholds.
28. Compressed swap, tracing instances, NVMe namespaces, and confidential migration.
29. CAN FD, link-layer hardware, virtio sound and GPU commands, and zone append.
30. Frame replication, industrial Ethernet, enclave quotes, and a USB4 host router.
31. CANopen, PCIe link power, dynamic CXL capacity, and USB extended-range power.
32. MACsec, field-protocol authentication, CXL.cache, and a live kernel update.
33. QUIC, RDMA transports, forwarding detection, and clock holdover.
34. Timed media transport, building buses, edge-triggered poll, and PCIe L1 substates.
35. Media session setup, lighting-over-network, NVMe telemetry, and NVMe over TCP.
36. Redundant industrial rings, flow exports, media-path checks, and the remaining NVMe commands.
37. Routing protocols, certificate enrollment, SMB over QUIC, and a paravisor.
38. Certificate enrollment protocols, multicast VPN, NFS over RDMA, and route-based IPsec.
39. Policy-based IPsec, SMB Direct, packet steering, and BPF tokens.
