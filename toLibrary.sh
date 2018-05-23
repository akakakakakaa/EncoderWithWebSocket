rm -rf *.o
rm -rf libcollabo.so
gcc -fPIC -std=c++11 -c Collabo.cpp Encoder.cpp EncoderCQ.cpp WebSocketServer.cpp MySQLConnector.cpp Base64.cpp JpegCompressor.cpp AgentManager.cpp -I../FFmpeg/build/include -I../json/include -I../mysql-connector-c++/include -I../uWebSockets/build/include -I../libjpeg-turbo/build/include
gcc --shared -Wl,-soname,libcollabo.so -o libcollabo.so Collabo.o Encoder.o EncoderCQ.o WebSocketServer.o MySQLConnector.o Base64.o JpegCompressor.o AgentManager.o
