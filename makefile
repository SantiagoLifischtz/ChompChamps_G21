CC=gcc
SRC=src
CFLAGS=-Wall -Wextra -std=c99 -g -I$(SRC)/include
BUILD=build
PLAYERS_SRC=$(wildcard $(SRC)/players/*.c)
PLAYERS_BIN=$(patsubst $(SRC)/players/%.c,$(BUILD)/players/%,$(PLAYERS_SRC))

all: $(BUILD)/master $(BUILD)/vista $(PLAYERS_BIN)

$(BUILD)/playerlib.o: $(SRC)/playerlib.c | $(BUILD)/
	$(CC) $(CFLAGS) -c -o $@ $<

$(PLAYERS_BIN): $(BUILD)/players/%: $(SRC)/players/%.c $(BUILD)/playerlib.o | $(BUILD)/players/
	$(CC) $(CFLAGS) -o $@ $< $(BUILD)/playerlib.o

$(BUILD)/players/:
	mkdir -p $(BUILD)/players/

$(BUILD)/:
	mkdir -p $(BUILD)/

$(BUILD)/master: $(SRC)/master.c | $(BUILD)/
	$(CC) $(CFLAGS) -o $@ $< -lrt -pthread -lm

$(BUILD)/vista: $(SRC)/vista.c | $(BUILD)/
	$(CC) $(CFLAGS) -o $@ $< -lrt -pthread

clean:
	rm -Rf $(BUILD)/*
