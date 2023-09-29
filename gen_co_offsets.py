#!/usr/bin/python

from statistics import stdev as stdev
import re

co_offsets = []

with open("fb.tmp") as f:
    lines = f.readlines()
    i = 0
    for line in lines:
        i = i + 1
        try:
            res = re.match(r".*offset=(\d+)&size=0",line)
            if(res is not None):
                co_offsets.append(int(res.group(1)))
        except Exception as e:
            pass


with open("co_offsets", "w") as wf:
    wf.write("%d\n"%len(co_offsets))
    for co_offset in co_offsets:
        wf.write("%d\n"%(co_offset-co_offsets[0]))
        print("offset = 0x%x"%(co_offset - co_offsets[0]))
