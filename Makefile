CC := g++
CFLAGS := -O0 -g -std=c++11 -Wall -Wno-strict-aliasing
LDFLAGS := -lm -lGL -lSDL2
TARGET = main


SRCS = $(wildcard src/*.cpp)
OBJS = $(patsubst src/%.cpp,obj/%.o,$(SRCS))
LIB_SRCS = $(wildcard libs/imgui/*.cpp)
LIB_OBJS = $(patsubst libs/imgui/%.cpp,obj/%.o,$(LIB_SRCS))
LIB_SRCS2 = $(wildcard ./libs/imgui/backends/imgui_impl_sdl2*.cpp)
LIB_OBJS2 = $(patsubst libs/imgui/backends/%.cpp,obj/%.o,$(LIB_SRCS2))
LIB_SRCS3 = $(wildcard ./libs/imgui/backends/imgui_impl_opengl3*.cpp)
LIB_OBJS3 = $(patsubst libs/imgui/backends/%.cpp,obj/%.o,$(LIB_SRCS3))


g: $(TARGET)
$(TARGET): $(OBJS) $(LIB_OBJS) $(LIB_OBJS2) $(LIB_OBJS3)
	$(CC)  -o $@ $^ $(LDFLAGS)

obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: libs/imgui/%.cpp
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: libs/imgui/backends/%.cpp
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj/ $(TARGET)

.PHONY: all clean

len:
	find . -name '*.cpp' | xargs wc -l
