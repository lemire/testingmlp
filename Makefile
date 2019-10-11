CXXFLAGS = -O3 -std=c++11 -Wall 

.PHONY = clean test

.DELETE_ON_ERROR:

testingmlp: testingmlp.o generated.o 
	$(CXX) $(CXXFLAGS) -o $@ $^


ifneq ($(GENERATE), 0)
generated.cpp : gen.py
	python3 gen.py > generated.cpp
endif

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $^

test: testingmlp
	./testingmlp

clean:
	rm -f testingmlp generated.cpp  *.o
