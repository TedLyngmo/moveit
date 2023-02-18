mvx: mvx.o
	$(CXX) -o $@ $<

mvx.o: mvx.cpp Makefile
	$(CXX) -std=c++20 -O3 -Wall -Wextra -pedantic-errors -c -o $@ $<

clean:
	rm -f mvx mvx.o
format:
	clang-format -i mvx.cpp
