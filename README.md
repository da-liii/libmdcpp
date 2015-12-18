# libmdcpp

A markdown implementation forked from [cpp-markdown](http://sourceforge.net/projects/cpp-markdown/).

It is intended for [marketo](https://github.com/sadhen/marketo).

**Note**: The implementation won't 100% support CommonMark, but aims to pass the most significant test case. If you find an important test case libmdcpp hasn't pass, please tell me in the issues.

## Install
First of all, you need to install the dependencies. In **Debian Sid**, just
```
sudo apt-get install g++ cmake libboost-regex-dev
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
