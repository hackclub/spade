# Spade - an implementation of Sprig engine

This repo is a C implementation of the engine you can find in the "engine" folder of the repo github.com/hackclub/sprig

It was reimplemented in C to run on the Raspberry Pi Pico.

However, debugging is hard when compiling directly to arm-eabi-none, so the engine can also be compiled to x86, in which case it renders to a minifb window.

The environment variable -DSPADE_TARGET can be passed to Cmake to specify which version you would like to build, one you can run on your PC, or one you can run on your Sprig hardware.

## Building

Prerequisites:

- A working Python 3 environment.
- The ability to run Bash scripts.
- A C build environment, preferably Clang. on Windows, GC won't work and you must use Clang. make sure CMake and Make are both working.
- *Optional:* entr installed. Only needed if you want to use jsdev.sh.

Set up your build environment. All folders need to be in your home directory, although they can be linked if you prefer.

Clone Spade:

```sh
cd ~
git clone https://github.com/hackclub/spade.git
cd spade
git submodule update --init --recursive
```

Install JerryScript:

```sh
mkdir ~/jerryscript_build
cd ~/jerryscript_build
git clone https://github.com/jerryscript-project/jerryscript.git
cd jerryscript
git checkout 8ba0d1b6ee5a065a42f3b306771ad8e3c0d819bc # version 2.4.0

cd ~/spade
./pc/jerry/refresh.sh
```

Download the Pico SDK:

```sh
mkdir ~/raspberrypi
cd ~/raspberrypi
git clone -b 1.3.1 https://github.com/raspberrypi/pico-sdk.git
git clone https://github.com/raspberrypi/pico-extras.git
cd pico-sdk
git submodule update --init
cd ../pico-extras
git submodule update --init
```

### Engine CStrings

For compiling on both PC and Pico you'll need to convert engine.js to a .cstring file.

If you installed entr, just run `./jsdev.sh` to update the file every time you change it.

Otherwise: `./tools/cstringify.py engine.js > engine.js.cstring`

### Pico Build

```sh
cmake --preset=rpi
cd rpi_build
make
```

A UF2 file will be outputted to `spade.uf2`. On macOS, with a Pico plugged in and in BOOTSEL mode, you can transfer from the CLI with `
cp spade.uf2 /Volumes/RPI-RP2`.

### PC Build

```sh
cmake --preset=pc
cd pc_build
make
./spade ../game.js
```

The audio emulator is written for CoreAudio and audio will be muted on non-macOS systems.

If you get an error about a missing Pico SDK, run the following and try again:

```sh
export PICO_SDK_PATH=~/raspberrypi/pico-sdk
export PICO_EXTRAS_PATH=~/raspberrypi/pico-extras
```
