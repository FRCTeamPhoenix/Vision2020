g++ -std=c++17 $(ls /usr/local/wpilib/include | awk -v FS=" " '{for (i=1; i<=NF; i++) printf " -I/usr/local/wpilib/include/"$i}') vision.cpp
