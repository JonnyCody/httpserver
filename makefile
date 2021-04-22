CXX ?= gcc
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
endif
server: main.cpp threadpool.cpp http.cpp  pub.c
	$(CXX) -g -o server $^ $(CXXFLAGS) -lpthread