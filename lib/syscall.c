#include <syscall.h>
#include <log.h>
#include <stdio.h>

int8_t value;

int32_t syscall_handler(int8_t syscall_id, nvm_process_t* proc) {
    switch(syscall_id) {
        case SYSCALL_EXIT:
            // Exit with code from stack, or 0 if stack is empty
            if(proc->sp > 0) {
                proc->exit_code = proc->stack[--proc->sp];
            } else {
                proc->exit_code = 0;
            }
            proc->active = false;
            LOG_DEBUG("Process %d: Exited with code %d\n", proc->pid, proc->exit_code);
            break;

        case SYSCALL_PRINT: // temporary, will be replace for /dev/console
            if (proc->sp < 1) {
                LOG_WARN("Process %d: Stack underflow for print\n", proc->pid);
                return -1;
            }

            value = proc->stack[proc->sp - 1] & 0xFF;
            printf("%c", (char)value);
            proc->sp -= 1;
            break;
        default:
            LOG_WARN("Process %d: Unknown syscall %d\n", proc->pid, syscall_id);
            proc->exit_code = -1;
            proc->active = false;
            return -1;
    }

    return 0;
}
