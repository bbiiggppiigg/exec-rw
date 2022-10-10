#!/bin/bash

clang++ -g exec-rw.cpp -lelf -o exec-rw -I `pwd`/ELFIO 2>&1 | bat
