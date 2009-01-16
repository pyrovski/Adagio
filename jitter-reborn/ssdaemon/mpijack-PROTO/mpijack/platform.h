#ifdef _X86
// should work with gcc on x86 from what I can tell
//#define CALLER_ADDRESS(r) asm("movl %%esp, %0;" :"=r"(r))


#define CALLER_ADDRESS(r) \
                asm("push %esi"); \
                asm("movl (%ebp), %esi"); \
                asm("movl 4(%%esi), %0;" : "=r"(r)); \
                asm("popl %esi");


//#define CALLER_ADDRESS(r) asm("movl 4(%%ebx), %0;" :"=r"(r))

#define MARK_CALL_ADDRESS1(r) asm("movl 4(%%ebp), %0;" :"=r"(r));

#endif

/*
 *
int a=10, b;
        asm ("movl %1, %%eax; 
              movl %%eax, %0;"
             :"=r"(b)         output 
             :"r"(a)          input
             :"%eax"          clobbered register
             );
 */

