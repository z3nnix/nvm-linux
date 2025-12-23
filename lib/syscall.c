#include <syscall.h>
#include <log.h>
#include <stdio.h>

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

        default:
            LOG_WARN("Process %d: Unknown syscall %d\n", proc->pid, syscall_id);
            proc->exit_code = -1;
            proc->active = false;
            return -1;
    }

    return 0;
}
