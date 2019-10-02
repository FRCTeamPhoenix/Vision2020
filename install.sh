unzip allwpilib-master.zip
mv allwpilib-master wpiliba
mv wpiliba ~
cd ~/wpiliba
mkdir build && cd build
cmake -DWITHOUT_JAVA=ON ..
make -j4
sudo make install
