

all: a.out

clean:
	-rm -fr a.out

a.out: coropile_test.cpp ../inc/coropile.hpp
	g++ -std=c++20 -fcoroutines -g -I../inc coropile_test.cpp
