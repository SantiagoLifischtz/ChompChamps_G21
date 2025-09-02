#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

double randNorm() {
    return (double)rand() / ((double)RAND_MAX + 1);
}

int randInt(int minInclusive, int maxExclusive) {
    return minInclusive + (int)(randNorm() * (maxExclusive - minInclusive));
}

int main() {
    srand(getpid() ^ time(NULL));
    while(1) {
        unsigned char move = randInt(0,8);
        write(STDOUT_FILENO, &move, sizeof(move));
    }
    return 0;
}