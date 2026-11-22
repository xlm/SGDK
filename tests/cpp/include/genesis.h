#ifndef TEST_GENESIS_H
#define TEST_GENESIS_H

extern "C" void* MEM_alloc(unsigned short size);
extern "C" void MEM_free(void* pointer);
extern "C" void SYS_die(char* message, ...);

#endif
