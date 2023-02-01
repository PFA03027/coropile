

all: a.out

clean:
	-rm -fr a.out

a.out: coropile_test.cpp ../inc/coropile.hpp
	g++ -std=c++20 -fcoroutines -g -I../inc -DCOROPILE_INTERNAL_DEBUG_LOG coropile_test.cpp
