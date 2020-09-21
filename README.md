# AXPbox Alpha emulator

AXPbox is a fork of the discontinued es40 emulator. It can be used for running any operating system that runs on the Tru64 PALcode (e.g. OpenVMS, Tru64 UNIX, Linux, NetBSD).

The emulator supports SCSI, IDE, serial ports, Ethernet (using PCAP) and VGA graphics (using SDL).

## Usage

First invoke the interactive configuration file generator:
```
axpbox configure
```
This creates a file named es40.cfg, which you can now modify (the generator UI doesn't allow to set all options). After the configuration file and the required ROM image are ready, you can start the emulation:
```
axpbox run
```

## Changes in comparison with es40

- Renamed from es40 to AXPbox to avoid confusion with the physical machine (AlphaServer ES40)
- CMake is used for compilation instead of autotools
- OpenVMS host support was dropped
- es40 and es40_cfg were merged into one executable (axpbox)
