dnf install git python3 clang make cmake entr uglify-js gcc-arm-linux-gnu arm-none-eabi-gcc-cs-c++ arm-none-eabi-gcc-cs arm-none-eabi-binutils arm-none-eabi-newlib -y

cd ~
git clone https://github.com/hackclub/spade.git
cd spade

mkdir ~/jerryscript_build
cd ~/jerryscript_build
git clone https://github.com/jerryscript-project/jerryscript.git
cd jerryscript
git checkout 8ba0d1b6ee5a065a42f3b306771ad8e3c0d819bc # version 2.4.0

cd ~/spade
./src/pc/jerry/refresh.sh

mkdir ~/raspberrypi
cd ~/raspberrypi
git clone -b 1.3.1 https://github.com/raspberrypi/pico-sdk.git
git clone https://github.com/raspberrypi/pico-extras.git
cd pico-sdk
git checkout 7070d230c0cdf1add83fa86b6832b47b2da47788
git submodule update --init
cd ../pico-extras
git submodule update --init

mkdir /artifacts