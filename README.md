# mruby Block Puzzle for PlayStation 1

This repository contains the source code for a block puzzle implemented in mruby (and C) for Sony PlayStation.
It also contains a patch and build_config.rb required to build mruby for Sony PlayStation.
The patch is quite 'hacky' and not in a good state but it is meant to be work in progress.

## Prerequisites

You need PSn00bSDK installed on your machine, and a checkout of mruby repository.

## Building

First, you need to build mruby.

```
# in this directory
git clone git@github.com:mruby/mruby.git
cd mruby
git checkout 171847a9
git apply ../patch-171847a9
cp ../playstation.rb build_config/
make MRUBY_CONFIG=playstation
cd ..
```

Then build in this repo:

```
./mruby/bin/mrbc -Bprogram -o src/program.c src/program.rb && cmake --preset default . && cmake --build build
```

If successful, you should have `blockpuzzle-psx.elf` in the `build/` directory.
You can use an emulator to execute this file.

You should also have `blockpuzzle-psx.bin` and `blockpuzzle-psx.cue` in the same directory, which you should be able to write to a CD-R to boot in a PlayStation unit.

> [!NOTE]
> Unless you have a way to boot non-official PlayStation discs (eg. modchips or software mods), you will not be able to boot from a CD-R.

