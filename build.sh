g++ -Wall -O3 -rdynamic -std=c++1y test.cpp -o app `pkg-config --cflags --libs opencv`