#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

// After building this program, you can use
//    $ objdump -d target > target.s
// to disassemble the binary. This tells us
// at what line of code is the second printf.
int main(int argc, char** argv)
{
    int x = 10;
    printf("%s: x = %d\n", argv[0], x);
    // When this program is run by the program "debugger", the value of 
    // x will be changed mid-execution, to be set to 202. 
    printf("%s: x = %d\n", argv[0], x);
    return 0; 
}
