all: pub sub

%: %.cc
	g++ -o $@ $< -I${CONDA_PREFIX}/include -L${CONDA_PREFIX}/lib -L${CONDA_PREFIX}/lib64 -lmosquitto
