# libmdcpp

A markdown implementation forked from [cpp-markdown](http://sourceforge.net/projects/cpp-markdown/).

It is intended for [KMarkNote](https://github.com/sadhen/KMarkNote).

## Install
First of all, you need to install the dependencies. In **Debian Sid**, just
```
sudo apt-get install g++ cmake libboost-all-dev
```

Then compile and install
```
git clone https://github.com/sadhen/libmdcpp
cd libmdcpp/
mkdir build && cd build/
# if you do not specify CMAKE_INSTALL_PREFIX
# the library will be installed in /usr/local
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

## License
MIT
