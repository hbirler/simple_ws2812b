
bin/proj: main.cpp
	mkdir -p bin
	g++ -O3 -ggdb -fuse-ld=lld -std=c++17 -L/usr/local/lib/ -lbcm2835 $< -o $@

clean:
	rm -r bin/

.PHONY: clean
