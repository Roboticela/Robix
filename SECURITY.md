# Security policy

Robix is a language for programs that move machines. A defect in the compiler, the package check, or the network runtime can become a defect in a robot. Please report security issues privately.

## 📣 Reporting

Open a **private security advisory** on this GitHub repository:

**Security → Advisories → Report a vulnerability**

Include:

- The Robix version (`robix version` or `robixc --version`).
- The operating system and the exact command.
- The source file, reduced as far as you can while the problem still happens.
- What you expected and what happened.

Please give us a chance to look before you publish the details. We will answer on the advisory.

There is no separate security email for this repository. The GitHub advisory is the channel.

## 🧭 Supported versions

| Version | Security reports |
| --- | --- |
| 1.0.0 on the default branch | Accepted |
| Older snapshots | Accepted when the defect is still on the default branch |

`Documentation/Status.md` records the toolchain that was actually built and run.

## 🔍 What to report

These are in scope:

- Memory safety bugs in `robix`, `robixc`, the interpreter, or the native runtime: out-of-bounds access, use-after-free, double-free, or a compiler crash on ordinary source.
- A package whose bytes do not match the lockfile SHA-256, or an HMAC-SHA256 publisher signature that is accepted when the key does not match.
- Secrets from a `.env` file, a certificate password, or a server token copied into an executable that should not contain them.
- A RobixUI document or event binding that runs a handler the type checker rejected.
- A network session that accepts a peer without the check the project asked for.
- A diagnostic that is skipped so unsafe code runs as if it were safe.

## 🧱 What this tree already states

Saying these plainly keeps a report useful:

- Package download is checked with SHA-256 and an HMAC-SHA256 publisher key. There is no public registry in this tree. A signature match means the key you configured, not a global authority.
- TLS samples use a self-signed certificate created on the machine that ran them (`robix-stagek`, `robix-stagen`). They are not issued by a public certificate authority.
- `GPIO`, `PWM`, `UART`, `SPI`, `I2C`, `CAN`, `USB`, `PCIe`, `DMA`, sensors, and motors on Windows report that the device is not available. They do not return a measured pin level.
- A freestanding image that has booted under QEMU has not been validated on physical hardware.
- A successful compile does not mean a robot is physically safe. Actuator limits, emergency stops, and watchdogs belong in the program and in the hardware.

## ⏱️ What happens next

1. We confirm the report on the version you named.
2. We agree on a fix and a note in [CHANGELOG.md](CHANGELOG.md).
3. We publish the advisory when the fix is available, or earlier if you and we agree the details are already public.

Please avoid public proof-of-concept code that drops a shell, steals credentials, or drives hardware. A description and a small source file are enough.
