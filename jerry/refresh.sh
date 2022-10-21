#!/bin/bash

JS_DIR=/home/me/jerryscript-build
SPADE_DIR=/home/me/clones/hackclub--spade

cd $JS_DIR
rm -r example-*

python3 jerryscript/tools/build.py \
  --builddir=$(pwd)/example_build \
  --cmake-param="-DCMAKE_INSTALL_PREFIX=$(pwd)/example_install/" \
  --debug \
  --clean \
  --error-messages=ON \
  --mem-stats=ON \
  --line-info=ON \
  --jerry-cmdline=OFF
make -C $(pwd)/example_build install

cd $SPADE_DIR/jerry
cp $JS_DIR/example_build/lib/* lib/
rm -r include
cp -r $JS_DIR/example_install/include ./
