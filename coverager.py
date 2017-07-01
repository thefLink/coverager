import os
import ctypes
import time

from ctypes import *

def get_new_bbs(bitmap, new_bitmap):
    return (bitmap ^ new_bitmap) & new_bitmap

def parse_bitmap(bitmap):

    lbitmap = list(bitmap)
    cov = 0
    for c in lbitmap:
        cov = (cov << 8) + ord(c)

    return cov


amount_bbs = 0
superbitmap = 0
description = open("patched_description", "r").read()
amount_bbs = len(description.split('\x0a'))
corpuses = os.listdir("/home/flink/storage/testcases")
usefull_corpuses = []
progress = 0

libcoverager = cdll.LoadLibrary("./libcoverager.so")
libcoverager.generate_bitmap.restype = ctypes.POINTER(ctypes.c_char)
libcoverager.init_coveraging(description, "/usr/bin/evince", "/usr/lib/libevview3.so.3.0.0", 1)

for corpus in corpuses:
    pntr = POINTER(c_char_p)(create_string_buffer("/home/flink/storage/testcases/" + corpus))
    bitmap_tmp = libcoverager.generate_bitmap(pntr)[0:amount_bbs / 8 + 1]

    bitmap_tmp = parse_bitmap(bitmap_tmp)
    new_bbs = get_new_bbs(superbitmap, bitmap_tmp)

    if bin(new_bbs).count("1") > 5:
        superbitmap = superbitmap | new_bbs
        usefull_corpuses.append(corpus)    

    progress += 1
    if progress % 10 == 0:
        print "[+] Found: %d of %d basic blocks. %d Corpuses remaining" % (bin(superbitmap).count("1"), amount_bbs, len(corpuses) - progress)

    time.sleep(0.1)
print "********************"
print usefull_corpuses
print "********************"

with open("usefull_corpuses", "w") as logfile:
    for c in usefull_corpuses:
        logfile.write(c + "\n")

logfile.close()
