#!/usr/bin/env python3
"""Pixel-exact OLED 128x64 Simulation and Rendering Engine"""

import os
import re
import subprocess
import tempfile
from PIL import Image

FONT_5X7 = [[0, 0, 0, 0, 0], [0, 0, 95, 0, 0], [0, 7, 0, 7, 0], [20, 127, 20, 127, 20], [36, 42, 127, 42, 18], [35, 19, 8, 100, 98], [54, 73, 85, 34, 80], [0, 5, 3, 0, 0], [0, 28, 34, 65, 0], [0, 65, 34, 28, 0], [8, 42, 28, 42, 8], [8, 8, 62, 8, 8], [0, 80, 48, 0, 0], [8, 8, 8, 8, 8], [0, 96, 96, 0, 0], [32, 16, 8, 4, 2], [62, 81, 73, 69, 62], [0, 66, 127, 64, 0], [66, 97, 81, 73, 70], [33, 65, 69, 75, 49], [24, 20, 18, 127, 16], [39, 69, 69, 69, 57], [60, 74, 73, 73, 48], [1, 113, 9, 5, 3], [54, 73, 73, 73, 54], [6, 73, 73, 41, 30], [0, 54, 54, 0, 0], [0, 86, 54, 0, 0], [0, 8, 20, 34, 65], [20, 20, 20, 20, 20], [65, 34, 20, 8, 0], [2, 1, 81, 9, 6], [50, 73, 121, 65, 62], [126, 17, 17, 17, 126], [127, 73, 73, 73, 54], [62, 65, 65, 65, 34], [127, 65, 65, 34, 28], [127, 73, 73, 73, 65], [127, 9, 9, 1, 1], [62, 65, 65, 81, 50], [127, 8, 8, 8, 127], [0, 65, 127, 65, 0], [32, 64, 65, 63, 1], [127, 8, 20, 34, 65], [127, 64, 64, 64, 64], [127, 2, 4, 2, 127], [127, 4, 8, 16, 127], [62, 65, 65, 65, 62], [127, 9, 9, 9, 6], [62, 65, 81, 33, 94], [127, 9, 25, 41, 70], [70, 73, 73, 73, 49], [1, 1, 127, 1, 1], [63, 64, 64, 64, 63], [31, 32, 64, 32, 31], [127, 32, 24, 32, 127], [99, 20, 8, 20, 99], [3, 4, 120, 4, 3], [97, 81, 73, 69, 67], [0, 127, 65, 65, 0], [2, 4, 8, 16, 32], [0, 65, 65, 127, 0], [4, 2, 1, 2, 4], [64, 64, 64, 64, 64], [0, 1, 2, 4, 0], [32, 84, 84, 84, 120], [127, 72, 68, 68, 56], [56, 68, 68, 68, 32], [56, 68, 68, 72, 127], [56, 84, 84, 84, 24], [8, 126, 9, 1, 2], [8, 20, 84, 84, 60], [127, 8, 4, 4, 120], [0, 68, 125, 64, 0], [32, 64, 68, 61, 0], [127, 16, 40, 68, 0], [0, 65, 127, 64, 0], [124, 4, 24, 4, 120], [124, 8, 4, 4, 120], [56, 68, 68, 68, 56], [124, 20, 20, 20, 8], [8, 20, 20, 24, 124], [124, 8, 4, 4, 8], [72, 84, 84, 84, 32], [4, 63, 68, 64, 32], [60, 64, 64, 32, 124], [28, 32, 64, 32, 28], [60, 64, 48, 64, 60], [68, 40, 16, 40, 68], [12, 80, 80, 80, 60], [68, 100, 84, 76, 68], [0, 8, 54, 65, 0], [0, 0, 127, 0, 0], [0, 65, 54, 8, 0], [8, 8, 42, 28, 8]]

FONT_6X12 = [[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 32, 32, 32, 32, 32, 0, 32, 32, 0, 0], [0, 80, 80, 80, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 80, 80, 248, 80, 80, 248, 80, 80, 0, 0], [0, 0, 32, 112, 168, 160, 112, 40, 168, 112, 32, 0], [0, 0, 72, 168, 80, 16, 32, 40, 84, 72, 0, 0], [0, 0, 32, 80, 80, 32, 104, 144, 144, 104, 0, 0], [0, 32, 32, 32, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 16, 32, 64, 64, 64, 64, 32, 16, 0, 0], [0, 0, 64, 32, 16, 16, 16, 16, 32, 64, 0, 0], [0, 0, 0, 0, 80, 32, 248, 32, 80, 0, 0, 0], [0, 0, 0, 0, 32, 32, 248, 32, 32, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 32, 32, 64, 0], [0, 0, 0, 0, 0, 0, 248, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 32, 32, 0, 0], [0, 0, 8, 8, 16, 16, 32, 32, 64, 64, 0, 0], [0, 0, 112, 136, 152, 168, 200, 136, 136, 112, 0, 0], [0, 0, 32, 96, 32, 32, 32, 32, 32, 112, 0, 0], [0, 0, 112, 136, 136, 8, 16, 32, 64, 248, 0, 0], [0, 0, 112, 136, 8, 48, 8, 8, 136, 112, 0, 0], [0, 0, 8, 24, 40, 72, 136, 248, 8, 8, 0, 0], [0, 0, 248, 128, 128, 240, 8, 8, 136, 112, 0, 0], [0, 0, 112, 128, 128, 240, 136, 136, 136, 112, 0, 0], [0, 0, 248, 8, 8, 16, 16, 32, 32, 32, 0, 0], [0, 0, 112, 136, 136, 112, 136, 136, 136, 112, 0, 0], [0, 0, 112, 136, 136, 136, 120, 8, 8, 112, 0, 0], [0, 0, 0, 0, 32, 32, 0, 0, 32, 32, 0, 0], [0, 0, 0, 0, 32, 32, 0, 0, 32, 32, 64, 0], [0, 0, 0, 8, 16, 32, 64, 32, 16, 8, 0, 0], [0, 0, 0, 0, 248, 0, 0, 248, 0, 0, 0, 0], [0, 0, 0, 64, 32, 16, 8, 16, 32, 64, 0, 0], [0, 0, 112, 136, 136, 16, 32, 0, 32, 32, 0, 0], [0, 0, 112, 136, 152, 168, 168, 152, 128, 120, 0, 0], [0, 0, 112, 136, 136, 136, 248, 136, 136, 136, 0, 0], [0, 0, 240, 136, 136, 240, 136, 136, 136, 240, 0, 0], [0, 0, 112, 136, 128, 128, 128, 128, 136, 112, 0, 0], [0, 0, 224, 144, 136, 136, 136, 136, 144, 224, 0, 0], [0, 0, 248, 128, 128, 240, 128, 128, 128, 248, 0, 0], [0, 0, 248, 128, 128, 240, 128, 128, 128, 128, 0, 0], [0, 0, 112, 136, 128, 128, 184, 136, 136, 112, 0, 0], [0, 0, 136, 136, 136, 248, 136, 136, 136, 136, 0, 0], [0, 0, 112, 32, 32, 32, 32, 32, 32, 112, 0, 0], [0, 0, 56, 16, 16, 16, 16, 144, 144, 96, 0, 0], [0, 0, 136, 144, 160, 192, 192, 160, 144, 136, 0, 0], [0, 0, 128, 128, 128, 128, 128, 128, 128, 248, 0, 0], [0, 0, 136, 216, 168, 168, 136, 136, 136, 136, 0, 0], [0, 0, 136, 136, 200, 168, 152, 136, 136, 136, 0, 0], [0, 0, 112, 136, 136, 136, 136, 136, 136, 112, 0, 0], [0, 0, 240, 136, 136, 136, 240, 128, 128, 128, 0, 0], [0, 0, 112, 136, 136, 136, 136, 136, 168, 112, 8, 0], [0, 0, 240, 136, 136, 136, 240, 160, 144, 136, 0, 0], [0, 0, 112, 136, 128, 112, 8, 8, 136, 112, 0, 0], [0, 0, 248, 32, 32, 32, 32, 32, 32, 32, 0, 0], [0, 0, 136, 136, 136, 136, 136, 136, 136, 112, 0, 0], [0, 0, 136, 136, 136, 80, 80, 80, 32, 32, 0, 0], [0, 0, 136, 136, 136, 136, 168, 168, 216, 136, 0, 0], [0, 0, 136, 136, 80, 32, 32, 80, 136, 136, 0, 0], [0, 0, 136, 136, 80, 80, 32, 32, 32, 32, 0, 0], [0, 0, 248, 8, 16, 32, 64, 128, 128, 248, 0, 0], [0, 0, 112, 64, 64, 64, 64, 64, 64, 112, 0, 0], [0, 0, 64, 64, 32, 32, 16, 16, 8, 8, 0, 0], [0, 0, 112, 16, 16, 16, 16, 16, 16, 112, 0, 0], [0, 32, 80, 136, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 248, 0], [64, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 112, 8, 120, 136, 136, 120, 0, 0], [0, 0, 128, 128, 240, 136, 136, 136, 136, 240, 0, 0], [0, 0, 0, 0, 112, 136, 128, 128, 136, 112, 0, 0], [0, 0, 8, 8, 120, 136, 136, 136, 136, 120, 0, 0], [0, 0, 0, 0, 112, 136, 248, 128, 128, 120, 0, 0], [0, 0, 24, 32, 112, 32, 32, 32, 32, 32, 0, 0], [0, 0, 0, 0, 120, 136, 136, 136, 136, 120, 8, 112], [0, 0, 128, 128, 240, 136, 136, 136, 136, 136, 0, 0], [0, 32, 32, 0, 96, 32, 32, 32, 32, 112, 0, 0], [0, 8, 8, 0, 24, 8, 8, 8, 8, 8, 72, 48], [0, 0, 64, 64, 72, 80, 96, 96, 80, 72, 0, 0], [0, 0, 96, 32, 32, 32, 32, 32, 32, 112, 0, 0], [0, 0, 0, 0, 240, 168, 168, 168, 168, 168, 0, 0], [0, 0, 0, 0, 240, 136, 136, 136, 136, 136, 0, 0], [0, 0, 0, 0, 112, 136, 136, 136, 136, 112, 0, 0], [0, 0, 0, 0, 240, 136, 136, 136, 136, 240, 128, 128], [0, 0, 0, 0, 120, 136, 136, 136, 136, 120, 8, 8], [0, 0, 0, 0, 184, 192, 128, 128, 128, 128, 0, 0], [0, 0, 0, 0, 120, 128, 112, 8, 8, 240, 0, 0], [0, 0, 32, 32, 112, 32, 32, 32, 32, 24, 0, 0], [0, 0, 0, 0, 136, 136, 136, 136, 136, 120, 0, 0], [0, 0, 0, 0, 136, 136, 80, 80, 32, 32, 0, 0], [0, 0, 0, 0, 136, 136, 168, 168, 168, 112, 0, 0], [0, 0, 0, 0, 136, 80, 32, 32, 80, 136, 0, 0], [0, 0, 0, 0, 136, 136, 136, 136, 136, 120, 8, 112], [0, 0, 0, 0, 248, 16, 32, 64, 128, 248, 0, 0], [0, 0, 24, 32, 32, 64, 32, 32, 32, 24, 0, 0], [0, 0, 32, 32, 32, 32, 32, 32, 32, 32, 0, 0], [0, 0, 96, 16, 16, 8, 16, 16, 16, 96, 0, 0], [0, 0, 0, 0, 0, 72, 168, 144, 0, 0, 0, 0]]

FONT_12X20 = [[0, 252, 254, 7, 7, 7, 7, 7, 7, 254, 252, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 63, 127, 224, 224, 224, 224, 224, 224, 127, 63, 0], [0, 16, 24, 12, 6, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0, 192, 192, 192, 255, 255, 192, 192, 192, 0, 0], [0, 4, 6, 7, 7, 7, 7, 7, 7, 254, 252, 0, 0, 0, 0, 0, 128, 224, 112, 28, 15, 3, 0, 0, 0, 248, 254, 238, 231, 225, 224, 224, 224, 224, 224, 0], [0, 2, 3, 3, 3, 3, 3, 3, 3, 255, 254, 0, 0, 0, 0, 0, 12, 12, 12, 12, 12, 253, 253, 0, 0, 60, 96, 224, 224, 224, 224, 224, 224, 127, 63, 0], [0, 0, 0, 128, 224, 120, 30, 7, 1, 255, 254, 0, 0, 248, 254, 135, 129, 128, 128, 128, 128, 255, 255, 0, 0, 1, 1, 1, 1, 1, 1, 1, 255, 255, 1, 0], [0, 255, 255, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 31, 31, 28, 28, 28, 28, 28, 28, 252, 248, 0, 0, 60, 96, 224, 224, 224, 224, 224, 224, 127, 63, 0], [0, 252, 254, 7, 7, 3, 3, 3, 3, 2, 0, 0, 0, 255, 255, 24, 24, 24, 24, 24, 24, 248, 240, 0, 0, 63, 127, 224, 224, 224, 224, 224, 224, 127, 63, 0], [0, 7, 7, 7, 7, 7, 7, 7, 199, 255, 63, 0, 0, 0, 0, 0, 0, 128, 240, 126, 15, 1, 0, 0, 0, 0, 128, 224, 124, 31, 3, 0, 0, 0, 0, 0], [0, 252, 254, 3, 3, 3, 3, 3, 3, 254, 252, 0, 0, 227, 255, 28, 28, 28, 28, 28, 28, 255, 227, 0, 0, 63, 127, 224, 224, 224, 224, 224, 224, 127, 63, 0], [0, 252, 254, 3, 3, 3, 3, 3, 3, 254, 252, 0, 0, 7, 31, 24, 24, 24, 24, 24, 24, 255, 255, 0, 0, 0, 64, 192, 192, 224, 224, 224, 224, 127, 63, 0], [0, 0, 192, 192, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 193, 193, 193, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 60, 60, 60, 0, 0, 0, 0, 0, 0], [0, 124, 68, 68, 124, 0, 0, 0, 128, 96, 28, 0, 0, 0, 0, 0, 128, 96, 24, 6, 1, 0, 0, 0, 0, 32, 24, 6, 1, 0, 0, 62, 34, 34, 62, 0], [0, 252, 254, 7, 7, 7, 7, 7, 7, 7, 6, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 63, 127, 224, 224, 224, 224, 224, 224, 224, 96, 0], [0, 0, 62, 34, 34, 34, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]]

ICONS_16X16 = {'processor': [0, 0, 10, 80, 10, 80, 0, 0, 111, 246, 15, 240, 111, 246, 15, 240, 15, 240, 111, 246, 15, 240, 111, 246, 0, 0, 10, 80, 10, 80, 0, 0], 'computer_fan': [0, 0, 31, 248, 63, 252, 123, 30, 114, 110, 106, 198, 109, 190, 103, 206, 115, 230, 125, 182, 99, 86, 118, 78, 120, 222, 63, 252, 31, 248, 0, 0], 'thermometer_alt': [0, 0, 0, 0, 3, 192, 2, 64, 6, 96, 6, 96, 7, 224, 7, 224, 7, 224, 5, 160, 5, 160, 7, 224, 5, 160, 3, 192, 0, 0, 0, 0], 'ssd': [0, 0, 15, 240, 31, 248, 31, 248, 30, 120, 30, 120, 30, 120, 30, 120, 30, 120, 31, 248, 31, 248, 31, 248, 24, 24, 30, 248, 2, 192, 0, 0], 'ram_alt': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 63, 252, 127, 254, 37, 164, 63, 252, 127, 254, 63, 252, 63, 188, 0, 0, 0, 0, 0, 0, 0, 0], 'hdd': [0, 0, 15, 240, 31, 248, 30, 120, 24, 24, 24, 24, 16, 8, 17, 136, 16, 8, 24, 24, 27, 56, 26, 248, 29, 248, 31, 248, 15, 240, 0, 0], 'wi_fi_alt': [0, 0, 0, 0, 3, 192, 31, 248, 63, 252, 56, 28, 1, 128, 7, 224, 7, 224, 0, 0, 1, 128, 3, 192, 3, 192, 1, 128, 0, 0, 0, 0], 'clock': [0, 0, 0, 0, 7, 224, 31, 248, 31, 248, 63, 252, 63, 252, 63, 124, 63, 252, 63, 252, 63, 252, 31, 248, 31, 248, 7, 224, 0, 0, 0, 0], 'gauge': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 224, 15, 240, 16, 56, 32, 28, 32, 140, 33, 140, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0], 'bar_chart': [0, 0, 0, 0, 32, 0, 48, 0, 32, 0, 32, 96, 32, 96, 32, 96, 44, 96, 45, 96, 47, 96, 47, 96, 47, 104, 31, 252, 0, 0, 0, 0], 'lightning_bolt': [0, 0, 0, 0, 0, 64, 0, 192, 1, 192, 3, 128, 7, 128, 15, 240, 15, 240, 1, 224, 1, 192, 3, 128, 3, 0, 2, 0, 0, 0, 0, 0], 'arrow_down_alt': [0, 0, 0, 0, 1, 128, 1, 128, 1, 128, 1, 128, 1, 128, 33, 132, 49, 140, 25, 152, 13, 176, 7, 224, 3, 192, 1, 128, 0, 0, 0, 0], 'arrow_up_alt': [0, 0, 0, 0, 1, 128, 3, 192, 7, 224, 13, 176, 25, 152, 49, 140, 33, 132, 1, 128, 1, 128, 1, 128, 1, 128, 1, 128, 0, 0, 0, 0]}


from PIL import Image

WIDTH = 128
HEIGHT = 64

class OledCanvas:
    def __init__(self):
        self.fb = [0] * (WIDTH * HEIGHT)

    def clear(self):
        self.fb = [0] * (WIDTH * HEIGHT)

    def pixel(self, x, y, color=1):
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            self.fb[y * WIDTH + x] = 1 if color else 0

    def get_pixel(self, x, y):
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            return self.fb[y * WIDTH + x]
        return 0

    def line(self, x0, y0, x1, y1, color=1):
        dx = abs(x1 - x0)
        dy = -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            self.pixel(x0, y0, color)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def rect(self, x, y, w, h, fill=False, color=1):
        if fill:
            for r in range(y, y + h):
                for c in range(x, x + w):
                    self.pixel(c, r, color)
        else:
            self.line(x, y, x + w - 1, y, color)
            self.line(x, y + h - 1, x + w - 1, y + h - 1, color)
            self.line(x, y, x, y + h - 1, color)
            self.line(x + w - 1, y, x + w - 1, y + h - 1, color)

    def draw_char_small(self, x, y, char, color=1):
        ascii_code = ord(char)
        if 32 <= ascii_code <= 126:
            glyph = FONT_5X7[ascii_code - 32]
            for col_idx, col_byte in enumerate(glyph):
                for bit in range(7):
                    if (col_byte >> bit) & 1:
                        self.pixel(x + col_idx, y + bit, color)
        return 6

    def draw_string_small(self, x, y, text, color=1):
        cx = x
        for ch in text:
            self.draw_char_small(cx, y, ch, color)
            cx += 6
        return cx - x

    def draw_char_med(self, x, y, char, color=1):
        ascii_code = ord(char)
        if 32 <= ascii_code <= 126:
            glyph = FONT_6X12[ascii_code - 32]
            for row_idx, row_byte in enumerate(glyph):
                for bit in range(6):
                    if (row_byte >> (7 - bit)) & 1:
                        self.pixel(x + bit, y + row_idx, color)
        return 7

    def draw_string_med(self, x, y, text, color=1):
        cx = x
        for ch in text:
            self.draw_char_med(cx, y, ch, color)
            cx += 7
        return cx - x

    def draw_char_large(self, x, y, char, color=1):
        idx = -1
        if '0' <= char <= '9':
            idx = ord(char) - ord('0')
        elif char == ':': idx = 10
        elif char == '.': idx = 11
        elif char == '%': idx = 12
        elif char in ('C', 'c'): idx = 13
        elif char == '^': idx = 14  # degree
        if idx < 0:
            return 12

        glyph = FONT_12X20[idx]
        for page in range(3):
            for col in range(12):
                byte = glyph[page * 12 + col]
                for bit in range(8):
                    py = y + page * 8 + bit
                    if py < y + 24 and ((byte >> bit) & 1):
                        self.pixel(x + col, py, color)
        return 7 if char in (':', '.') else 12

    def draw_string_large(self, x, y, text, color=1):
        cx = x
        for ch in text:
            w = self.draw_char_large(cx, y, ch, color)
            cx += w + 1
        return cx - x

    def draw_icon(self, x, y, name, color=1, w=None, h=None):
        if name in CUSTOM_ICONS:
            info = CUSTOM_ICONS[name]
            bitmap = info["msb_bytes"]
            iw = info["w"]
            ih = info["h"]
            row_bytes = (iw + 7) // 8
            for r in range(ih):
                for c in range(iw):
                    byte_idx = r * row_bytes + (c // 8)
                    bit = 7 - (c % 8)
                    if (bitmap[byte_idx] >> bit) & 1:
                        self.pixel(x + c, y + r, color)
            return

        if name in ICONS_16X16:
            bitmap = ICONS_16X16[name]
            for r in range(16):
                for c in range(16):
                    byte_idx = r * 2 + (c // 8)
                    bit = 7 - (c % 8)
                    if (bitmap[byte_idx] >> bit) & 1:
                        self.pixel(x + c, y + r, color)

    def draw_bar(self, x, y, w, h, pct):
        self.rect(x, y, w, h, fill=False, color=1)
        fill_w = int((w - 2) * max(0.0, min(100.0, pct)) / 100.0)
        for r in range(y + 1, y + h - 1):
            for c in range(x + 1, x + 1 + fill_w):
                self.pixel(c, r, 1)

    def draw_equalizer(self, x, y, h, bands, peaks=None):
        num_bands = len(bands)
        bar_w = 2
        gap = 1
        for i in range(num_bands):
            bx = x + i * (bar_w + gap)
            val = max(0, min(h - 1, bands[i]))
            for r in range(y + h - 1 - val, y + h):
                for c in range(bx, bx + bar_w):
                    self.pixel(c, r, 1)
            if peaks and i < len(peaks):
                pk = max(0, min(h - 1, peaks[i]))
                if pk > val:
                    for c in range(bx, bx + bar_w):
                        self.pixel(c, y + h - 1 - pk, 1)

    def draw_sparkline(self, x, y, w, h, data, min_v=None, max_v=None):
        if len(data) < 2 or w < 2 or h < 2:
            return
        if min_v is None: min_v = min(data)
        if max_v is None: max_v = max(data)
        if max_v <= min_v: max_v = min_v + 1.0

        self.line(x, y + h - 1, x + w - 1, y + h - 1, 1)
        step = max(1, len(data) // w) if len(data) > w else 1
        prev_px = prev_py = None
        for i in range(0, len(data), step):
            px = x + (i * (w - 1)) // (len(data) - 1)
            val = max(min_v, min(max_v, data[i]))
            py = y + h - 1 - int(((val - min_v) / (max_v - min_v)) * (h - 2))
            if prev_px is not None:
                self.line(prev_px, prev_py, px, py, 1)
            prev_px, prev_py = px, py

    def to_ssd1306_bytes(self):
        buf = bytearray(1024)
        for page in range(8):
            for col in range(WIDTH):
                b = 0
                for bit in range(8):
                    y = page * 8 + bit
                    if self.fb[y * WIDTH + col]:
                        b |= (1 << bit)
                buf[page * WIDTH + col] = b
        return bytes(buf)

    def to_pil_image(self, scale=4, on_rgb=(0, 229, 255), off_rgb=(9, 18, 28)):
        img = Image.new('RGB', (WIDTH * scale, HEIGHT * scale), off_rgb)
        pixels = img.load()
        for y in range(HEIGHT):
            for x in range(WIDTH):
                if self.fb[y * WIDTH + x]:
                    for dy in range(scale - 1):
                        for dx in range(scale - 1):
                            pixels[x * scale + dx, y * scale + dy] = on_rgb
        return img

import re

def parse_xbm(text):
    w_m = re.search(r'#define\s+\w+_width\s+(\d+)', text)
    h_m = re.search(r'#define\s+\w+_height\s+(\d+)', text)
    w = int(w_m.group(1)) if w_m else 16
    h = int(h_m.group(1)) if h_m else 16
    raw = [int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', text)]
    return w, h, raw

def _add_image_methods():
    def draw_xbm(self, x, y, w, h, byte_data, color=1):
        row_bytes = (w + 7) // 8
        for r in range(h):
            for c in range(w):
                b_idx = r * row_bytes + (c // 8)
                bit = c % 8
                if b_idx < len(byte_data) and ((byte_data[b_idx] >> bit) & 1):
                    self.pixel(x + c, y + r, color)

    def draw_image(self, x, y, img_pil, threshold=128, color=1):
        mono = img_pil.convert('RGBA')
        w, h = mono.size
        for r in range(h):
            for c in range(w):
                pr, pg, pb, pa = mono.getpixel((c, r))
                if pa > 50:
                    lum = int(0.299 * pr + 0.587 * pg + 0.114 * pb)
                    if lum > threshold:
                        self.pixel(x + c, y + r, color)

    OledCanvas.draw_xbm = draw_xbm
    OledCanvas.draw_image = draw_image

_add_image_methods()

import re
import subprocess
import tempfile

CUSTOM_ICONS = {}

def parse_xbm_text(text, default_name="custom_icon"):
    w_m = re.search(r'#define\s+\w+_width\s+(\d+)', text)
    h_m = re.search(r'#define\s+\w+_height\s+(\d+)', text)
    if not w_m or not h_m:
        raise ValueError("Ungültige XBM Datei: width/height Definition fehlt.")
    w = int(w_m.group(1))
    h = int(h_m.group(1))
    
    name_m = re.search(r'static\s+(?:unsigned\s+)?char\s+(\w+)_bits', text)
    name = name_m.group(1) if name_m else default_name
    
    hex_values = re.findall(r'0x[0-9a-fA-F]+', text)
    xbm_bytes = [int(x, 16) for x in hex_values]
    
    row_bytes = (w + 7) // 8
    msb_bytes = []
    for r in range(h):
        for cb in range(row_bytes):
            idx = r * row_bytes + cb
            b_in = xbm_bytes[idx] if idx < len(xbm_bytes) else 0
            # Flip bit order: XBM is LSB-first, SSD1306 engine is MSB-first
            b_out = 0
            for bit in range(8):
                if (b_in >> bit) & 1:
                    b_out |= (1 << (7 - bit))
            msb_bytes.append(b_out)
            
    return name, w, h, msb_bytes, xbm_bytes

def generate_xbm_text(name, w, h, xbm_bytes):
    hex_strs = [f"0x{b:02x}" for b in xbm_bytes]
    lines = [
        f"#define {name}_width {w}",
        f"#define {name}_height {h}",
        f"static unsigned char {name}_bits[] = {{"
    ]
    row_bytes = (w + 7) // 8
    for r in range(h):
        chunk = hex_strs[r * row_bytes : (r + 1) * row_bytes]
        comma = "," if r < h - 1 else ""
        lines.append("   " + ", ".join(chunk) + comma)
    lines.append("};")
    return "\n".join(lines)

def convert_svg_to_bitmap(svg_path, w=16, h=16, invert=False, threshold=100):
    with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as tmp:
        png_path = tmp.name
    subprocess.run(['rsvg-convert', '-w', str(w), '-h', str(h), '-o', png_path, svg_path], check=True)
    img = Image.open(png_path).convert('RGBA')
    alpha = img.split()[-1]
    
    row_bytes = (w + 7) // 8
    msb_bytes = []
    xbm_bytes = []
    
    for r in range(h):
        for c_byte in range(row_bytes):
            msb_b = 0
            xbm_b = 0
            for bit in range(8):
                c = c_byte * 8 + bit
                if c < w:
                    p = alpha.getpixel((c, r))
                    is_on = (p > threshold) if not invert else (p <= threshold)
                    if is_on:
                        msb_b |= (1 << (7 - bit))
                        xbm_b |= (1 << bit)
            msb_bytes.append(msb_b)
            xbm_bytes.append(xbm_b)
            
    base_name = re.sub(r'[^a-zA-Z0-9_]', '_', os.path.splitext(os.path.basename(svg_path))[0])
    xbm_text = generate_xbm_text(base_name, w, h, xbm_bytes)
    return base_name, w, h, msb_bytes, xbm_bytes, xbm_text

def convert_image_to_bitmap(img_path, w=16, h=16, invert=False, threshold=128):
    img = Image.open(img_path).convert('L').resize((w, h), Image.Resampling.LANCZOS)
    row_bytes = (w + 7) // 8
    msb_bytes = []
    xbm_bytes = []
    
    for r in range(h):
        for c_byte in range(row_bytes):
            msb_b = 0
            xbm_b = 0
            for bit in range(8):
                c = c_byte * 8 + bit
                if c < w:
                    p = img.getpixel((c, r))
                    # L: 0=black, 255=white. On OLED, white is pixel on
                    is_on = (p > threshold) if not invert else (p <= threshold)
                    if is_on:
                        msb_b |= (1 << (7 - bit))
                        xbm_b |= (1 << bit)
            msb_bytes.append(msb_b)
            xbm_bytes.append(xbm_b)
            
    base_name = re.sub(r'[^a-zA-Z0-9_]', '_', os.path.splitext(os.path.basename(img_path))[0])
    xbm_text = generate_xbm_text(base_name, w, h, xbm_bytes)
    return base_name, w, h, msb_bytes, xbm_bytes, xbm_text
