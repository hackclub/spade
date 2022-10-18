cd ~/jerryscript_build
rm -rf example-*

python3 jerryscript/tools/build.py \
  --builddir=$(pwd)/example_build \
  --cmake-param="-DCMAKE_INSTALL_PREFIX=$(pwd)/example_install/" \
  --debug \
  --clean \
  --error-messages=ON \
  --line-info=ON \
  --jerry-cmdline=OFF
make -C $(pwd)/example_build install\

cd ~/spade/jerry
cp ~/jerryscript_build/example_build/lib/* lib/
cp ~/jerryscript_build/example_install/include
rm -rf include
cp ~/jerryscript_build/example_install/include ./
cp -r ~/jerryscript_build/example_install/include ./
