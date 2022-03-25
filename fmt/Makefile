CFLAGS = -O3 -g -Wall -fPIC `pkg-config --cflags gmp`
CPPFLAGS = -O3 -g -Wall -fPIC -std=c++17 `pkg-config --cflags gmp fmt`
LDFLAGS = `pkg-config --libs gmp fmt`

all: main libfmt_capi.so

main: main.c libfmt_capi.so
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -L. -lfmt_capi

libfmt_capi.so: fmt.cpp fmt.hpp
	$(CXX) -o $@ $< -shared $(CPPFLAGS) $(LDFLAGS)

clean:
	-rm -f main libfmt_capi.so
