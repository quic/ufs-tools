# SPDX-License-Identifier: BSD-3-Clause-Clear
# Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.

import getopt
import sys
import re
import subprocess
import datetime

EomVersion = 1.0
Reporter = None
TimingMaxSteps = 0
VoltageMaxSteps = 0

class LsufsCli(object):
    def __init__(self):
        self.lsufs_path = None
        self.device_path = None
        self.side_opt = "local"

    def query_desc(self, idn, index):
        command = f"{self.lsufs_path} query -o 1 -i {idn} -I {index} -s 0 -d {self.device_path}"
        return subprocess.check_output(["adb", "shell"] + command.split()).decode("utf-8")

    def uic_set(self, lane, index, value, selec_direc, side_override=None):
        lane_opt = f"--lane {lane}" if lane is not None else ""
        side_opt = f"--{side_override}" if side_override is not None else self.side_opt
        command = f"adb shell {self.lsufs_path} uic -s {hex(value)} {side_opt} {lane_opt} -i {hex(index)} --{selec_direc} -d {self.device_path}"
        subprocess.run(command, capture_output=True, text=True)

    def uic_get(self, lane, index, selec_direc, side_override=None):
        lane_opt = f"--lane {lane}" if lane is not None else ""
        side_opt = f"--{side_override}" if side_override is not None else self.side_opt
        command = f"{self.lsufs_path} uic -g {side_opt} {lane_opt} -i {hex(index)} --{selec_direc} -d {self.device_path}"
        output = subprocess.check_output(["adb", "shell"] + command.split()).decode("utf-8")
        match = re.search(r'(?<= = 0x)[0-9A-Za-z]+', output)
        return int(match.group(), 16) if match else None


class EomMisc(object):
    def __init__(self, lsufs_cli):
        self.eom_misc_lsufs = lsufs_cli

    def get_device_info(self):
        global Reporter
        manufacturer_name = ''
        product_name = ''
        product_revision = ''

        # Read device descriptor
        device_descriptor = self.eom_misc_lsufs.query_desc(0, 0)

        i_manufacturer_name_index = int(re.search(r'(?<=Offset 0x14 : 0x)[0-9A-Za-z]+', device_descriptor).group(), 16)
        i_product_name_index = int(re.search(r'(?<=Offset 0x15 : 0x)[0-9A-Za-z]+', device_descriptor).group(), 16)
        i_product_revision_index = int(re.search(r'(?<=Offset 0x2a : 0x)[0-9A-Za-z]+', device_descriptor).group(), 16)

        # Read Manufacturer Name String Descriptor
        manufacturer_desc = self.eom_misc_lsufs.query_desc(5, i_manufacturer_name_index)
        manufacturer_name = self.parse_desc(manufacturer_desc, 8)
        # Read Product Name String Descriptor
        product_desc = self.eom_misc_lsufs.query_desc(5, i_product_name_index)
        product_name = self.parse_desc(product_desc, 16)
        # Read Product Revision Level String Descriptor
        product_desc = self.eom_misc_lsufs.query_desc(5, i_product_revision_index)
        product_revision = self.parse_desc(product_desc, 4)

        print(f'- - - - UFS INQUIRY ID: {manufacturer_name} {product_name} {product_revision}', file=Reporter)
        print(f'- - - - UFS INQUIRY ID: {manufacturer_name} {product_name} {product_revision}')

    def get_eom_caps(self, lane):
        global Reporter, TimingMaxSteps, VoltageMaxSteps
        # Read capabilities
        TimingMaxSteps = self.eom_misc_lsufs.uic_get(lane, 0xf2, "RX")
        TimingMaxOffset = self.eom_misc_lsufs.uic_get(lane, 0xf3, "RX")
        VoltageMaxSteps = self.eom_misc_lsufs.uic_get(lane, 0xf4, "RX")
        VoltageMaxOffset = self.eom_misc_lsufs.uic_get(lane, 0xf5, "RX")

        print("EOM Capabilities:", file=Reporter)
        print(f"TimingMaxSteps {TimingMaxSteps}, TimingMaxOffset {TimingMaxOffset}", file=Reporter)
        print(f"VoltageMaxSteps {VoltageMaxSteps}, VoltageMaxOffset {VoltageMaxOffset}", file=Reporter)

    def determine_lanes(self, lane):
        lane_num_max = 2

        if lane is None:
            return lane_num_max, 0, "0_1"
        elif lane > (lane_num_max - 1):
            print("Invalid Lane number")

        return lane + 1, lane, str(lane)

    def determine_voltage(self, voltage):
        global Reporter
        if voltage is None:
            return VoltageMaxSteps, False
        if abs(voltage) > VoltageMaxSteps:
            print("Invalid input for voltage")
            Reporter.close()
            sys.exit(2)
        return voltage, True

    def parse_desc(self, desc, length):
        result = ''
        for i in range(length):
            offset = 2 * i + 3
            hex_offset = hex(offset)
            match = re.search(rf'(?<=Offset {hex_offset} : 0x)[0-9A-Za-z]+', desc)
            if match and match.group() != '0':
                result += chr(int(match.group(), 16))
        return result


class UFSEOM(object):
    def __init__(self):
        self.side = None
        self.lanes = None
        self.start_lane = None
        self.voltage_max = None
        self.single_voltage = False
        self.target_test_count = None
        self.EomReport = None
        self.lsufs = LsufsCli()
        self.misc = EomMisc(self.lsufs)

    def eom_prepare(self, argv):
        global Reporter
        target_test_count_default = 0x5D
        lane = None
        voltage = None
        lsufs_path = None
        device_path = None

        print('Command line input:', argv)

        try:
            options, args = getopt.getopt(argv, '', ['side=', 'lane=', 'voltage=', 'target=', 'lsufs_path=', 'device_path='])
        except getopt.GetoptError:
            print_usage()
            sys.exit(2)

        for opt, arg in options:
            if opt == "--side":
                self.side = arg
            elif opt == "--lane":
                lane = int(arg)
            elif opt == "--voltage":
                voltage = int(arg)
            elif opt == "--target":
                self.target_test_count = int(arg)
            elif opt == "--lsufs_path":
                lsufs_path = arg
            elif opt == "--device_path":
                device_path = arg

        if not self.side:
            print("ERROR: --side not given")
            sys.exit(2)

        if not lsufs_path:
            print("ERROR: --lsufs_path not given")
            sys.exit(2)

        if not device_path:
            print("ERROR: --device_path not given")
            sys.exit(2)

        if not check_path(lsufs_path):
            print("ERROR: invalid input for --lsufs_path")
            sys.exit(2)

        if not check_path(device_path):
            print("ERROR: invalid input for --device_path")
            sys.exit(2)

        self.lsufs.lsufs_path = lsufs_path
        self.lsufs.device_path = device_path

        if self.side != "local" and self.side != "peer":
            print("Invalid input for --side, expecting 'local' or 'peer'")
            sys.exit(2)

        self.lsufs.side_opt = f"--{self.side}"

        if self.target_test_count is None:
            self.target_test_count = target_test_count_default
        elif not (0 < self.target_test_count <= 127):
            print("Invalid input for --target, expecting 1 to 127")
            sys.exit(2)

        self.lanes, self.start_lane, lane_string = self.misc.determine_lanes(lane)

        self.EomReport = f"{self.side}_lane_{lane_string}_ttc_{self.target_test_count}.eom"
        Reporter = open(self.EomReport, 'w+')

        self.misc.get_device_info()
        self.misc.get_eom_caps(self.start_lane)

        self.voltage_max, self.single_voltage = self.misc.determine_voltage(voltage)

    def eom_start(self):
        global Reporter, TimingMaxSteps, VoltageMaxSteps
        start_time = datetime.datetime.now()

        side = "Host" if self.side == "local" else "Device"
        print("Start EOM Scan...")
        print(f"UFS {side} Side Eye Monitor Start", file = Reporter)
        for lane in range(self.start_lane, self.lanes):
            # Enable Eye Monitor Test control register.
            self.lsufs.uic_set(lane, 0xf6, 0x1, "RX")

            if not self.single_voltage:
                for timing in range(-TimingMaxSteps, TimingMaxSteps + 1):
                    for voltage in range(-self.voltage_max, self.voltage_max + 1):
                        self.eom_scan(lane, timing, voltage, self.target_test_count)
            else:
                for timing in range(-TimingMaxSteps, TimingMaxSteps + 1):
                    self.eom_scan(lane, timing, self.voltage_max, self.target_test_count)
            # Disable Eye Monitor Test control register.
            self.lsufs.uic_set(lane, 0xf6, 0x0, "RX")

        end_time = datetime.datetime.now()
        spent_time = end_time - start_time
        print(f"EOM Scan Finished!\nTime elapsed: {spent_time}")
        print(f"EOM Scan Finished!\nTime elapsed: {spent_time}", file = Reporter)
        Reporter.close()
        print(f"EOM results saved to {self.EomReport}")

    def eom_scan(self, lane, timing, voltage, target_test_count):
        global Reporter
        direction_bit = 6
        step_mask = 0x3f
        err_cnt_threshold = 60
        # Calculate timing and voltage values to config UFS EOM
        # Bit[5:0]: 0 to 63
        # Bit[6]: 0 - Right/Plus Direction; 1 - Left/Minus Direction
        voltage_config = self.calculate_config(voltage, direction_bit, step_mask)
        timing_config = self.calculate_config(timing, direction_bit, step_mask)

        self.config_eom(lane, timing_config, voltage_config, target_test_count)

        while True:
            # Read RX_EYEMON_Start
            eyemon_start = self.lsufs.uic_get(lane, 0xfc, "RX")
            if eyemon_start == 1:
                continue
            elif eyemon_start == 0:
                # EOM stops, read the result
                test_count = self.lsufs.uic_get(lane, 0xfa, "RX")
                err_count = self.lsufs.uic_get(lane, 0xfb, "RX")
                if test_count is not None and err_count is not None:
                    if test_count >= target_test_count or err_count >= err_cnt_threshold:
                        print('lane:', lane, 'timing:', timing, 'voltage:', voltage, 'error count:', err_count, file=Reporter)
                        break
                else:
                    print("Failed to get vaild RX_EYEMON_Tested_Count or RX_EYEMON_Error_Count")
                    break
            else:
                print("Failed to get RX_EYEMON_Start")
                break

    def config_eom(self, lane, timing_config, voltage_config, target_test_count_config):
        # Enable Eye Monitor Test control register
        self.lsufs.uic_set(lane, 0xf6, 0x1, "RX")
        # Config Eye Monitor timing register
        self.lsufs.uic_set(lane, 0xf7, timing_config, "RX")
        # Config Eye Monitor voltage register
        self.lsufs.uic_set(lane, 0xf8, voltage_config, "RX")
        # Config Eye Monitor target test count
        self.lsufs.uic_set(lane, 0xf9, target_test_count_config, "RX")
        # Select NO_ADAPT
        self.lsufs.uic_set(None, 0x15d4, 0x3, "TX", "local")
        # Do a Power Mode Change to Fast Mode to apply NO_ADAPT and also trigger a RCT to kick start EOM
        self.lsufs.uic_set(None, 0x1571, 0x11, "TX", "local")

    def calculate_config(self, value, direction_bit, step_mask):
        direction = 1 if value < 0 else 0
        config = (direction << direction_bit) | (abs(value) & step_mask)
        return config


def print_usage():
    print()
    print('{:s} --side=local/peer [--lane=0/1] [--voltage=<voltage value>] [--target=<target test count>] --lsufs_path=<path to lsufs> --device_path=<path to the UFS BSG device node>'.format(sys.argv[0]))
    print()
    print("--version: UFS EOM version")
    print("--help: show this help menu")
    print("--side: 'local' or 'peer'")
    print("--lane: lane no. 0 or 1, collect EOM data for all connected lanes if not given")
    print("--voltage: collect EOM data for this voltage only")
    print("--target: target test count")
    print("--lsufs_path: path to the lsufs executable CLI program on target device")
    print("--device_path: path to the UFS BSG device node, e.g., /dev/ufs-bsg0")
    print()


def check_path(path):
    command = f"adb shell ls {path}"
    output = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if output.returncode != 0:
        print(f"Invalid path: {path}!")
        return False
    return True


if __name__ == "__main__":

    if sys.argv[1] == '--version':
        print('ufseom version:', EomVersion)
        sys.exit(2)

    if sys.argv[1] == '--help':
        print_usage()
        sys.exit(2)

    if len(sys.argv) < 4:
        print_usage()
        sys.exit(2)

    eom_inst = UFSEOM()
    eom_inst.eom_prepare(sys.argv[1:])
    eom_inst.eom_start()
