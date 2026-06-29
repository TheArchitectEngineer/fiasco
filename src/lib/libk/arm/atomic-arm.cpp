IMPLEMENTATION [arm && !arm_v6plus]:

template<typename T> inline
bool
local_cas(T *mem, T oldval, T newval)
{
  static_assert(sizeof(T) == sizeof(Mword));

  Mword ret;
  asm volatile("    mrs    r5, cpsr    \n"
               "    mov    r6,r5       \n"
               "    orr    r6,r6,#0xc0 \n"
               "    msr    cpsr_c, r6  \n"
               ""
               "    ldr    r6, [%1]    \n"
               "    cmp    r6, %2      \n"
               "    streq  %3, [%1]    \n"
               "    moveq  %0, #1      \n"
               "    movne  %0, #0      \n"
               "    msr    cpsr_c, r5  \n"
               : "=r" (ret)
               : "r"  (mem), "r" (oldval), "r" (newval)
               : "r5", "r6", "memory");
  return ret;
}

inline
void
local_atomic_and(Mword *mem, Mword value)
{
  Mword old;
  do { old = *mem; }
  while (!local_cas(mem, old, old & value));
}

inline
void
local_atomic_or(Mword *mem, Mword value)
{
  Mword old;
  do { old = *mem; }
  while (!local_cas(mem, old, old | value));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

#include "mem.h"

template<typename T> inline NEEDS["mem.h"]
bool
local_cas(T *mem, T oldval, T newval)
{
  Mem::barrier();
  bool res = cas_relaxed(mem, oldval, newval);
  Mem::barrier();
  return res;
}

inline NEEDS["mem.h"]
void
local_atomic_or(Mword *mem, Mword value)
{
  Mem::barrier();
  atomic_or_relaxed(mem, value);
  Mem::barrier();
}

inline NEEDS["mem.h"]
void
local_atomic_and(Mword *mem, Mword value)
{
  Mem::barrier();
  atomic_and_relaxed(mem, value);
  Mem::barrier();
}
