SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))
BIN = cppmthrd
CXX ?= g++
CFLAGS := -fsanitize=address
LDFLAGS := -lpthread
LDFLAGS += -fsanitize=address

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) -o $@ $^ $(CFLAGS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS)

clean:
	rm -rf $(OBJS)
	rm -rf $(BIN)
