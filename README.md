# Spade - an implementation of Sprig engine

This repo is a C implementation of the engine you can find in the "engine" folder of the repo [hackclub/sprig](https://github.com/hackclub/sprig)

It was reimplemented in C to run on the Raspberry Pi Pico.

However, debugging is hard when compiling directly to arm-eabi-none, so the engine can also be compiled to x86, in which case it renders to a minifb window.

The environment variable -DSPADE_TARGET can be passed to CMake to specify which version you would like to build, one you can run on your PC, or one you can run on your Sprig hardware.

## Building

Prerequisites:

- A working Python 3 environment.
- The ability to run Bash scripts.
- A C build environment, preferably Clang. on Windows, GCC won't work and you must use Clang. Make sure CMake and Make are both working.
- Entr and uglifyjs installed to use jsdev.sh.

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

Run `./jsdev.sh` to minify and update the engine. Keep it running to auto-update.

### Pico Build

```sh
cmake --preset=rpi
# then...
cmake --build --preset=rpi
```

A UF2 file will be outputted to `rpi_build/src/spade.uf2`. On macOS, with a Pico plugged in and in BOOTSEL mode, you can transfer from the CLI with `cp ./rpi_build/src/spade.uf2 /Volumes/RPI-RP2`.

### PC Build

```sh
cmake --preset=pc
# then...
cmake --build --preset=pc
./pc_build/src/spade ../game.min.js
```

The audio emulator is written for CoreAudio and audio will be muted on non-macOS systems.

If you get an error about a missing Pico SDK, run the following and try again:

```sh
export PICO_SDK_PATH=~/raspberrypi/pico-sdk
export PICO_EXTRAS_PATH=~/raspberrypi/pico-extras
```
