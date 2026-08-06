@echo off
g++ -o forcedelete.exe force-remove.cpp release-exec.cpp nt.cpp utils.cpp -lpathcch -std=c++23 -static -O3
upx --best --ultra-brute forcedelete.exe