apt install libboost-all-dev
apt install openssl
apt install libssl-dev
apt install libopus-dev
apt install libogg-dev
git submodule update --init --recursive
mkdir build
cd build
cmake ..  # or cmake -DCMAKE_BUILD_TYPE=Release ..
make