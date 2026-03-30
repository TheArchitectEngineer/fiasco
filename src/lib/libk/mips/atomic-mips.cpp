INTERFACE [mips]:

#include "asm_mips.h"
#include "mem.h"

// preprocess off
#define ATOMIC_OP(name, op)                                                    \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  void                                                                         \
  atomic_##name##_relaxed(T *mem, V value)                                     \
  {                                                                            \
    T val = value;                                                             \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            ASM_LL " %[tmp], %[mem]  \n"                                       \
            op    " %[tmp], %[val] \n"                                         \
            ASM_SC " %[tmp], %[mem]  \n"                                       \
            : [tmp] "=&r" (tmp), [mem] "+ZC" (*mem)                            \
            : [val] "Ir" (val));                                               \
      }                                                                        \
    while (!tmp);                                                              \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  T                                                                            \
  atomic_fetch_##name##_relaxed(T *mem, V value)                               \
  {                                                                            \
    T val = value;                                                             \
    T old;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            ASM_LL   " %[tmp], %[ptr]  \n"                                     \
            "move      %[old], %[tmp]  \n"                                     \
            op      " %[tmp], %[val]    \n"                                    \
            ASM_SC   " %[tmp], %[ptr]  \n"                                     \
            : [tmp] "=&r" (tmp), [ptr] "+ZC" (*mem), [old] "=&r"(old)          \
            : [val] "Ir" (val));                                               \
      }                                                                        \
    while (!tmp);                                                              \
    return old;                                                                \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  T                                                                            \
  atomic_##name##_fetch_relaxed(T *mem, V value)                               \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            ASM_LL   " %[tmp], %[ptr]  \n"                                     \
            op      " %[tmp], %[val]    \n"                                    \
            "move      %[res], %[tmp]  \n"                                     \
            ASM_SC   " %[tmp], %[ptr]  \n"                                     \
            : [tmp] "=&r" (tmp), [ptr] "+ZC" (*mem), [res] "=&r"(res)          \
            : [val] "Ir" (val));                                               \
      }                                                                        \
    while (!tmp);                                                              \
    return res;                                                                \
  }
ATOMIC_OP(or, "or")
ATOMIC_OP(and, "and")
ATOMIC_OP(add, ASM_ADDU)
#undef ATOMIC_OP
// preprocess on

template<typename T> requires(sizeof(T) == 4) inline
T
atomic_load_relaxed(T const *mem)
{
  T res;
  __asm__ __volatile__ ("lw %0, %1" : "=r" (res) : "m"(*mem));
  return res;
}

template<typename T> requires(sizeof(T) == 8) inline
T
atomic_load_relaxed(T const *mem)
{
  T res;
  __asm__ __volatile__ ("ld %0, %1" : "=r" (res) : "m"(*mem));
  return res;
}

template<typename T, typename V> requires(sizeof(T) == 4) inline
void
atomic_store_relaxed(T *mem, V value)
{
  T val = value;
  __asm__ __volatile__ ("sw %1, %0" : "=m" (*mem) : "r" (val));
}

template<typename T, typename V> requires(sizeof(T) == 8) inline
void
atomic_store_relaxed(T *mem, V value)
{
  T val = value;
  __asm__ __volatile__ ("sd %1, %0" : "=m" (*mem) : "r" (val));
}

template<typename T> inline
T
atomic_load_acquire(T const *mem)
{
  T res = atomic_load_relaxed(mem);
  Mem::mp_mb();
  return res;
}

template<typename T> inline
T
atomic_load_seq_cst(T const *mem)
{
  Mem::mp_mb();
  T res = atomic_load_relaxed(mem);
  Mem::mp_mb();
  return res;
}

template<typename T, typename V> inline
void
atomic_store_release(T *mem, V value)
{
  Mem::mp_mb();
  atomic_store_relaxed(mem, value);
}

template<typename T, typename V> inline
void
atomic_store_seq_cst(T *mem, V value)
{
  Mem::mp_mb();
  atomic_store_relaxed(mem, value);
  Mem::mp_mb();
}

template<typename T, typename V>
requires(sizeof(T) == sizeof(Mword)) inline
T
atomic_exchange_relaxed(T *mem, V value)
{
  T val = value;
  T old;
  Mword tmp;

  do
    {
      __asm__ __volatile__(
          ASM_LL   " %[old], %[ptr]  \n"
          "move      %[tmp], %[val]  \n"
          ASM_SC   " %[tmp], %[ptr]  \n"
          : [tmp] "=&r" (tmp), [ptr] "+ZC" (*mem), [old] "=&r"(old)
          : [val] "r" (val));
    }
  while (!tmp);
  return old;
}

inline NEEDS["asm_mips.h"]
bool
cas_arch_relaxed(Mword *ptr, Mword oldval, Mword newval)
{
  Mword ret;

  __asm__ __volatile__(
      "     .set    push                \n"
      "     .set    noat    #CAS        \n"
      "     .set    noreorder           \n"
      "1:   " ASM_LL " %[ret], %[ptr]   \n"
      "     bne     %[ret], %z[old], 2f \n"
      "       move $1, %z[newval]       \n"
      "     " ASM_SC " $1, %[ptr]       \n"
      "     beqz    $1, 1b              \n"
      "       nop                       \n"
      "2:                               \n"
      "     .set    pop                 \n"
      : [ret] "=&r" (ret), [ptr] "+ZC" (*ptr)
      : [old] "Jr" (oldval), [newval] "Jr" (newval)
      : "memory"); // for some unknown reason this is necessary for newer
                   // gcc compilers

  // true is ok
  // false is failed
  return ret == oldval;
}

// preprocess off
#define WRAP_ATOMIC_OP_VOID(name, order, pre, post)                            \
  template<typename T, typename V>  inline                                     \
  void                                                                         \
  name##_##order(T *mem, V value)                                              \
  {                                                                            \
    pre;                                                                       \
    name ## _relaxed(mem, value);                                              \
    post;                                                                      \
  }

#define WRAP_ATOMIC_OP_RET(name, order, pre, post)                             \
  template<typename T, typename V>  inline                                     \
  T                                                                            \
  name##_##order(T *mem, V value)                                              \
  {                                                                            \
    pre;                                                                       \
    T res = name ## _relaxed(mem, value);                                      \
    post;                                                                      \
    return res;                                                                \
  }

#define WRAP_ATOMIC_OP_CAS(name, order, pre, post)                             \
  inline                                                                       \
  bool                                                                         \
  name##_##order(Mword *ptr, Mword oldval, Mword newval)                       \
  {                                                                            \
    pre;                                                                       \
    Mword res = name ## _relaxed(ptr, oldval, newval);                         \
    post;                                                                      \
    return res;                                                                \
  }

#define ATOMIC_IMPL_VARIANTS(OP_MACRO, name)                                   \
  OP_MACRO(name, acquire,             , Mem::mp_mb())                          \
  OP_MACRO(name, release, Mem::mp_mb(),             )                          \
  OP_MACRO(name, acq_rel, Mem::mp_mb(), Mem::mp_mb())                          \
  OP_MACRO(name, seq_cst, Mem::mp_mb(), Mem::mp_mb())

ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_VOID, atomic_and)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_VOID, atomic_or)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_VOID, atomic_add)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_fetch_and)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_fetch_or)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_fetch_add)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_and_fetch)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_or_fetch)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_add_fetch)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_exchange)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_CAS, cas_arch)

#undef ATOMIC_IMPL_VARIANTS
#undef ATOMIC_OP_VOID
#undef ATOMIC_OP_RET
#undef ATOMIC_OP_CAS
// preprocess on

//---------------------------------------------------------------------------
IMPLEMENTATION [mips]:

#include "mem.h"

inline
void
local_atomic_and(Mword *mem, Mword value)
{
  atomic_and_relaxed<Mword, Mword>(mem, value);
}

inline
void
local_atomic_or(Mword *mem, Mword value)
{
  atomic_or_relaxed<Mword, Mword>(mem, value);
}

inline
void
local_atomic_add(Mword *mem, Mword value)
{
  atomic_add_relaxed<Mword, Mword>(mem, value);
}

// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.
inline
bool
local_cas_unsafe(Mword *m, Mword o, Mword n)
{ return cas_arch_relaxed(m, o, n); }
