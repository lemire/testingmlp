CXXFLAGS = -O3 -std=c++11 -Wall -g
CFLAGS   = -O3 -Wall

.PHONY = clean test

.DELETE_ON_ERROR:

testingmlp: testingmlp.o generated.o page-info.o
	$(CXX) $(CXXFLAGS) -o $@ $^

ifneq ($(GENERATE), 0)
generated.cpp : gen.py
	python3 gen.py > generated.cpp
endif

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $<

%.o : %.c
	$(CC) $(CFLAGS) -c $<

page-info.o : page-info.h

generated.o testingmlp.o : common.hpp

test: testingmlp
	./testingmlp

clean:
	rm -f testingmlp generated.cpp *.o
