# AXPbox Alpha emulator

AXPbox is a fork of the discontinued es40 emulator. It could theoretically used for running any operating system that runs on the OpenVMS or Tru64 PALcode (e.g. OpenVMS, Tru64 UNIX, Linux, NetBSD), however as of now only OpenVMS can be installed (for more details see [Guest support](https://github.com/lenticularis39/axpbox/wiki/Guest-support)).

The emulator supports SCSI, IDE, serial ports, Ethernet (using PCAP) and [VGA graphics](https://github.com/lenticularis39/axpbox/wiki/VGA) (using SDL).

## Usage

First invoke the interactive configuration file generator:
```
axpbox configure
```
This creates a file named es40.cfg, which you can now modify (the generator UI doesn't allow to set all options). After the configuration file and the required ROM image are ready, you can start the emulation:
```
axpbox run
```

Please read the [Installation Guide](https://github.com/lenticularis39/axpbox/wiki/OpenVMS-installation-guide) for information to get OpenVMS installed in the emulator.

## Changes in comparison with es40

- Renamed from es40 to AXPbox to avoid confusion with the physical machine (AlphaServer ES40)
- CMake is used for compilation instead of autotools
- OpenVMS host support was dropped
- es40 and es40_cfg were merged into one executable (axpbox)
- The code was cleaned to compile without warnings on most compilers
- Code modernizing, replacing POCO framework parts by native C++ counterparts not available in 2008 (std::threads, etc)
- Incorporate various patches from other es40 forks, for example, added MC146818 periodic interrupt to allow netbsd to boot and install, skip_memtest for faster booting.
- Bug fixes, less Access Violations, less Segfaults, overal less crashes. 
- [More](https://github.com/lenticularis39/axpbox/wiki/] documentation and usage information on the various features and operating systems 
