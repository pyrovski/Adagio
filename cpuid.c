#include <stdint.h>

static inline uint64_t native_read_tscp(uint32_t *aux)
{
  unsigned long low, high;
  asm volatile(".byte 0x0f,0x01,0xf9"
	       : "=a" (low), "=d" (high), "=c" (*aux));
  return low | ((uint64_t)high << 32);
}

uint64_t getcpu(unsigned *cpu, unsigned *node)
{
  uint32_t p;

  //if (*vdso_vgetcpu_mode == VGETCPU_RDTSCP) {
  /* Load per CPU data from RDTSCP */
  native_read_tscp(&p);
  //} else {
  /* Load per CPU data from GDT */
  //asm("lsl %1,%0" : "=r" (p) : "r" (__PER_CPU_SEG));
  //}
  if (cpu)
    *cpu = p & 0xfff;
  if (node)
    *node = p >> 12;
  return 0;
}

uint32_t get_cpuid(){
  uint32_t cpu;
  if(getcpu(&cpu, 0))
    return -1;
  return cpu;
}
