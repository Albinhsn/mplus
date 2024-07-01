CC := g++
CFLAGS := -O0 -g -std=c++11 -Wall -Werror -Wno-strict-aliasing
LDFLAGS := -lm -lGL -lSDL2
TARGET = main


c:
	$(CC) $(CFLAGS) src/main.cpp -o main $(LDFLAGS)

clean:
	rm -rf obj/ $(TARGET)

.PHONY: all clean

len:
	find . -name '*.cpp' | xargs wc -l
