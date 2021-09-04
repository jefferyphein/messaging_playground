CXX = g++
CPPFLAGS += `pkg-config --cflags flatbuffers` -std=c++11 -g -O3 -fPIC

LDFLAGS += -L/usr/local/lib `pkg-config --libs flatbuffers`

# The protobuf compiler.
FLATC = flatc

# Path to the protobuf specifications.
FBS_PATH = ./flatbuffers

# Supply missing .proto files from here.
vpath %.fbs $(FBS_PATH)

all: flat

flat: flat.cc flat_generated.h
	$(CXX) -o $@ $< $(CPPFLAGS) $(LDFLAGS)

.PRECIOUS: %_generated.h
%_generated.h: %.fbs
	$(FLATC) --cpp $<

clean:
	-rm -f flat flat_generated.h
