CC := g++
CFLAGS := -O0 -g -std=c++11 -Wno-strict-aliasing
LDFLAGS := -lm -lGL -lSDL2
TARGET = main



c:
	$(CC) $(CFLAGS) src/main.cpp -o main $(LDFLAGS) && ./main
d:
	$(CC) $(CFLAGS) src/main.cpp -o main $(LDFLAGS) -DDEBUG && ./main

convert:
	python3 convert.py

clean:
	rm -rf obj/ $(TARGET)

.PHONY: all clean

len:
	find src/ -name '*.cpp' | xargs wc -l
