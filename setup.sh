echo "Spade Builder and Setup:tm:"
echo "To quickly build your game for pc or rpi, run the following:"
echo "./setup.sh rpi"
echo "./setup.sh pc"
if [ "$1" == "rpi" ]; then
echo "Building for sprig console"
cd ~/spade
cmake --preset=rpi
cd rpi_build
make
echo "Built!"
exit
elif [ "$1" == "pc" ]; then
echo "Building for pc"
cd ~/spade
cmake --preset=pc
cd pc
make
echo "Built! Running your game."
./spade ../game.js
exit
fi

cd ~
git clone https://github.com/hackclub/spade.git
cd spade
git submodule update --init --recursive

if [ -f "~/jerryscript_build" ]; then
    echo "jerryscript seems to be installed"
else 
  mkdir ~/jerryscript_build
cd ~/jerryscript_build
git clone https://github.com/jerryscript-project/jerryscript.git
cd jerryscript
git checkout 8ba0d1b6ee5a065a42f3b306771ad8e3c0d819bc # version 2.4.0
fi

cd ~/spade
./pc/jerry/refresh.sh

if [ -f "~/raspberrypi/pico-sdk" ]; then
    echo "pico-sdk seems to be installed"
else 
   mkdir ~/raspberrypi
    cd ~/raspberrypi
    git clone https://github.com/raspberrypi/pico-sdk.git
    git clone https://github.com/raspberrypi/pico-extras.git
    cd pico-sdk
    git submodule update --init
    cd ../pico-extras
    git submodule update --init
fi
# Always do this in case
./tools/cstringify.py engine.js > engine.js.cstring
