# simple-sgm
Implementation of a simplified version of the SemiGlobal Matching algorithm (Hirschmuller, H. (2005). Accurate and Efficient Stereo Processing by Semi Global Matching and Mutual Information. CVPR .) 

## How to build

```
#install conan, needed only once
pip install conan

#from the repo root
conan install -if build --build missing .
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install -G "Ninja" ..
ninja install 

```