#!/usr/bin/env bash

if [[ `uname` == "Darwin" ]]; then
    # Check if user is using brew
    which -s brew
    if [[ $? != 0 ]] ; then
        echo Homebrew not found. Please first install homebrew through: https://brew.sh/
    exit 1
    fi
    brew update && brew bundle --file=Brewfile
elif [[ `uname` == "Linux" ]]; then

    sudo apt-get update
    sudo apt-get install software-properties-common build-essential --yes
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test --yes
    sudo add-apt-repository "deb http://security.ubuntu.com/ubuntu bionic-security main"
    sudo apt-get update
    sudo apt-get install gcc-10 g++-10 libconfig++-dev --yes
    sudo apt-get install libssl1.0-dev --yes
    #  TODO: Use libssl1.0-dev when upgrade to CMake3.20
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 60 --slave /usr/bin/g++ g++ /usr/bin/g++-10

    if [ ! -d "cmake-3.15.5" ]
    then
      wget https://cmake.org/files/v3.15/cmake-3.15.5.tar.gz
      tar -xzvf cmake-3.15.5.tar.gz
      rm -f cmake-3.15.5.tar.gz
      cd cmake-3.15.5
      ./bootstrap
      make -j$(nproc)
    else
      cd cmake-3.15.5
      sudo make install -j4
    fi
    cd ..

    if [ ! -d "benchmark" ]
    then
      git clone https://github.com/google/benchmark.git
      git clone https://github.com/google/googletest.git benchmark/googletest
      cd benchmark
      cmake -E make_directory "build"
      cmake -E chdir "build" cmake -DCMAKE_BUILD_TYPE=Release ../
      sudo cmake --build "build" --config Release --target install
    else
      cd benchmark/build
      sudo make install -j4
    fi
fi
