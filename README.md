# ufs-tools

## Introduction

ufs-tools is a collection of tools built for Universal Flash Storage (UFS). ufs-tools contains
scripts and UFS Command-Line Interface (CLI) programs.


## Directories

.		root dir
./scripts	useful scripts built for UFS
./ufs-cli	UFS CLI programs source codes


## Scripts

Scripts are expected to be used on the host (PC) side.

### ufs-eom.py

ufs-eom.py exercises UFS Eye Opening Monitor (EOM) and collects EOM logs. ufs-eom.py alone cannot
get UFS EOM to work, it actually demands the lsufs CLI program (in ufs-cli) to send necessary UFS
UIC commands and QUERY requests to fulfill the mission.

ufs-eom.py eventually generates a report, something like local/peer_lane_0/_1.eom, which can be fed
to ufs-eom-plot.py to plot out the UFS Eye diagrams.

ufs-eom.py uses ADB to interact with the lsufs program. If ADB is not available, one can use the
standalone ufseom CLI program (in ufs-cli) instead. ufs-eom.py does not conduct I/O transactions on
its own, user can stress the UFS links while ufs-eom.py is running.

Note that to get accurate EOM data, user should disable UFS driver low power mode features, such as
Clock Scaling, Clock Gating, Suspend/Resume and Auto Hibernate. For example:
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkscale_enable
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkgate_enable
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/auto_hibern8

In addition, ufs-eom.py changes UFS Host and/or UFS device UIC layer execution environments,
although UFS EOM is not supposed to disturb normal I/O traffics, it is recommanded to reboot the
system after use ufseom.

For detailed usage of ufs-eom.py, try

$ python ufs-eom.py

### ufs-eom-plot.py

ufs-eom-plot.py feeds on the UFS EOM report files (come with suffix *.eom) as input to plot out UFS
Eye diagrams.

For detailed usage of ufs-eom-plot.py, try

$ python ufs-eom-plot.py


## ufs-cli

UFS CLI programs are expected to be used on the target device side.

### How to build

If cross compilation is required, please modify Makefile manually.

$ cd ./ufs-cli
$ make

### lsufs

lsufs is a CLI program to send commands to UFS driver via Linux Block SCSI Generic (BSG) famework.

The detailed usage of lsufs can be found in its help menus:

$ ./lsufs -h
$ ./lsufs uic -h
$ ./lsufs query -h

### ufseom

ufseom is a CLI program to exercise the UFS Eye Opening Monitor (EOM) and collect EOM data. Unlike
the ufs-eom.py (in scripts), ufseom is a standalone program which does not leverage lsufs program.
ufseom is an alternate (better and more efficient) choice to ufs-eom.py (in scripts), but both serve
the same purpose.

ufseom eventually generates a report (in the path given via --output command line option), something
like local/peer_lane_0/_1.eom, which can be fed to ufs-eom-plot.py to plot out the UFS Eye diagrams.

Note that to get accurate EOM data, user should disable UFS driver low power mode features, such as
Clock Scaling, Clock Gating, Suspend/Resume and Auto Hibernate. For example:
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkscale_enable
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkgate_enable
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/auto_hibern8

In addition, ufseom changes UFS Host and/or UFS device UIC layer execution environments, although
UFS EOM is not supposed to disturb normal I/O traffics, it is recommanded to reboot the system after
use ufseom.

The detailed usage of ufseom can be found in its help menu:

$ ./ufseom -h


## License

This project is license under the BSD-3-Clause-Clear license.
