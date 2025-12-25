// SPDX-License-Identifier: LGPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <syscall.h>
#include <log.h>
#include <nvm.h>
#include <caps.h>

nvm_process_t processes[MAX_PROCESSES];
uint8_t current_process = 0;
uint32_t timer_ticks = 0;

void nvm_init() {
    for(int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].active = false;
        processes[i].sp = 0;
        processes[i].ip = 0;
        processes[i].exit_code = 0;
        processes[i].caps_count = 0;
    }
}

// Signature checking and process creation
int nvm_create_process(uint8_t* bytecode, uint32_t size, uint16_t initial_caps[], uint8_t caps_count) {
    if(bytecode[0] != 0x4E || bytecode[1] != 0x56 || 
       bytecode[2] != 0x4D || bytecode[3] != 0x30) {
        LOG_WARN("Invalid NVM signature\n");
        return -1;
    }
    
    for(int i = 0; i < MAX_PROCESSES; i++) {
        if(!processes[i].active) {
            processes[i].bytecode = bytecode;
            processes[i].ip = 4;
            processes[i].size = size;
            processes[i].sp = 0;
            processes[i].active = true;
            processes[i].exit_code = 0;
            processes[i].pid = i;
            processes[i].caps_count = 0;

            // Initializing capabilities
            for(int j = 0; j < caps_count && j < MAX_CAPS; j++) {
                processes[i].capabilities[j] = initial_caps[j];
            }
            processes[i].caps_count = caps_count;
            
            for(int j = 0; j < MAX_LOCALS; j++) {
                processes[i].locals[j] = 0;
            }

            return i;
        }
    }
    
    LOG_WARN("No free process slots\n");
    return -1;
}

// Execute one instruction
bool nvm_execute_instruction(nvm_process_t* proc) {
    if(proc->ip >= proc->size) {
        LOG_WARN("Process %d: Instruction pointer out of bounds\n", proc->pid);
        proc->exit_code = -1;
        proc->active = false;
        return false;
    }
    
    uint8_t opcode = proc->bytecode[proc->ip++];
    
    switch(opcode) {
        // Basic:
        case 0x00: // HALT
            proc->active = false;
            proc->exit_code = 0;
            LOG_DEBUG("Process %d: Halted\n", proc->pid);
            return false;
        
        case 0x01: // NOP
            break;
            
        case 0x02: // PUSH
            if(proc->ip + 3 < proc->size) {
                uint32_t value = (proc->bytecode[proc->ip] << 24) |
                                (proc->bytecode[proc->ip + 1] << 16) |
                                (proc->bytecode[proc->ip + 2] << 8) |
                                proc->bytecode[proc->ip + 3];
                proc->ip += 4;
                
                if(proc->sp < STACK_SIZE) {
                    proc->stack[proc->sp++] = (int32_t)value;
                    
                    // TODO: switch to core/kernel/log.h features
                    /* char dbg[64];
                    serial_print("DEBUG PUSH32: value=0x");
                    itoa(value, dbg, 16);
                    serial_print(dbg);
                    serial_print(" (");
                    itoa((int32_t)value, dbg, 10);
                    serial_print(dbg);
                    serial_print(") at ip=");
                    itoa(proc->ip, dbg, 10);
                    serial_print(dbg);
                    serial_print("\n"); */

                } else {
                    LOG_WARN("Process %d: Stack overflow in PUSH32\n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            } else {
                LOG_WARN("Process %d: Not enough bytes\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x04: // POP
            if(proc->sp > 0) {
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in POP\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x05: // DUP
            if(proc->sp == 0) {
                LOG_WARN("Process %d: Stack underflow in DUP\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            if(proc->sp >= STACK_SIZE) {
                LOG_WARN("Process %d: Stack overflow in DUP\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            
            proc->stack[proc->sp] = proc->stack[proc->sp - 1];
            proc->sp++;
            break;
        
        case 0x06: // SWAP
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                proc->stack[proc->sp - 2] = top;
                proc->stack[proc->sp - 1] = second;
            } else {
                LOG_WARN("Process %d: Stack underflow in SWAP\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        // Arithmetic:
        case 0x10: // ADD
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                int32_t result = top + second; 
                
                proc->stack[proc->sp - 2] = result;
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in ADD\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x11: // SUB
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                int32_t result = second - top;
                
                proc->stack[proc->sp - 2] = result;
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in SUB\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x12: // MUL
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                int32_t result = second * top;
                
                proc->stack[proc->sp - 2] = result;
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in MUL\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x13: // DIV
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                int32_t result;

                if(top != 0) {
                    result = second / top;
                    proc->stack[proc->sp - 2] = result;
                    proc->sp--;
                } else {
                    LOG_WARN("Process %d: Zero division DIV. Terminate process. \n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            } else {
                LOG_WARN("Process %d: Stack underflow in DIV\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x14: // MOD
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                
                if (top == 0) {
                    LOG_WARN("Process %d: Zero division MOD. Terminate process. \n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }

                int32_t result = second % top;
                
                proc->stack[proc->sp - 2] = result;
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in MOD\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;
        
        // Comparisons:
        case 0x20: // CMP
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                int32_t result;

                if(second < top) {
                    result = -1;
                } else if (top == second) {
                    result = 0;
                } else {
                    result = 1;
                }
                
                proc->stack[proc->sp - 2] = result;
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in CMP\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x21: // EQ
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                int32_t result = (top == second) ? 1 : 0;
                
                proc->stack[proc->sp - 2] = result;
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in EQ\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x22: // NEQ
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                int32_t result = (top != second) ? 1 : 0;
                
                proc->stack[proc->sp - 2] = result;
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in NEQ\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x23: // GT
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                int32_t result = (second > top) ? 1 : 0;
                
                proc->stack[proc->sp - 2] = result;
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in GT\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x24: // LT
            if(proc->sp >= 2) {
                int32_t top = proc->stack[proc->sp - 1];
                int32_t second = proc->stack[proc->sp - 2];
                int32_t result = (second < top) ? 1 : 0;
                
                proc->stack[proc->sp - 2] = result;
                proc->sp--;
            } else {
                LOG_WARN("Process %d: Stack underflow in LT\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        // Flow control (32-bit addresses):
        case 0x30: // JMP
            if(proc->ip + 3 < proc->size) {
                uint32_t addr = (proc->bytecode[proc->ip] << 24) |
                               (proc->bytecode[proc->ip + 1] << 16) |
                               (proc->bytecode[proc->ip + 2] << 8) |
                               proc->bytecode[proc->ip + 3];
                proc->ip += 4;
                
                if(addr >= 4 && addr < proc->size) {
                    proc->ip = addr;
                } else {
                    LOG_WARN("Process %d: Invalid address for JMP\n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            }
            break;

        case 0x31: // JZ
            if (proc->sp > 0) {
                int32_t value = proc->stack[--proc->sp];
                if (proc->ip + 3 < proc->size) {
                    uint32_t addr = (proc->bytecode[proc->ip] << 24) |
                                   (proc->bytecode[proc->ip + 1] << 16) |
                                   (proc->bytecode[proc->ip + 2] << 8) |
                                   proc->bytecode[proc->ip + 3];
                    proc->ip += 4;
                    
                    if (value == 0) {
                        if (addr >= 4 && addr < proc->size) {
                            proc->ip = addr;
                        } else {
                            LOG_WARN("Process %d: Invalid address for JZ\n", proc->pid);
                            proc->exit_code = -1;
                            proc->active = false;
                            return false;
                        }
                    }
                } else {
                    LOG_WARN("Process %d: Not enough bytes for address JZ32\n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            } else {
                LOG_WARN("Process %d: Stack underflow in JZ32\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x32: // JNZ
            if (proc->sp > 0) {
                int32_t value = proc->stack[--proc->sp];
                if (proc->ip + 3 < proc->size) {
                    uint32_t addr = (proc->bytecode[proc->ip] << 24) |
                                   (proc->bytecode[proc->ip + 1] << 16) |
                                   (proc->bytecode[proc->ip + 2] << 8) |
                                   proc->bytecode[proc->ip + 3];
                    proc->ip += 4;
                    
                    if (value != 0) {
                        if (addr >= 4 && addr < proc->size) {
                            proc->ip = addr;
                        } else {
                            LOG_WARN("Process %d: Invalid address for JNZ\n", proc->pid);
                            proc->exit_code = -1;
                            proc->active = false;
                            return false;
                        }
                    }
                } else {
                    LOG_WARN("Process %d: Not enough bytes for address JNZ\n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            } else {
                LOG_WARN("Process %d: Stack underflow in JNZ32\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x33: // CALL
            if(proc->ip + 3 < proc->size) {
                int32_t addr = (proc->bytecode[proc->ip] << 24) |
                               (proc->bytecode[proc->ip + 1] << 16) |
                               (proc->bytecode[proc->ip + 2] << 8) |
                               proc->bytecode[proc->ip + 3];
                proc->ip += 4;
                
                if(proc->sp < STACK_SIZE - 1) {
                    proc->stack[proc->sp++] = proc->ip;
                    
                    if(addr >= 4 && addr < proc->size) {
                        proc->ip = addr;
                    } else {
                        LOG_WARN("Process %d: Invalid address for CALL\n", proc->pid);
                        proc->exit_code = -1;
                        proc->active = false;
                        return false;
                    }
                } else {
                    LOG_WARN("Process %d: Stack overflow in CALL\n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            } else {
                LOG_WARN("Process %d: Not enough bytes for address CALL\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        case 0x34: // RET
            if(proc->sp > 0) {
                uint32_t return_addr = (int32_t)proc->stack[--proc->sp];
                
                if(return_addr >= 4 && return_addr < proc->size) {
                    proc->ip = return_addr;
                } else {
                    LOG_WARN("Process %d: invalid return address\n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            } else {
                LOG_WARN("Process %d: stack underflow in RET\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        // Memory:
        case 0x40: // LOAD
            if(proc->ip < proc->size) {
                uint8_t var_index = proc->bytecode[proc->ip++];
                
                if(var_index < MAX_LOCALS) {
                    int32_t value = proc->locals[var_index];
                    
                    if(proc->sp < STACK_SIZE) {
                        proc->stack[proc->sp++] = value;
                    } else {
                        LOG_WARN("Process %d: Stack overflow in LOAD\n", proc->pid);
                        proc->exit_code = -1;
                        proc->active = false;
                        return false;
                    }
                } else {
                    LOG_WARN("Process %d: invalid variable index in LOAD\n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            }
            break;

        case 0x41: // STORE
            if(proc->ip < proc->size) {
                uint8_t var_index = proc->bytecode[proc->ip++];
                
                if(var_index < MAX_LOCALS && proc->sp > 0) {
                    int32_t value = proc->stack[--proc->sp];
                    proc->locals[var_index] = value;
                } else {
                    LOG_WARN("Process %d: invalid index or stack underflow in STORE\n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            }
            break;

        // Memory absolute access
        case 0x45: // STORE_ABS - store to absolute memory address
            if (!caps_has_capability(proc, CAP_DRV_ACCESS)) {
                LOG_WARN("Procces %d: Required caps not receivedn\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }

            if(proc->sp >= 2) {
                uint32_t addr = (uint32_t)proc->stack[proc->sp - 2]; // address
                int32_t value = proc->stack[proc->sp - 1]; // value

                if((addr >= 0x100000 && addr < 0xFFFFFFFF) || 
                (addr >= 0xB8000 && addr <= 0xB8FA0)) {
                    // Special handling for VGA text buffer - write only 16 bits (char + attribute)
                    if (addr >= 0xB8000 && addr <= 0xB8FA0) {
                        *(uint16_t*)addr = (uint16_t)(value & 0xFFFF);
                    } else {
                        *(int32_t*)addr = value;
                    }
                    proc->sp -= 2;
                } else {
                    LOG_WARN("Procces %d: Invalid memory address in STORE_ABS\n", proc->pid);
                    proc->exit_code = -1;
                    proc->active = false;
                    return false;
                }
            } else {
                LOG_WARN("Procces %d: Stack underflow in STORE_ABS\n", proc->pid);
                proc->exit_code = -1;
                proc->active = false;
                return false;
            }
            break;

        // System calls:
        case 0x50: // SYSCALL
            if(proc->ip < proc->size) {
                uint8_t syscall_id = proc->bytecode[proc->ip++];
                syscall_handler(syscall_id, proc);
            }
            break;

        // System calls:
        case 0x51: // BREAK
            LOG_DEBUG("Process %d: Stop from BREAK at IP=%d, SP=%d\n", proc->pid, proc->ip, proc->sp);
            break;
            
        default:
            char buffer[32];
            LOG_WARN("Process %d: Unknown opcode: 0x%s", proc->pid, buffer);
            proc->exit_code = -1;
            proc->active = false;
            return false;
    }
    
    return true;
}

void nvm_execute(uint8_t* bytecode, uint32_t size, uint16_t* capabilities, uint8_t caps_count) {
    int pid = nvm_create_process(bytecode, size, capabilities, caps_count);
    if(pid >= 0) {
        LOG_INFO("NVM process started with PID: %d\n", pid);

        // Execute the process until completion
        while(processes[pid].active) {
            if(!nvm_execute_instruction(&processes[pid])) {
                break;
            }
        }

        LOG_INFO("NVM process %d finished with exit code: %d\n", pid, processes[pid].exit_code);
    } else {
        LOG_ERROR("Failed to create NVM process\n");
    }
}

// Function for get exit code
int32_t nvm_get_exit_code(uint8_t pid) {
    if(pid < MAX_PROCESSES && !processes[pid].active) {
        return processes[pid].exit_code;
    }
    return -1;
}

// Function for check process activity
bool nvm_is_process_active(uint8_t pid) {
    if(pid < MAX_PROCESSES) {
        return processes[pid].active;
    }
    return false;
}

int main(int argc, char* argv[]) {
    if(argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s [--log <output>] <bytecode_file>\n", argv[0]);
        fprintf(stderr, "  --log file    : Log to 'nvm.log' file\n");
        fprintf(stderr, "  --log stdio   : Log to stdout (default)\n");
        fprintf(stderr, "  --log no      : Disable logging\n");
        return 1;
    }

    const char* filename = NULL;
    log_output_t log_output = LOG_OUTPUT_FILE;
    const char* log_filename = "nvm.log";

    // Parse arguments
    int arg_index = 1;
    while (arg_index < argc) {
        if (strcmp(argv[arg_index], "--log") == 0) {
            if (arg_index + 1 >= argc) {
                fprintf(stderr, "Error: --log requires an argument\n");
                return 1;
            }

            const char* log_arg = argv[arg_index + 1];
            if (strcmp(log_arg, "file") == 0) {
                log_output = LOG_OUTPUT_FILE;
            } else if (strcmp(log_arg, "stdio") == 0) {
                log_output = LOG_OUTPUT_STDOUT;
            } else if (strcmp(log_arg, "no") == 0) {
                log_output = LOG_OUTPUT_NONE;
            } else {
                fprintf(stderr, "Error: Invalid --log argument: %s\n", log_arg);
                fprintf(stderr, "Valid options: file, stdio, no\n");
                return 1;
            }
            arg_index += 2;
        } else {
            // This should be the filename
            if (filename != NULL) {
                fprintf(stderr, "Error: Multiple filenames specified\n");
                return 1;
            }
            filename = argv[arg_index];
            arg_index++;
        }
    }

    if (filename == NULL) {
        fprintf(stderr, "Error: No bytecode file specified\n");
        return 1;
    }

    // Configure logging
    log_set_output(log_output, log_filename);

    // Open and read the bytecode file
    FILE* file = fopen(filename, "rb");
    if(!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return 1;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if(file_size < 4) {
        fprintf(stderr, "Error: File too small to contain NVM bytecode\n");
        fclose(file);
        return 1;
    }

    // Allocate memory for bytecode
    int8_t* bytecode = (int8_t*)malloc(file_size);
    if(!bytecode) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return 1;
    }

    // Read the file
    size_t bytes_read = fread(bytecode, 1, file_size, file);
    fclose(file);

    if(bytes_read != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read entire file\n");
        free(bytecode);
        return 1;
    }

    // Initialize NVM
    nvm_init();

    // Execute the bytecode with no special capabilities
    int16_t capabilities[1] = {CAPS_NONE};
    nvm_execute(bytecode, file_size, capabilities, 1);

    // Cleanup
    free(bytecode);

    return 0;
}