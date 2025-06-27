# SPDX-License-Identifier: BSD-3-Clause-Clear
# Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.

import sys
import pdb
import csv
import pandas as pd
import seaborn as sns
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import matplotlib.colors as mcolors
from matplotlib.colors import ListedColormap

class ufs_eye_monitor_plot(object):
    def __init__(self):
        self.file_name = None
        self.fd = None
        self.file_data = None
        self.match_line_list = ['lane', 'timing', 'voltage', 'error count']

    def string_to_number(self, str_val):
        if str_val[0] == '-':
            num = int(str_val[1:].split(',')[0])
            num *= -1
        else:
            num = int(str_val.split(',')[0])
        return num

    def quit_with_err_msg(self, msg):
        print('ERROR: {:s}'.format(msg))
        exit(-1)

    def open_file(self, filename):
        self.file_name = filename
        self.fd = open(self.file_name, 'r')
        self.file_data = self.fd.readlines()

        self.lane0_data = dict()
        self.lane1_data = dict()
        self.timing_max_steps = 0
        self.timing_max_offset = 0
        self.timing_step = 0
        self.voltage_max_steps = 0
        self.voltage_max_offset = 0
        self.voltage_step = 0
        self.lane_list = []
        self.lane0_timing_list = []
        self.lane0_timing_list_adj = []
        self.lane0_timing_list_set = []
        self.lane0_voltage_list = []
        self.lane0_voltage_list_adj = []
        self.lane0_voltage_list_set = []
        self.lane0_error_count_list = []
        self.lane1_timing_list = []
        self.lane1_timing_list_adj = []
        self.lane1_timing_list_set = []
        self.lane1_voltage_list = []
        self.lane1_voltage_list_adj = []
        self.lane1_voltage_list_set = []
        self.lane1_error_count_list = []
        self.lane0_x_len = 0
        self.lane0_y_len = 0
        self.lane0_x_mid = 0
        self.lane0_y_mid = 0
        self.lane1_x_len = 0
        self.lane1_y_len = 0
        self.lane1_x_mid = 0
        self.lane1_y_mid = 0

        self.bad_data_count = 0
        self.line_no = 0

        self.ufs_version_no = ''
        self.ufs_device_mfr_name = ''
        self.ufs_device_mfr_id = ''
        self.ufs_device_size = ''
        self.ufs_gear = ''
        self.side = ''

        for line in self.file_data:
            self.line_no += 1
            line_list = line.split()
            if 'TimingMaxSteps' in line and 'TimingMaxOffset' in line:
                assert(line_list[0] == 'TimingMaxSteps')
                assert(line_list[2] == 'TimingMaxOffset')
                self.timing_max_steps = int(self.string_to_number(line_list[1]))
                self.timing_max_offset = int(self.string_to_number(line_list[3]))
                self.timing_step = (self.timing_max_offset * 0.01) / self.timing_max_steps
            if 'VoltageMaxSteps' in line and 'VoltageMaxOffset' in line:
                assert(line_list[0] == 'VoltageMaxSteps')
                assert(line_list[2] == 'VoltageMaxOffset')
                self.voltage_max_steps = int(self.string_to_number(line_list[1]))
                self.voltage_max_offset = int(self.string_to_number(line_list[3]))
                self.voltage_step = (self.voltage_max_offset * 10) / self.voltage_max_steps
            if 'UFS Spec Version:' in line:
                if line_list[4] == 'UFS' and line_list[5] == 'Spec' and line_list[6] == 'Version:':
                    self.ufs_version_no = 'UFS' + line_list[7]
            if 'UFS INQUIRY ID:' in line:
                if line_list[4] == 'UFS' and line_list[5] == 'INQUIRY' and line_list[6] == 'ID:':
                    self.ufs_device_mfr_name = line_list[7]
                    self.ufs_device_mfr_id = line_list[8]
            if 'UFS Total Size:' in line:
                if line_list[4] == 'UFS' and line_list[5] == 'Total' and line_list[6] == 'Size:':
                    self.ufs_device_size = line_list[7] + line_list[8]
            if 'UFS Speed: Gear' in line:
                if line_list[4] == 'UFS' and line_list[5] == 'Speed:' and line_list[6] == 'Gear':
                    self.ufs_gear = 'HSG{:s} {:s} {:s} {:s} {:s}'.format(line_list[7], line_list[8], line_list[9], line_list[10], line_list[11])
            if 'Side Eye Monitor Start' in line:
                if line_list[0] == 'UFS' and line_list[2] == 'Side' and line_list[3] == 'Eye' and line_list[4] == 'Monitor' and line_list[5] == 'Start':
                    self.side = line_list[1]
            for item in self.match_line_list:
                if item not in line:
                    continue
            if len(line_list) <= 7:
                continue
            if line_list[0] != 'lane' and line_list[2] != 'timing' and line_list[4] != 'voltage' and line_list[6] != 'error' and line_list[7] != 'count':
                continue

            try:
                lane_no = int(line_list[1])
                if lane_no not in self.lane_list:
                    self.lane_list.append(lane_no)
                timing = self.string_to_number(line_list[3])
                voltage = self.string_to_number(line_list[5])
                error_count = self.string_to_number(line_list[8])
            except:
                self.bad_data_count += 1
                print('Error on Line number {:d}: BAD Data.. plese check this below line in the log'.format(self.line_no))
                print(line)
                continue

            if lane_no == 0:
                self.lane0_timing_list.append(timing)
                self.lane0_voltage_list.append(voltage)
                self.lane0_error_count_list.append(error_count)
                key = 't#{:d}#v#{:d}'.format(timing, voltage)
                self.lane0_data[key] = error_count
            elif lane_no == 1:
                self.lane1_timing_list.append(timing)
                self.lane1_voltage_list.append(voltage)
                self.lane1_error_count_list.append(error_count)
                key = 't#{:d}#v#{:d}'.format(timing, voltage)
                self.lane1_data[key] = error_count
            else:
                self.quit_with_err_msg('wrong lane number on line {:d}'.format(self.line_no))

        if self.bad_data_count != 0:
            return
        if self.timing_step == 0:
            self.quit_with_err_msg('Missing this line: TimingMaxSteps <>, TimingMaxOffset <>')
        if self.voltage_step == 0:
            self.quit_with_err_msg('Missing this line: VoltageMaxSteps <>, VoltageMaxOffset <>')

        print('-' * 50)
        print('Parsed [{:d}] lines from the log file [{:s}]\n'.format(self.line_no, filename))
        print('-' * 50)

        #Lane-0 data tweaks
        self.lane0_timing_list_set = sorted(set(self.lane0_timing_list), key=int)
        self.lane0_voltage_list_set = sorted(set(self.lane0_voltage_list), key=int)
        self.lane0_x_len = len(self.lane0_timing_list_set)
        self.lane0_y_len = len(self.lane0_voltage_list_set)
        self.lane0_x_mid = len(self.lane0_timing_list_set) / 2
        self.lane0_y_mid = len(self.lane0_voltage_list_set) / 2

        for timing in self.lane0_timing_list:
            timing_adj = round((timing * self.timing_step), 4)
            self.lane0_timing_list_adj.append(timing_adj)

        for voltage in self.lane0_voltage_list:
            voltage_adj = round((voltage * self.voltage_step), 2)
            self.lane0_voltage_list_adj.append(voltage_adj)

        #Lane-1 data tweaks
        self.lane1_timing_list_set = sorted(set(self.lane1_timing_list), key=int)
        self.lane1_voltage_list_set = sorted(set(self.lane1_voltage_list), key=int)
        self.lane1_x_len = len(self.lane1_timing_list_set)
        self.lane1_y_len = len(self.lane1_voltage_list_set)
        self.lane1_x_mid = len(self.lane1_timing_list_set) / 2
        self.lane1_y_mid = len(self.lane1_voltage_list_set) / 2

        for timing in self.lane1_timing_list:
            timing_adj = round((timing * self.timing_step), 4)
            self.lane1_timing_list_adj.append(timing_adj)

        for voltage in self.lane1_voltage_list:
            voltage_adj = round((voltage * self.voltage_step), 2)
            self.lane1_voltage_list_adj.append(voltage_adj)

        for lane in self.lane_list:
            missing_data_count = self.validate_data(lane)
            if missing_data_count != 0:
                return
            self.plot_eye(lane)

        return

    def validate_data(self, lane_no):
        missing_data_count = 0
        assert((lane_no == 0) or (lane_no == 1))
        if lane_no == 0:
            lane_data = self.lane0_data
            lane_timing_list = self.lane0_timing_list
            lane_voltage_list = self.lane0_voltage_list
            lane_error_count_list = self.lane0_error_count_list
            min_time = self.lane0_timing_list_set[0]
            max_time = self.lane0_timing_list_set[-1]
            min_volt = self.lane0_voltage_list_set[0]
            max_volt = self.lane0_voltage_list_set[-1]
        elif lane_no == 1:
            lane_data = self.lane1_data
            lane_timing_list = self.lane1_timing_list
            lane_voltage_list = self.lane1_voltage_list
            lane_error_count_list = self.lane1_error_count_list
            min_time = self.lane1_timing_list_set[0]
            max_time = self.lane1_timing_list_set[-1]
            min_volt = self.lane1_voltage_list_set[0]
            max_volt = self.lane1_voltage_list_set[-1]
        else:
            print('wrong lane_no')
            return

        max_volt += 1
        for time in range(min_time, max_time):
            for volt in range(min_volt, max_volt):
                key = 't#{:d}#v#{:d}'.format(time, volt)
                try:
                    error_count = lane_data[key]
                except KeyError:
                    missing_data_count += 1
                    print('Log file is missing data. please check')
                    print('Lane={:d} Timing={:d} Voltage={:d}'.format(lane_no, time, volt))
        return missing_data_count

    def plot_eye(self, lane_no):
        assert((lane_no == 0) or (lane_no == 1))
        if lane_no == 0:
            lane_title = 'Lane 0'
            lane_timing_list_adj = self.lane0_timing_list_adj
            lane_voltage_list_adj = self.lane0_voltage_list_adj
            lane_error_count_list = self.lane0_error_count_list
            x_len = self.lane0_x_len
            y_len = self.lane0_y_len
            x_mid = self.lane0_x_mid
            y_mid = self.lane0_y_mid
            lane_timing_list_set = self.lane0_timing_list_set
            lane_voltage_list_set = self.lane0_voltage_list_set
        elif lane_no == 1:
            lane_title = 'Lane 1'
            lane_timing_list_adj = self.lane1_timing_list_adj
            lane_voltage_list_adj = self.lane1_voltage_list_adj
            lane_error_count_list = self.lane1_error_count_list
            x_len = self.lane1_x_len
            y_len = self.lane1_y_len
            x_mid = self.lane1_x_mid
            y_mid = self.lane1_y_mid
            lane_timing_list_set = self.lane1_timing_list_set
            lane_voltage_list_set = self.lane1_voltage_list_set
        else:
            print('wrong lane_no')
            return

        if x_len <= 34:
            eye_width = 5
        elif x_len <= 66:
            eye_width = 10
        elif x_len <= 129:
            eye_width = 20
        else:
            eye_width = 0

        if y_len <= 50:
            eye_height = 5
        elif y_len <= 130:
            eye_height = 11
        elif y_len <= 201:
            eye_height = 14
        else:
            eye_height = 0

        fig, ax1 = plt.subplots(sharex=True, sharey=True)

        df0 = pd.DataFrame(list(zip(lane_timing_list_adj, lane_voltage_list_adj, lane_error_count_list)), columns=['UI', 'mV', 'count'])
        df1 = df0.pivot(index='mV', columns='UI', values='count')
        colors = ['#008000', '#FF0000', '#EE4040', '#DD2C2C', '#CD2626', '#8B3A3A', '#600000', '#000000']
        boundaries = [0, 1, 10, 20, 30, 40, 50, 60, 63]
        cmap = mcolors.LinearSegmentedColormap.from_list('custom_cmap', colors, N=256)
        norm = mcolors.BoundaryNorm(boundaries=boundaries, ncolors=256)
        ax2 = sns.heatmap(df1, annot=False, cmap=cmap, ax=ax1, norm=norm)

        if self.ufs_device_mfr_name != '':
            title_1 = self.side + ' ' + lane_title + ' : ' + self.ufs_device_mfr_name + ', ' + self.ufs_device_mfr_id + ' ' + self.ufs_device_size + ' ' + self.ufs_version_no + ' '  + self.ufs_gear
        else:
            title_1 = self.side + ' ' + lane_title + ' : < UFS Device information missing in the log > '

        if self.timing_step != 0:
            title_2 = 'X-axis  : Timing : max_steps={:d}, max_offset={:d}, step_size={:f}, value_range=[{:d} to {:d}, {:d}]'.format(
            self.timing_max_steps, self.timing_max_offset, self.timing_step, lane_timing_list_set[0], lane_timing_list_set[-1],
            len(lane_timing_list_set))
        else:
            title_2 = 'X-axis  : Timing : Timing Information is missing in the log'

        if self.voltage_step != 0:
            title_3 = 'Y-axis  : Voltage: max_steps={:d}, max_offset={:d}, step_size={:f}, value_range=[{:d} to {:d}, {:d}]'.format(
            self.voltage_max_steps, self.voltage_max_offset, self.voltage_step, lane_voltage_list_set[0], lane_voltage_list_set[-1],
            len(lane_voltage_list_set))
        else:
            title_2 = 'Y-axis  : Voltage: Voltage Information is missing in the log'

        title = title_1 + '\n' + title_2 + '\n' + title_3
        ax2.set_title(title, fontsize=17, fontweight='bold', loc='center')
        ax2.set_xlabel('UI', fontsize=14, fontweight='bold')
        ax2.set_ylabel('mV', fontsize=14, fontweight='bold')
        ax2.xaxis.label.set_color('blue')
        ax2.invert_yaxis()
        ax2.yaxis.label.set_color('green')

        plt.show()

def print_usage():
    print('-' * 60)
    print('{:s} <your_eom_report.eom>'.format(sys.argv[0]))
    print('-' * 60)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print_usage()
        quit()
    plot_inst = ufs_eye_monitor_plot()
    plot_inst.open_file(sys.argv[1])