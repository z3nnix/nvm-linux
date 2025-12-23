#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stdbool.h>
#include <nvm.h>

#define SYSCALL_EXIT        0x00
#define SYSCALL_PRINT       0x0E

// System call handler
int32_t syscall_handler(int8_t syscall_id, nvm_process_t* proc);

#endif // SYSCALL_H