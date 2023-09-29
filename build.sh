#!/bin/bash

clang++ -g exec-rw.cpp -lelf -o exec-rw -I `pwd`/ELFIO 2>&1 | cat
clang++ -g exec-rw2.cpp -lelf -o exec-rw2 -I `pwd`/ELFIO 2>&1 | cat
# clang++ -g fix-symtab-rw.cpp -lelf -o fix-symtab -I `pwd`/ELFIO 2>&1 | bat
