set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJECT_HOME=$SCRIPT_DIR/../../../..

cd $PROJECT_HOME/jerryscript_build
rm -rf example-*

python3 jerryscript/tools/build.py \
  --builddir=$(pwd)/example_build \
  --cmake-param="-DCMAKE_INSTALL_PREFIX=$(pwd)/example_install/" \
  --cmake-param="-G Unix Makefiles" \
  --mem-heap=350 \
  --debug \
  --clean \
  --error-messages=ON \
  --mem-stats=ON \
  --line-info=ON \
  --jerry-cmdline=OFF
make -C $(pwd)/example_build install\

cd $PROJECT_HOME/spade/src/pc/jerry
cp -r $PROJECT_HOME/jerryscript_build/example_build/lib ./
rm -rf include
cp -r $PROJECT_HOME/jerryscript_build/example_install/include ./
