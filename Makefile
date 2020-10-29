CXX=clang++
CXXFLAGS=-std=c++17 -g -stdlib=libc++
OBJS=main.o
sc: $(OBJS)
	$(CXX) -o sc $(CXXFLAGS) $(OBJS)
