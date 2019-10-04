unzip allwpilib-master.zip
mv allwpilib-master wpilib
mv wpilib ~
cd ~/wpilib
mkdir build && cd build
cmake -DWITHOUT_JAVA=ON ..
make -j4
sudo make install