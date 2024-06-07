CC := g++
CFLAGS := -O0 -g -std=c++11 -Wall -Werror -Wno-strict-aliasing
LDFLAGS := -lm -lGL -lSDL2
TARGET = main


LIB_SRCS = $(wildcard src/*.cpp)
LIB_OBJS = $(patsubst src/%.cpp,obj/%.o,$(LIB_SRCS))


g: $(TARGET)
$(TARGET): $(LIB_OBJS)
	$(CC)  -o $@ $^ $(LDFLAGS)

obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

obj/common.o: src/common.cpp
	$(CC) $(CFLAGS) -c src/common.cpp -o obj/common.o

obj/files.o: src/files.cpp
	$(CC) $(CFLAGS) -c src/files.cpp -o obj/files.o

obj/random.o: src/random.cpp
	$(CC) $(CFLAGS) -c src/random.cpp -o obj/random.o

compile lib_sta.a: obj/common.o obj/random.o obj/files.o
	ar rcs lib/lib_sta.a obj/common.o obj/files.o obj/random.o

render:
	$(CC) $(CFLAGS)  ./src/font.cpp ./src/files.cpp ./src/noise.cpp src/random.cpp src/common.cpp src/vector.cpp src/sdl.cpp src/sta_renderer.cpp main.cpp $(LDFLAGS) -o main && ./main


clean:
	rm -rf obj/ $(TARGET)

.PHONY: all clean

len:
	find . -name '*.c' | xargs wc -l
