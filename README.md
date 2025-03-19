
# ufs-tools

## Introduction

`ufs-tools` is a collection of tools built for Universal Flash Storage (UFS). It contains scripts and UFS Command-Line Interface (CLI) programs.

## Directories

- `.`: Root directory
- `./scripts`: Useful scripts built for UFS
- `./ufs-cli`: UFS CLI programs source codes

## Scripts

The scripts are intended to be used on the host (PC) side.

### ufs-eom.py

`ufs-eom.py` exercises the UFS Eye Opening Monitor (EOM) and collects EOM logs. This script alone cannot activate UFS EOM; it requires the `lsufs` CLI program (located in `ufs-cli`) to send necessary UFS UIC commands and QUERY requests to complete the task.

`ufs-eom.py` eventually generates a report, such as `local/peer_lane_0/_1.eom`, which can be fed into `ufs-eom-plot.py` to generate UFS Eye diagrams.

`ufs-eom.py` uses ADB to interact with the `lsufs` program. If ADB is not available, you can use the standalone `ufseom` CLI program (in `ufs-cli`) instead. Note that `ufs-eom.py` does not conduct I/O transactions by itself; the user can stress the UFS links while `ufs-eom.py` is running.

**Important:** To obtain accurate EOM data, you should disable UFS driver low power mode features, such as Clock Scaling, Clock Gating, Suspend/Resume, and Auto Hibernate. For example:

```bash
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkscale_enable
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkgate_enable
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/auto_hibern8
```

Additionally, `ufs-eom.py` alters the UFS Host and/or UFS device UIC layer execution environments. Although UFS EOM is not supposed to interfere with normal I/O traffic, it is recommended to reboot the system after using `ufseom`.

For detailed usage of `ufs-eom.py`, try:

```bash
$ python ufs-eom.py
```

### ufs-eom-plot.py

`ufs-eom-plot.py` takes UFS EOM report files (with the `.eom` suffix) as input and plots UFS Eye diagrams.

For detailed usage of `ufs-eom-plot.py`, try:

```bash
$ python ufs-eom-plot.py
```

## ufs-cli

UFS CLI programs are intended to be used on the target device side.

### How to build

If cross-compilation is required, modify the Makefile manually.

```bash
$ cd ./ufs-cli
$ make
```

### lsufs

`lsufs` is a CLI program that sends commands to the UFS driver via the Linux Block SCSI Generic (BSG) framework.

For detailed usage of `lsufs`, refer to its help menus:

```bash
$ ./lsufs -h
$ ./lsufs uic -h
$ ./lsufs query -h
```

### ufseom

`ufseom` is a CLI program that exercises the UFS Eye Opening Monitor (EOM) and collects EOM data. Unlike `ufs-eom.py` (in `scripts`), `ufseom` is a standalone program and does not rely on the `lsufs` program. It is a more efficient and alternate choice to `ufs-eom.py` (in `scripts`), but both serve the same purpose.

`ufseom` eventually generates a report (in the path specified via the `--output` command line option), such as `local/peer_lane_0/_1.eom`, which can be fed into `ufs-eom-plot.py` to generate UFS Eye diagrams.

**Important:** To obtain accurate EOM data, you should disable UFS driver low power mode features, such as Clock Scaling, Clock Gating, Suspend/Resume, and Auto Hibernate. For example:

```bash
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkscale_enable
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/clkgate_enable
$ echo 0 > /sys/devices/<path to platform devices>/*.ufshc/auto_hibern8
```

Additionally, `ufseom` alters the UFS Host and/or UFS device UIC layer execution environments. Although UFS EOM is not supposed to interfere with normal I/O traffic, it is recommended to reboot the system after using `ufseom`.

For detailed usage of `ufseom`, refer to its help menu:

```bash
$ ./ufseom -h
```

## License

This project is licensed under the BSD-3-Clause-Clear license.
