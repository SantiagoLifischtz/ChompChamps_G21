CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -g
BUILD=build
SRC=src

all: $(BUILD)/master $(BUILD)/jugador1 $(BUILD)/jugador2 $(BUILD)/jugador3 $(BUILD)/vista

$(BUILD)/master: $(SRC)/master.c
	$(CC) $(CFLAGS) -o $@ $< -lrt -pthread

$(BUILD)/jugador1: $(SRC)/jugador1.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/jugador2: $(SRC)/jugador2.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/jugador3: $(SRC)/jugador3.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD)/vista: $(SRC)/vista.c
	$(CC) $(CFLAGS) -o $@ $< -lrt -pthread

clean:
	rm -f $(BUILD)/*
