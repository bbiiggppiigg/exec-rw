# build
git submodule init ELFIO
git submodule update
./build.sh

# exec-rw

Tool to embed new (instrumented) fat binaries into original executable.


## Usage

### Insert new hipFatbin
```
$ exec-rw <og-exec> <fatbin> <new-exec>
```

### Patch hipFatbinSegment

```
$ roc-obj-ls <inserted-exec> > fb.tmp
$ python gen_co_offsets.py
$ exec-rw2 <og-exec> <fatbin> <new-exec> <co_offsets>
$ llvm-objcopy --rename-section=.hip_fatbin=<random-name> <new-exec>
$ llvm-objcopy --rename-section=.new_fatbin=.hip_fatbin <new-exec>
```

## License
MIT
