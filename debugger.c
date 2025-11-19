// debugger.c: Simple "debugging" program intended to show
//    how one process (this one) can manipulate another
//    one. This program is hard-coded to work with a
//    target process called "target". It is also hard-coded
//    to set a breakpoint before the target's second printf,
//    and at that point, to modify the variable 'x' in the target
//    to be equal to 202.

#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

void wait_for_target(int pid);
void continue_on(int pid);
void single_step(int pid);
uint64_t set_breakpoint(int pid, uint64_t addr);
void preserve_brkpoint_and_continue(int pid, uint64_t addr, uint64_t orig_inst);
void rewind_rip(int pid);
void set_x_in_target(int pid, uint32_t val);
void show_target_breakpoint(int pid, uint64_t addr);

// We inspect the target binary to learn at what address the value of 'x' (in
// the target) is read from the stack prior to the second printf.
// That is the address below. That's a hack. A real debugger would infer
// the address by examining debugging information (symbol tables and so on).
#define STACK_LOAD 0x4018a7

int main(int argc, char** argv)
{
    int pid = fork();
    if (pid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        if (execl("target", "target", NULL) < 0)
            perror("exec()");
    }

    if (pid < 0)
       perror("failed to fork\n");

    wait_for_target(pid);

    // When setting a breakpoint, we have to keep around the 
    // original contents of the target's code at the memory location.
    uint64_t original = set_breakpoint(pid, STACK_LOAD);
    show_target_breakpoint(pid, STACK_LOAD);
    continue_on(pid);

    set_x_in_target(pid, 202);

    preserve_brkpoint_and_continue(pid, STACK_LOAD, original);

    return 0;

}

void wait_for_target(int pid)
{
    int status;

    while (1) {

        waitpid(pid, &status, 0);

        if (WIFSTOPPED(status)) {
            if (WSTOPSIG(status) != SIGTRAP &&
                WSTOPSIG(status) != SIGSTOP)  {
                    printf("target stopped due to signal: %d\n", WSTOPSIG(status));
            }
            break;
        } else if (WIFEXITED(status)) {
            printf("target exited\n");
            break;
        }
    }
}

void continue_on(int pid)
{
    if (ptrace(PTRACE_CONT, pid, NULL, NULL) < 0)
        perror("ptrace continue");

    wait_for_target(pid);
}


void single_step(int pid)
{
   if (ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL) < 0)
        perror("ptrace singlestep");

   wait_for_target(pid);
}

uint64_t set_breakpoint(int pid, uint64_t addr)
{
    uint64_t orig_instruction = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);

    // The next lines insert an instruction in the target that raises an
    // exception. Specifically, on x86, 0xcc is a special instruction that
    // causes the CPU to raise the "breakpoint exception". 
    uint64_t orig_upper_bytes = orig_instruction & ~(uint64_t)0xff;

    if (ptrace(PTRACE_POKEDATA, pid, addr, orig_upper_bytes | 0xcc) < 0)
        perror("ptrace pokedata");

   return orig_instruction;
}

void preserve_brkpoint_and_continue(int pid, uint64_t addr, uint64_t orig_inst)
{
    // Write the original instruction back so it can execute
    if (ptrace(PTRACE_POKEDATA, pid, addr, orig_inst) < 0)
        perror("ptrace pokedata");

    // Right here, %rip is one past the instruction we wish to re-execute. 
    // The function below puts %rip where it should be.
    rewind_rip(pid);

    // Execute the restored instruction
    single_step(pid);

    // At this point, the target is past the breakpoint, so 
    // set the breakpoint again, and continue. If this were a real debugger
    // we would have to capture the return value of set_breakpoint, to be able
    // to (again) restore the original instruction, in case it's
    // different versus when we first captured it.
    set_breakpoint(pid, addr); 
    continue_on(pid);
}

// Read %rip (with all other registers), decrement it in the local 
// data structure, and then set all of the registers, with the updated %rip.
void rewind_rip(int pid)
{
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0)
        perror("ptrace getregs");

    printf("%%rip in target is 0x%llx but we want it to be 0x%x\n", regs.rip, STACK_LOAD);

    regs.rip--;

    if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) < 0)
        perror("ptrace setregs");
}

// This function sets the variable 'x' in the target, which
// lives at the address 4 bytes below the frame pointer.
// Although what we are trying to do is conceptually 
// straightforward, the code winds up being complicated
// by the fact that PEEK_DATA and POKE_DATA read only in 64-bit
// quantities. So the code has to take care to preserve the stuff "before"
// and "after" the relevant slot, which is only 32 bits. 
void set_x_in_target(int pid, uint32_t newval)
{
    struct user_regs_struct regs;
    uint64_t x_in_stack;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0)
        perror("ptrace getregs");

    // Read all 64 bits from the relevant location in the target's stack frame
    // and display the bottom 32 bits.
    x_in_stack = ptrace(PTRACE_PEEKDATA, pid, regs.rbp - 4, NULL);
    printf("Checking: *(%%rbp-4): %ld\n", x_in_stack & 0xffffffff);

    // Zero out the bottom four bytes and then set those bytes to be newval.
    x_in_stack &= ~(uint64_t)0xffffffff;  
    x_in_stack |= newval;                 

    // Write 64 bits into the target's stack frame
    if (ptrace(PTRACE_POKEDATA, pid, (uint64_t)regs.rbp - 4, x_in_stack) < 0)
        perror("ptrace pokedata");

    x_in_stack = ptrace(PTRACE_PEEKDATA, pid, regs.rbp - 4, NULL);
    printf("Checking: *(%%rbp-4): %ld\n", x_in_stack & 0xffffffff);
}

void show_target_breakpoint(int pid, uint64_t addr)
{
    uint64_t instruction = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);
    printf("instruction at addr 0x%lx is now: 0x%lx\n", addr, instruction);
}

