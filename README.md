# libmdcpp

A markdown implementation forked from [cpp-markdown](http://sourceforge.net/projects/cpp-markdown/).

It is intended for [KMarknote](https://github.com/sadhen/KMarknoteo).

## Install
First of all, you need install the dependencies. In debian, just
```
# you may only need regex library of boost, for convenience
# I recommend you install all the development files of libboost
sudo apt-get install cmake libboost-all-dev
```

Then compile and install
```
cd libmdcpp
mkdir build
# if you do not specify CMAKE_INSTALL_PREFIX
# the library will be install in /usr/local
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```