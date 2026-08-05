@echo off
g++ -o forcedelete.exe force-remove.cpp nt.cpp utils.cpp -lpathcch -static -O3
upx --best --ultra-brute forcedelete.exe