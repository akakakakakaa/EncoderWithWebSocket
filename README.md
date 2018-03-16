nvidia-docker run -it --privileged -e NVIDIA_DRIVER_CAPABILITIES=compute,utility,video nvidia/cuda:9.1-cudnn7-devel-centos7

yum -y update && apt-get upgrade
//보통 gcc버전 4.8깔림
yum -y install git wget which kernel-devel gcc
// nvidia-linux driver 설치
/*

1. Edit /etc/default/grub. Append the following  to “GRUB_CMDLINE_LINUX”
rd.driver.blacklist=nouveau

2. grub2-mkconfig -o /boot/grub2/grub.cfg //난이거 안됨
   grub2-mkconfig -o /boot/efi/EFI/centos/grub.cfg //이 위치가 됬었음

3. vi /etc/modprobe.d/nvidia-disable-nouveau.conf
4. blacklist nouveau 입력
*/
//kernel-devel깔면 kernel 버전을 맞춰주기위해, nouveau를 끄기 위해 리붓 필요
reboot

cd /root/mansu
sh nivida 드라이버
설치끝
rm -rf nvidia드라이버
wget 쿠다 런파일
sh 쿠다 런파일
설치 끝
rm -rf 쿠다 런파일

yum -y install centos-release-scl
//gcc 7버전깔림. gcc 4.8로 안되는거 있을때 쓰기 위함
yum -y install devtoolset-7-gcc-c++
//scl enable을 하면 gcc7을 쓸수 있음 exit로 devtoolset사용을 중지시킴
scl enable devtoolset-7 bash
문제: centos7에서 ldconfig로 라이브러리 path 추가가 안됨
      ldconfig가 인식하는 경로로 ffmpeg 라이브러리를 옮겨주어야 함

1. ffmpeg 설치
cd /root/mansu
git clone -b release/3.4 https://github.com/FFmpeg/FFmpeg.git
cd FFmpeg
yum install -y epel-release yasm make
./configure --prefix=./build --enable-shared --disable-static --enable-cuda --enable-cuvid --enable-nvenc --enable-libfdk-aac --enable-nonfree --enable-libnpp --cc="gcc -m64 -fPIC" --extra-cflags=-I/usr/local/cuda/include --extra-ldflags=-L/usr/local/cuda/lib64
make -j100
make install
//echo | gcc -shared -o /usr/lib/x86_64-linux-gnu/libGL.so.1 -Wl,-soname=libGL.so.1 -x c -

2. WebSocket 설치
cd /root/mansu
wget https://dl.bintray.com/boostorg/release/1.66.0/source/boost_1_66_0.tar.gz
tar -xvzf boost_1_66_0.tar.gz
rm -rf boost_1_66_0.tar.gz
cd boost_1_66_0
./bootstrap.sh
yum install python-devel
./b2 -j100 install
cd ..
git clone https://github.com/zaphoyd/websocketpp.git
mkdir vdi
cd vdi
//test.cpp 파일 생성
// /usr/local/lib가 ldconfig에 추가가 안된다. 왜그럴까
g++ test.cpp --std=c++11 -I../websocketpp -lpthread -lboost_system
LD_LIBRARY_PATH="/usr/local/lib" ./a.out

//uWebSocket
cd /root/mansu
git clone https://github.com/uNetworking/uWebSockets.git
cd uWebSockets
yum install openssl-devel
make PREFIX=/root/mansu/uWebSockets/build
make install PREFIX=/root/mansu/uWebSockets/build

3. MySQL Connector c++ 설치
cd /root/mansu
wget https://dev.mysql.com/get/Downloads/Connector-C++/mysql-connector-c++-1.1.9-linux-glibc2.12-x86-64bit.tar.gz
tar -xvzf mysql-connector-c++-1.1.9-linux-glibc2.12-x86-64bit.tar.gz
rm -rf mysql-connector-c++-1.1.9-linux-glibc2.12-x86-64bit.tar.gz
mv mysql-connector-c++-1.1.9-linux-glibc2.12-x86-64bit mysql-connector-c++

4. libsourcey 설치(webrtc용)
cd /root
git clone https://github.com/sourcey/libsourcey.git
cd libsourcey
mkdir webrtc
cd webrtc
wget https://github.com/sourcey/webrtc-precompiled-builds/raw/master/webrtc-16937-cf6f3f6-linux-x64.tar.gz
tar -xvzf webrtc-16937-cf6f3f6-linux-x64.tar.gz
rm -rf webrtc-16937-cf6f3f6-linux-x64.tar.gz
mkdir build
cd build
//root_dir 상대경로로 하면 안됨. 절대경로로 해야됨
nano /etc/yum.repos.d/FedoraRepo.repo
/*
[warning:fedora]
name=fedora
mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-23&arch=$basearch
enabled=1
gpgcheck=1
gpgkey=https://getfedora.org/static/34EC9CBA.txt
*/
yum -y update gcc g++
yum install libX11-devel libGLU-devel
rm -rf /etc/yum.repos.d/FedoraRepo.repo
yum clean
yum update
cmake .. -DWITH_WEBRTC=ON -DWEBRTC_ROOT_DIR=/root/libsourcey/webrtc
//vi /root/libsourcey/src/webrtc/src/peerfactorycontext.cpp해서 ~~~encoder_factory.h 주석처리 해야됨
make -j100
make install

5. json 설치
cd /root/mansu
git clone https://github.com/nlohmann/json.git

6. qemu 설치
cd /root/mansu
git clone https://github.com/qemu/qemu.git
cd qemu
mkdir build
cd build
yum -y install gtk2-devel flex bison gettext alsa-lib-devel glib
../configure --prefix=../../qemuBuild --target-list=x86_64-softmmu --audio-drv-list=alsa --extra-cflags="-I/root/mansu/vdi" --extra-ldflags="-L/root/mansu/FFmpeg/build/lib -L/root/mansu/mysql-connector-c++/lib -L/root/mansu/vdi -L/root/mansu/uWebSockets/build/lib64 -lcollabo -lavformat -lavcodec -lavutil -lavfilter -lswscale -lavdevice -lswresample -lpthread -lboost_system -ldl -luWS -lssl -lz -lm -lmysqlcppconn -lboost_thread"
make -j100
make install

//g++ main.cpp VDIServer.cpp MySQLConnector.cpp --std=c++11 -I../websocketpp -I../mysql-connector-c++/include -L../mysql-connector-c++/lib -lmysqlcppconn -lpthread -lboost_system
