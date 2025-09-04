CC=gcc
CFLAGS=-Wall -Wextra -std=c99 -g -Isrc/include
BUILD=build
SRC=src
PLAYERS_SRC=$(wildcard $(SRC)/players/*.c)
PLAYERS_BIN=$(patsubst $(SRC)/players/%.c,$(BUILD)/%,$(PLAYERS_SRC))

all: $(BUILD)/master $(BUILD)/vista $(PLAYERS_BIN)

$(PLAYERS_BIN): $(BUILD)/%: $(SRC)/players/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/master: $(SRC)/master.c
	$(CC) $(CFLAGS) -o $@ $< -lrt -pthread

$(BUILD)/vista: $(SRC)/vista.c
	$(CC) $(CFLAGS) -o $@ $< -lrt -pthread

clean:
	rm -f $(BUILD)/*
