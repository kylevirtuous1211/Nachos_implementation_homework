#include "syscall.h"

int main() {
    // Starting address - adjust based on Nachos memory layout
    unsigned int address = 0x400000; // Example starting address (4MB)

    while (1) {
        *((int*)address) = 1; // Write to the address to allocate the page
        address += 0x1000;     // Increment by page size (4KB) to move to next page
    }

    return 0;
}