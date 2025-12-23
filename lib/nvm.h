#ifndef NVM_H
#define NVM_H

#include <stdint.h>
#include <stdbool.h>

// Constants
#define MAX_PROCESSES 8
#define STACK_SIZE 256
#define MAX_LOCALS 32
#define MAX_CAPS 16
#define TIME_SLICE_MS 10

// Process structure
typedef struct {
    int8_t* bytecode;
    int32_t size;
    int32_t ip;           // Instruction pointer
    int32_t sp;           // Stack pointer
    int32_t stack[STACK_SIZE];
    int32_t locals[MAX_LOCALS];
    bool active;
    bool blocked;
    int8_t pid;
    int32_t exit_code;
    int16_t capabilities[MAX_CAPS];
    int8_t caps_count;
} nvm_process_t;

// Function declarations
void nvm_init();
int nvm_create_process(int8_t* bytecode, int32_t size, int16_t initial_caps[], int8_t caps_count);
bool nvm_execute_instruction(nvm_process_t* proc);
void nvm_scheduler_tick();
void nvm_execute(int8_t* bytecode, int32_t size, int16_t* capabilities, int8_t caps_count);
int32_t nvm_get_exit_code(int8_t pid);
bool nvm_is_process_active(int8_t pid);

#endif // NVM_H
