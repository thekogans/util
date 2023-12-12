// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_util.
//
// libthekogans_util is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_util is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_util. If not, see <http://www.gnu.org/licenses/>.

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include <intrin.h>
#elif defined (TOOLCHAIN_ARCH_ppc32) || defined (TOOLCHAIN_ARCH_ppc64)
    #if defined (TOOLCHAIN_OS_Linux)
        #include <features.h>
        #include <signal.h>
        #include <setjmp.h>
    #elif defined (TOOLCHAIN_OS_OSX)
        #include <sys/sysctl.h>
    #endif // defined (TOOLCHAIN_OS_Linux)
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cstring>
#include "thekogans/util/FixedArray.h"
#include "thekogans/util/CPU.h"

namespace thekogans {
    namespace util {

        namespace {
        #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        #if defined (TOOLCHAIN_OS_Windows)
        #if defined (TOOLCHAIN_ARCH_i386)
            bool HaveCPUID () {
                int haveCPUID = 0;
                __asm {
                    pushfd            ; Get original EFLAGS
                    pop eax
                    mov ecx, eax
                    xor eax, 200000h  ; Flip ID bit in EFLAGS
                    push eax          ; Save new EFLAGS value on stack
                    popfd             ; Replace current EFLAGS value
                    pushfd            ; Get new EFLAGS
                    pop eax           ; Store new EFLAGS in EAX
                    xor eax, ecx      ; Can not toggle ID bit,
                    jz done           ; Processor = 80486
                    mov haveCPUID, 1  ; We have CPUID support
                done:
                }
                return haveCPUID == 1;
            }
        #else // defined (TOOLCHAIN_ARCH_i386)
            bool HaveCPUID () {
                return true;
            }
        #endif // defined (TOOLCHAIN_ARCH_i386)
        #else // defined (TOOLCHAIN_OS_Windows)
        #if defined (TOOLCHAIN_ARCH_i386)
            bool HaveCPUID () {
                int haveCPUID = 0;
                __asm__ volatile (
                    "        pushfl                      # Get original EFLAGS             \n"
                    "        popl    %%eax                                                 \n"
                    "        movl    %%eax,%%ecx                                           \n"
                    "        xorl    $0x200000,%%eax     # Flip ID bit in EFLAGS           \n"
                    "        pushl   %%eax               # Save new EFLAGS value on stack  \n"
                    "        popfl                       # Replace current EFLAGS value    \n"
                    "        pushfl                      # Get new EFLAGS                  \n"
                    "        popl    %%eax               # Store new EFLAGS in EAX         \n"
                    "        xorl    %%ecx,%%eax         # Can not toggle ID bit,          \n"
                    "        jz      1f                  # Processor=80486                 \n"
                    "        movl    $1,%0               # We have CPUID support           \n"
                    "1:                                                                    \n"
                    : "=m" (haveCPUID)
                    :
                    : "%eax", "%ecx");
                return haveCPUID == 1;
            }
        #else // defined (TOOLCHAIN_ARCH_i386)
            bool HaveCPUID () {
                int haveCPUID = 0;
                __asm__ volatile (
                    "        pushfq                      # Get original EFLAGS             \n"
                    "        popq    %%rax                                                 \n"
                    "        movq    %%rax,%%rcx                                           \n"
                    "        xorl    $0x200000,%%eax     # Flip ID bit in EFLAGS           \n"
                    "        pushq   %%rax               # Save new EFLAGS value on stack  \n"
                    "        popfq                       # Replace current EFLAGS value    \n"
                    "        pushfq                      # Get new EFLAGS                  \n"
                    "        popq    %%rax               # Store new EFLAGS in EAX         \n"
                    "        xorl    %%ecx,%%eax         # Can not toggle ID bit,          \n"
                    "        jz      1f                  # Processor=80486                 \n"
                    "        movl    $1,%0               # We have CPUID support           \n"
                    "1:                                                                    \n"
                    : "=m" (haveCPUID)
                    :
                    : "%rax", "%rcx");
                return haveCPUID == 1;
            }
        #endif // defined (TOOLCHAIN_ARCH_i386)
            inline void __cpuid (
                    int registers[4],
                    int function) {
                __asm__ volatile (
                    "cpuid"
                    : "=a" (registers[0]), "=b" (registers[1]), "=c" (registers[2]), "=d" (registers[3])
                    : "a" (function));
            }

            inline void __cpuidex (
                    int registers[4],
                    int function,
                    int subfunction) {
                __asm__ volatile (
                    "cpuid"
                    : "=a" (registers[0]), "=b" (registers[1]), "=c" (registers[2]), "=d" (registers[3])
                    : "a" (function), "c" (subfunction));
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        #elif defined (TOOLCHAIN_ARCH_ppc32) || defined (TOOLCHAIN_ARCH_ppc64)
        #if defined (TOOLCHAIN_OS_Linux)
            jmp_buf jmpbuf;

            void IllegalInstruction (int /*sig*/) {
                longjmp (jmpbuf, 1);
            }

            bool HaveAltiVec () {
                volatile bool altivec = false;
                void (*handler) (int /*sig*/) = signal (SIGILL, IllegalInstruction);
                if (setjmp (jmpbuf) == 0) {
                    __asm__ volatile (
                        "mtspr 256, %0\n\t" "vand %%v0, %%v0, %%v0"
                        :
                        :"r" (-1));
                    altivec = true;
                }
                signal (SIGILL, handler);
                return altivec;
            }
        #elif defined (TOOLCHAIN_OS_OSX)
            bool HaveAltiVec () {
                ui32 hasVectorUnit = 0;
                int selectors[2] = {CTL_HW, HW_VECTORUNIT};
                size_t length = sizeof (hasVectorUnit);
                sysctl (selectors, 2, &hasVectorUnit, &length, 0, 0);
                return hasVectorUnit != 0;
            }
        #else // defined (TOOLCHAIN_OS_Linux)
            bool HaveAltiVec () {
                return false;
            }
        #endif // defined (TOOLCHAIN_OS_Linux)
        #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        }

    #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        CPU::CPU () :
                isIntel (false),
                isAMD (false),
                l1CacheLineSize (0),
                f_1_ECX (0),
                f_1_EDX (0),
                f_7_EBX (0),
                f_7_ECX (0),
                f_81_ECX (0),
                f_81_EDX (0) {
            if (HaveCPUID ()) {
                {
                    // Calling __cpuid with 0x0 as the function argument
                    // gets the number of the highest valid function ID in
                    // registers[0] and the vendor string in registers[1-3].
                    FixedArray<ui32, 4> registers;
                    __cpuid ((int *)registers.array, 0);
                    ui32 functionCount = registers.array[0];
                    // Capture vendor string.
                    ui32 vendor_[] = {
                        registers.array[1],
                        registers.array[3],
                        registers.array[2],
                        0
                    };
                    vendor = (const char *)vendor_;
                    if (vendor == "GenuineIntel") {
                        isIntel = true;
                    }
                    else if (vendor == "AuthenticAMD") {
                        isAMD = true;
                    }
                    // Load flags for function 0x00000001.
                    if (functionCount >= 1) {
                        __cpuidex ((int *)registers.array, 1, 0);
                        // If on Intel, get the L1 cache line size.
                        if (isIntel) {
                            l1CacheLineSize = (((registers.array[1] >> 8) & 0xff) * 8);
                        }
                        f_1_ECX = registers.array[2];
                        f_1_EDX = registers.array[3];
                    }
                    // Load flags for function 0x00000007.
                    if (functionCount >= 7) {
                        __cpuidex ((int *)registers.array, 7, 0);
                        f_7_EBX = registers.array[1];
                        f_7_ECX = registers.array[2];
                    }
                }
                {
                    // Calling __cpuid with 0x80000000 as the function argument
                    // gets the number of the highest valid extended ID in
                    // registers.array[0].
                    FixedArray<ui32, 4> registers;
                    __cpuid ((int *)registers.array, 0x80000000);
                    ui32 functionCount = registers.array[0];
                    // Load flags for function 0x80000001.
                    if (functionCount >= 0x80000001) {
                        __cpuidex ((int *)registers.array, 0x80000001, 0);
                        f_81_ECX = registers.array[2];
                        f_81_EDX = registers.array[3];
                    }
                    // Interpret CPU brand string if reported.
                    if (functionCount >= 0x80000004) {
                        FixedArray<ui32, 4> brand_[] = {
                            FixedArray<ui32, 4> (0),
                            FixedArray<ui32, 4> (0),
                            FixedArray<ui32, 4> (0),
                            FixedArray<ui32, 4> (0)
                        };
                        __cpuidex ((int *)brand_[0].array, 0x80000002, 0);
                        __cpuidex ((int *)brand_[1].array, 0x80000003, 0);
                        __cpuidex ((int *)brand_[2].array, 0x80000004, 0);
                        brand = (const char *)brand_;
                    }
                    // If on AMD, get the L1 cache line size.
                    if (isAMD && functionCount >= 0x80000005) {
                        __cpuidex ((int *)registers.array, 0x80000005, 0);
                        l1CacheLineSize = registers.array[2] & 0xff;
                    }
                }
            }
        }
    #elif defined (TOOLCHAIN_ARCH_ppc32) || defined (TOOLCHAIN_ARCH_ppc64)
        CPU::CPU () :
            isAltiVec (HaveAltiVec ()) {}
    #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)

    #if defined (TOOLCHAIN_COMPILER_cl)
    #if defined (TOOLCHAIN_ARCH_x86_64)
        #if !defined (YieldProcessor)
            extern "C" void _mm_pause (void);
            #pragma intrinsic (_mm_pause)
            #define YieldProcessor _mm_pause
        #endif // !defined (YieldProcessor)
        #if !defined (MemoryBarrier)
            extern "C" void _mm_mfence (void);
            #pragma intrinsic (_mm_mfence)
            #define MemoryBarrier _mm_mfence
        #endif // !defined (MemoryBarrier)
    #elif defined (TOOLCHAIN_ARCH_i386)
        #if !defined (YieldProcessor)
            #define YieldProcessor() __asm {rep nop}
        #endif // !defined (YieldProcessor)
        #if !defined (MemoryBarrier)
            #define MemoryBarrier() MemoryBarrierImpl ()
            __forceinline void MemoryBarrierImpl () {
                int32_t Barrier;
                __asm {
                    xchg Barrier, eax
                }
            }
        #endif // !defined (MemoryBarrier)
    #elif defined (TOOLCHAIN_ARCH_arm64)
        extern "C" void __yield (void);
        #pragma intrinsic (__yield)
        __forceinline void YieldProcessor () {\
            __yield();\
        }
        extern "C" void __dmb (const unsigned __int32 _Type);
        #pragma intrinsic (__dmb)
        #define MemoryBarrier() {\
            __dmb (_ARM64_BARRIER_SY);\
        }
    #elif defined (TOOLCHAIN_ARCH_arm32)
        __forceinline void YieldProcessor () {}
        extern "C" void __emit (const unsigned __int32 opcode);
        #pragma intrinsic (__emit)
        #define MemoryBarrier() {\
            __emit (0xF3BF);\
            __emit (0x8F5F);\
        }
    #else // defined (TOOLCHAIN_ARCH_x86_64)
        #error Unsupported architecture
    #endif // defined (TOOLCHAIN_ARCH_arm32)
    #else // defined (TOOLCHAIN_COMPILER_cl)
    #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        #if defined (TOOLCHAIN_OS_Linux)
            #if (__GNUC__ > 4 && __GNUC_MINOR > 7)
                // gcc added this intrinsic by 4.7.1
                #define YieldProcessor __builtin_ia32_pause
            #endif // (__GNUC__ > 4 && __GNUC_MINOR > 7)
            #if defined (__GNUC__)
                // gcc has had this intrinsic since forever
                #define MemoryBarrier __builtin_ia32_mfence
            #endif // defined (__GNUC__)
        #elif defined (TOOLCHAIN_OS_OSX)
            #if __has_builtin (__builtin_ia32_pause)
                // clang added this intrinsic in 3.8
                #define YieldProcessor __builtin_ia32_pause
            #endif // __has_builtin (__builtin_ia32_pause)
            #if __has_builtin (__builtin_ia32_mfence)
                // clang has had this intrinsic since at least 3.0
                #define MemoryBarrier __builtin_ia32_mfence
            #endif // __has_builtin (__builtin_ia32_mfence)
        #endif // defined (TOOLCHAIN_OS_Linux)
        // If we don't have intrinsics, we can do some inline asm instead.
        #if !defined (YieldProcessor)
             #define YieldProcessor() asm volatile ("pause")
        #endif // !defined (YieldProcessor)
        #if !defined (MemoryBarrier)
             #define MemoryBarrier() asm volatile ("mfence")
        #endif // !defined (MemoryBarrier)
    #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        #if defined (TOOLCHAIN_ARCH_arm64)
             #define YieldProcessor() asm volatile ("yield")
             #define MemoryBarrier __sync_synchronize
        #endif // defined (TOOLCHAIN_ARCH_arm64)
        #if defined (TOOLCHAIN_ARCH_arm32)
             #define YieldProcessor()
             #define MemoryBarrier __sync_synchronize
        #endif // defined (TOOLCHAIN_ARCH_arm32)
    #endif // defined (TOOLCHAIN_COMPILER_cl)

        void CPU::Pause () {
            YieldProcessor ();
        }

        void CPU::Barrier () {
            MemoryBarrier ();
        }

        void CPU::Dump (std::ostream &stream) const {
        #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64) ||\
            defined (TOOLCHAIN_ARCH_ppc32) || defined (TOOLCHAIN_ARCH_ppc64)
            auto Supported = [&stream] (
                    const std::string &feature,
                    bool supported) {
                stream << feature << (supported ? " supported" : " not supported") << std::endl;
            };
        #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64) ||
               // defined (TOOLCHAIN_ARCH_ppc32) || defined (TOOLCHAIN_ARCH_ppc64)
        #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
            stream << Vendor () << std::endl;
            stream << Brand () << std::endl;
            Supported ("3DNOW", _3DNOW ());
            Supported ("3DNOWEXT", _3DNOWEXT ());
            Supported ("ABM", ABM ());
            Supported ("ADX", ADX ());
            Supported ("AES", AES ());
            Supported ("AVX", AVX ());
            Supported ("AVX2", AVX2 ());
            Supported ("AVX512CD", AVX512CD ());
            Supported ("AVX512ER", AVX512ER ());
            Supported ("AVX512F", AVX512F ());
            Supported ("AVX512PF", AVX512PF ());
            Supported ("BMI1", BMI1 ());
            Supported ("BMI2", BMI2 ());
            Supported ("CLFSH", CLFSH ());
            Supported ("CMPXCHG16B", CMPXCHG16B ());
            Supported ("CX8", CX8 ());
            Supported ("ERMS", ERMS ());
            Supported ("F16C", F16C ());
            Supported ("FMA", FMA ());
            Supported ("FSGSBASE", FSGSBASE ());
            Supported ("FXSR", FXSR ());
            Supported ("HLE", HLE ());
            Supported ("INVPCID", INVPCID ());
            Supported ("LAHF", LAHF ());
            Supported ("LZCNT", LZCNT ());
            Supported ("MMX", MMX ());
            Supported ("MMXEXT", MMXEXT ());
            Supported ("MONITOR", MONITOR ());
            Supported ("MOVBE", MOVBE ());
            Supported ("MSR", MSR ());
            Supported ("OSXSAVE", OSXSAVE ());
            Supported ("PCLMULQDQ", PCLMULQDQ ());
            Supported ("POPCNT", POPCNT ());
            Supported ("PREFETCHWT1", PREFETCHWT1 ());
            Supported ("RDRAND", RDRAND ());
            Supported ("RDSEED", RDSEED ());
            Supported ("RDTSCP", RDTSCP ());
            Supported ("RTM", RTM ());
            Supported ("SEP", SEP ());
            Supported ("SHA", SHA ());
            Supported ("SSE", SSE ());
            Supported ("SSE2", SSE2 ());
            Supported ("SSE3", SSE3 ());
            Supported ("SSE4.1", SSE41 ());
            Supported ("SSE4.2", SSE42 ());
            Supported ("SSE4a", SSE4a ());
            Supported ("SSSE3", SSSE3 ());
            Supported ("SYSCALL", SYSCALL ());
            Supported ("TBM", TBM ());
            Supported ("XOP", XOP ());
            Supported ("XSAVE", XSAVE ());
            stream << "L1 cache line size: " << L1CacheLineSize () << std::endl;
        #elif defined (TOOLCHAIN_ARCH_ppc32) || defined (TOOLCHAIN_ARCH_ppc64)
            Supported ("AltiVec", AltiVec ());
        #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        }

    } // namespace util
} // namespace thekogans
