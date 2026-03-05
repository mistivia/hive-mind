CC= g++
CFLAGS= -g -std=c++17 -Wall
RM= rm -f

LIBS = -lm -lpthread -lmstch -static
INCS = -I./src/ -I./lib/crow/include/ -Ilib/mstch/include/

OBJ = $(patsubst %.cc,%.o,$(shell find src/ -name *.cc))

all: hivemind

lib/mstch/src/libmstch.a:
	cd lib/mstch && \
	cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ . && \
	make

hivemind: $(OBJ) lib/mstch/src/libmstch.a
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

package: hivemind
	tar --gzip -cf hivemind-release.tar.gz hivemind resource/

%.o: %.cc
	$(CC) -c -o $@ $(CFLAGS) ${INCS} $<

.PHONY: clean
clean:
	-$(RM) $(shell find . -name *.o) hivemind
	-cd lib/mstch && make clean
