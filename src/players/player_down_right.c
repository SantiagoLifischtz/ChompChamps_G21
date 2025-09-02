#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// jugador simple: se mueve en zig-zag derecha/abajo
int main() {
    srand(getpid() ^ time(NULL));
    for (int i=0; 1; i++) {
        unsigned char move;
        if (i % 2 == 0)
            move = 2; // derecha
        else
            move = 4; // abajo
        write(STDOUT_FILENO, &move, sizeof(move));
    }
    return 0;
}
