rm -rf *.o
rm -rf libcollabo.so
gcc -fPIC -std=c++11 -c Collabo.cpp Encoder.cpp WebSocketServer.cpp MySQLConnector.cpp Base64.cpp -I../FFmpeg/build/include -I../json/include -I../mysql-connector-c++/include -I../uWebSockets/build/include
gcc --shared -Wl,-soname,libcollabo.so -o libcollabo.so Collabo.o Encoder.o WebSocketServer.o MySQLConnector.o Base64.o
