CC = gcc
CFLAGS = -std=c11 -O2 -s -Wall -Wextra -Wpedantic -Werror -Isrc -Ilib/include
CORE = src/map.c src/algorithms.c src/game.c
LIBS = -Llib/lib -lraylib -lopengl32 -lgdi32 -lwinmm
HEADERS = $(wildcard src/*.h)
TARGET ?= TankBattle90.exe
TEST_TARGET ?= work/CoreTests.exe
VIEW_TARGET ?= work/ViewTests.exe
REVISION_TARGET ?= work/RevisionTests.exe
INPUT_TARGET ?= work/InputTests.exe

all: $(TARGET)
$(TARGET): $(CORE) src/view.c src/main.c $(HEADERS)
	$(CC) $(CFLAGS) $(CORE) src/view.c src/main.c -o "$@" $(LIBS) -mwindows
tests: $(TEST_TARGET)
$(TEST_TARGET): $(CORE) tests/test_core.c tests/wall_fixture.h src/tank.h | work
	$(CC) $(CFLAGS) $(CORE) tests/test_core.c -o "$@" -lm -Wl,--wrap=malloc,--wrap=calloc
visuals: $(VIEW_TARGET)
$(VIEW_TARGET): $(CORE) src/view.c tests/test_view.c tests/wall_fixture.h $(HEADERS) | work
	$(CC) $(CFLAGS) $(CORE) src/view.c tests/test_view.c -o "$@" $(LIBS)
revision: $(REVISION_TARGET)
$(REVISION_TARGET): $(CORE) tests/test_revision.c src/tank.h | work
	$(CC) $(CFLAGS) $(CORE) tests/test_revision.c -o "$@" -lm -Wl,--wrap=malloc,--wrap=calloc,--wrap=free
inputs: $(INPUT_TARGET)
$(INPUT_TARGET): $(CORE) src/view.c tests/test_input.c $(HEADERS) | work
	$(CC) $(CFLAGS) $(CORE) src/view.c tests/test_input.c -o "$@" $(LIBS) -Wl,--wrap=IsKeyPressed,--wrap=IsKeyDown
work:
	mkdir work
.PHONY: all tests visuals revision inputs
