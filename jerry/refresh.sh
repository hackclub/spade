cd ~/jerryscript_build
rm -rf example-*

python3 jerryscript/tools/build.py \
  --builddir=$(pwd)/example_build \
  --cmake-param="-DCMAKE_INSTALL_PREFIX=$(pwd)/example_install/" \
  --debug \
  --clean \
  --error-messages=ON \
  --profile=es.next \
  --mem-stats=ON \
  --line-info=ON \
  --promise-callback=ON \
  --jerry-cmdline=OFF
make -C $(pwd)/example_build install\

cd ~/spade/jerry
cp ~/jerryscript_build/example_install/lib/* lib/
rm -rf include
cp -r ~/jerryscript_build/example_install/include ./
