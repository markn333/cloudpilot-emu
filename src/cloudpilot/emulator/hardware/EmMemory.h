/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
        Copyright (c) 1998-2001 Palm, Inc. or its subsidiaries.
        All rights reserved.

        This file is part of the Palm OS Emulator.

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.
\* ===================================================================== */

#ifndef EmMemory_h
#define EmMemory_h

// Normally, I'd assume that these includes were pulled in
// by EmCommon.h.  However, EmMemory.h gets included by UAE,
// which doesn't pull in EmCommon.h.  So I have to explicitly
// make sure they're included.

#include "DebuggerMemoryBinding.h"
#include "EmAssert.h"   // EmAssert
#include "EmTypes.h"    // uint32, etc.
#include "Switches.h"   // WORDSWAP_MEMORY, UNALIGNED_LONG_ACCESS
#include "sysconfig.h"  // STATIC_INLINE

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
//		� EmAddressBank
// ---------------------------------------------------------------------------

typedef uint32 (*EmMemGetFunc)(emuptr);
typedef void (*EmMemPutFunc)(emuptr, uint32);
typedef uint8* (*EmMemTranslateFunc)(emuptr);
typedef int (*EmMemCheckFunc)(emuptr, uint32);
typedef void (*EmMemCycleFunc)(void);
typedef uint8* (*EmMemTranslateMetaFunc)(emuptr);

typedef struct EmAddressBank {
    /* These ones should be self-explanatory... */
    EmMemGetFunc lget, wget, bget;
    EmMemPutFunc lput, wput, bput;

    /* Use xlateaddr to translate an Amiga address to a uae_u8 * that can
     * be used to address memory without calling the wget/wput functions.
     * This doesn't work for all memory banks, so this function may call
     * abort(). */
    EmMemTranslateFunc xlateaddr;

    /* To prevent calls to abort(), use check before calling xlateaddr.
     * It checks not only that the memory bank can do xlateaddr, but also
     * that the pointer points to an area of at least the specified size.
     * This is used for example to translate bitplane pointers in custom.c */
    EmMemCheckFunc checkaddr;

    EmMemTranslateMetaFunc xlatemetaaddr;
    EmMemCycleFunc EmMemAddOpcodeCycles;
} EmAddressBank;

#ifndef ECM_DYNAMIC_PATCH

#if defined(PALMCYD_TWO_LEVEL_BANKS)
// PalmCYD (classic ESP32, no PSRAM): the bank table in two levels. gEmMemBanksL1[addr >> 24]
// points to 256 banks for (addr >> 16) & 0xFF: a block that is one bank throughout shares a
// uniform array, a mixed block has its own. A few KB instead of 256KB.
extern EmAddressBank*** gEmMemBanksL1;  // 256 entries, 32-bit-only IRAM
void EmMemSetBankSlot(uint32 index, EmAddressBank* bank);
void* EmMemAllocate32(size_t size);  // 32-bit-only IRAM if available (all fields 32 bits wide)
#elif defined(ESP_PLATFORM)
// PalmCYD: 256KB の表は内部 RAM に入らないため、起動時に PSRAM から確保する
extern EmAddressBank** gEmMemBanks;
#else
extern EmAddressBank* gEmMemBanks[65536];
#endif

#else  // ECM_DYNAMIC_PATCH

extern EmAddressBank** gDynEmMemBanksP;

#endif  // ECM_DYNAMIC_PATCH

// ---------------------------------------------------------------------------
//		� Support macros
// ---------------------------------------------------------------------------

#ifndef ECM_DYNAMIC_PATCH

    #define EmMemBankIndex(addr) (((emuptr)(addr)) >> 16)

#if defined(PALMCYD_TWO_LEVEL_BANKS)
    #define EmMemGetBankPtr(addr) \
        (gEmMemBanksL1[((emuptr)(addr)) >> 24][(((emuptr)(addr)) >> 16) & 0xFF])
    #define EmMemPutBank(addr, b) EmMemSetBankSlot(EmMemBankIndex(addr), (b))
#else
    #define EmMemGetBankPtr(addr) (gEmMemBanks[EmMemBankIndex(addr)])
    #define EmMemPutBank(addr, b) (gEmMemBanks[EmMemBankIndex(addr)] = (b))
#endif
    #define EmMemGetBank(addr) (*EmMemGetBankPtr(addr))

#else  // ECM_DYNAMIC_PATCH

    #define EmMemBankIndex(addr) (((unsigned long)(addr)) >> 16)

    #define EmMemGetBank(addr) (*((gDynEmMemBanksP)[EmMemBankIndex(addr)]))
    #define EmMemPutBank(addr, b) ((gDynEmMemBanksP)[EmMemBankIndex(addr)] = (b))

#endif  // ECM_DYNAMIC_PATCH

#define EmMemCallGetFunc(func, addr) ((*EmMemGetBank(addr).func)(addr))
#define EmMemCallPutFunc(func, addr, v) ((*EmMemGetBank(addr).func)(addr, v))

// Data read cache: like the instruction fetch cache below (EmMemFetch16), but with 256
// direct-mapped entries (indexed by bits 16-23 of the address) so that ROM and RAM reads
// do not evict each other. Each entry holds the host address of a 64KB ROM / RAM bank.
// Only even-aligned words / longs that stay within the bank take the fast path; everything
// else goes through the bank handlers. Invalidated together with the fetch cache
// (EmMemInvalidateCaches).

typedef struct EmMemReadCacheEntry {
    emuptr base;  // bank base address (see EmMemInvalidateCaches for invalid entries)
    uint8* host;  // host address of base
} EmMemReadCacheEntry;

#if defined(PALMCYD_TWO_LEVEL_BANKS)
// PalmCYD (classic ESP32): allocated in the 32-bit-only IRAM heap (all fields are 32 bits wide),
// which keeps the main DRAM region free for Palm's memory.
extern EmMemReadCacheEntry* gEmMemReadCache;
#else
extern EmMemReadCacheEntry gEmMemReadCache[256];
#endif

#define EmMemReadCacheEntryFor(addr) (&gEmMemReadCache[((addr) >> 16) & 0xFF])

// Data write cache for the RAM banks (EmBankDRAM / EmBankSRAM) and framebuffers
// (EmRegs::GetCacheableHost), same layout as the read cache. A hit does exactly what the
// bank's Set functions do for an aligned write that is not write protected: store, mark the
// page dirty, and report writes to meta memory marked as screen buffer (EmMemScreenWritten;
// a framebuffer uses a meta page that is all screen buffer). Banks that go through the SRAM write
// protection check are only cached while it is off; change the protection with
// EmMemSetProtectSRAM(), which drops those entries when it turns on.

typedef struct EmMemWriteCacheEntry {
    emuptr base;   // bank base address (see EmMemInvalidateCaches for invalid entries)
    uint8* host;   // host address of base
    uint8* meta;   // meta memory address of base
    emuptr phy;    // offset of base in its memory region (for the dirty page bitmap)
    uint8* dirty;  // dirty page bitmap of that region
} EmMemWriteCacheEntry;

#if defined(PALMCYD_TWO_LEVEL_BANKS)
extern EmMemWriteCacheEntry* gEmMemWriteCache;
#else
extern EmMemWriteCacheEntry gEmMemWriteCache[256];
#endif

#define EmMemWriteCacheEntryFor(addr) (&gEmMemWriteCache[((addr) >> 16) & 0xFF])

void EmMemPut32Slow(emuptr addr, uint32 l);
void EmMemPut16Slow(emuptr addr, uint16 w);
void EmMemPut8Slow(emuptr addr, uint8 b);
void EmMemScreenWritten(emuptr addressLo, emuptr addressHi);

STATIC_INLINE void EmMemDoPut32(void* a, uint32 v);
STATIC_INLINE void EmMemDoPut16(void* a, uint16 v);
STATIC_INLINE void EmMemDoPut8(void* a, uint8 v);

// Same as markDirty() in EmBankDRAM.cpp / EmBankSRAM.cpp / EmRegsFrameBuffer.cpp.
#define EmMemMarkRamDirty(dirty, phy) ((dirty)[(phy) >> 13] |= (uint8)(1 << (((phy) >> 10) & 0x07)))

// MetaMemory::kScreenBuffer in every byte (see MetaMemory.h).
#define EmMemScreenBits8 0x20
#define EmMemScreenBits16 0x2020

void EmMemInvalidateCaches(void);

uint32 EmMemGet32Slow(emuptr addr);
uint16 EmMemGet16Slow(emuptr addr);
uint8 EmMemGet8Slow(emuptr addr);

STATIC_INLINE uint32 EmMemDoGet32(void* a);
STATIC_INLINE uint16 EmMemDoGet16(void* a);
STATIC_INLINE uint8 EmMemDoGet8(void* a);

// ---------------------------------------------------------------------------
//		� EmMemGet32
// ---------------------------------------------------------------------------

STATIC_INLINE uint32 EmMemGet32(emuptr addr) {
#ifdef ENABLE_DEBUGGER
    DbgNotifyRead32(addr);

    return EmMemCallGetFunc(lget, addr);
#else
    const EmMemReadCacheEntry* entry = EmMemReadCacheEntryFor(addr);
    if (((addr ^ entry->base) & 0xFFFF0001) == 0 && (addr & 0xFFFF) <= 0xFFFC)
        return EmMemDoGet32(entry->host + (addr & 0xFFFF));

    return EmMemGet32Slow(addr);
#endif
}

// ---------------------------------------------------------------------------
//		� EmMemGet16
// ---------------------------------------------------------------------------

STATIC_INLINE uint16 EmMemGet16(emuptr addr) {
#ifdef ENABLE_DEBUGGER
    DbgNotifyRead16(addr);

    return EmMemCallGetFunc(wget, addr);
#else
    const EmMemReadCacheEntry* entry = EmMemReadCacheEntryFor(addr);
    if (((addr ^ entry->base) & 0xFFFF0001) == 0)
        return EmMemDoGet16(entry->host + (addr & 0xFFFF));

    return EmMemGet16Slow(addr);
#endif
}

// ---------------------------------------------------------------------------
//		� EmMemGet8
// ---------------------------------------------------------------------------

STATIC_INLINE uint8 EmMemGet8(emuptr addr) {
#ifdef ENABLE_DEBUGGER
    DbgNotifyRead8(addr);

    return EmMemCallGetFunc(bget, addr);
#else
    const EmMemReadCacheEntry* entry = EmMemReadCacheEntryFor(addr);
    if (((addr ^ entry->base) & 0xFFFF0000) == 0)
        return EmMemDoGet8(entry->host + (addr & 0xFFFF));

    return EmMemGet8Slow(addr);
#endif
}

// ---------------------------------------------------------------------------
//		� EmMemPut32
// ---------------------------------------------------------------------------

STATIC_INLINE void EmMemPut32(emuptr addr, uint32 l) {
#ifdef ENABLE_DEBUGGER
    DbgNotifyWrite32(addr);

    EmMemCallPutFunc(lput, addr, l);
#else
    const EmMemWriteCacheEntry* entry = EmMemWriteCacheEntryFor(addr);
    if (((addr ^ entry->base) & 0xFFFF0001) == 0 && (addr & 0xFFFF) <= 0xFFFC) {
        const emuptr offset = addr & 0xFFFF;
        const emuptr phy = entry->phy + offset;
        const uint16* meta = (const uint16*)(entry->meta + offset);

        EmMemDoPut32(entry->host + offset, l);
        EmMemMarkRamDirty(entry->dirty, phy);
        EmMemMarkRamDirty(entry->dirty, phy + 2);
    #if !defined(PALMCYD_NO_META_MEMORY)
        if (((meta[0] | meta[1]) & EmMemScreenBits16) != 0) EmMemScreenWritten(addr, addr + 4);
    #else
        (void)meta;
    #endif

        return;
    }

    EmMemPut32Slow(addr, l);
#endif
}

// ---------------------------------------------------------------------------
//		� EmMemPut16
// ---------------------------------------------------------------------------

STATIC_INLINE void EmMemPut16(emuptr addr, uint16 w) {
#ifdef ENABLE_DEBUGGER
    DbgNotifyWrite16(addr);

    EmMemCallPutFunc(wput, addr, w);
#else
    const EmMemWriteCacheEntry* entry = EmMemWriteCacheEntryFor(addr);
    if (((addr ^ entry->base) & 0xFFFF0001) == 0) {
        const emuptr offset = addr & 0xFFFF;
        const emuptr phy = entry->phy + offset;

        EmMemDoPut16(entry->host + offset, w);
        EmMemMarkRamDirty(entry->dirty, phy);
    #if !defined(PALMCYD_NO_META_MEMORY)
        if ((*(const uint16*)(entry->meta + offset) & EmMemScreenBits16) != 0)
            EmMemScreenWritten(addr, addr + 2);
    #endif

        return;
    }

    EmMemPut16Slow(addr, w);
#endif
}

// ---------------------------------------------------------------------------
//		� EmMemPut8
// ---------------------------------------------------------------------------

STATIC_INLINE void EmMemPut8(emuptr addr, uint8 b) {
#ifdef ENABLE_DEBUGGER
    DbgNotifyWrite8(addr);

    EmMemCallPutFunc(bput, addr, b);
#else
    const EmMemWriteCacheEntry* entry = EmMemWriteCacheEntryFor(addr);
    if (((addr ^ entry->base) & 0xFFFF0000) == 0) {
        const emuptr offset = addr & 0xFFFF;
        const emuptr phy = entry->phy + offset;

        EmMemDoPut8(entry->host + offset, b);
        EmMemMarkRamDirty(entry->dirty, phy);
    #if !defined(PALMCYD_NO_META_MEMORY)
        if ((entry->meta[offset] & EmMemScreenBits8) != 0) EmMemScreenWritten(addr, addr);
    #endif

        return;
    }

    EmMemPut8Slow(addr, b);
#endif
}

// ---------------------------------------------------------------------------
//		� EmMemGetRealAddress
// ---------------------------------------------------------------------------

STATIC_INLINE uint8* EmMemGetRealAddress(emuptr addr) { return EmMemGetBank(addr).xlateaddr(addr); }

// ---------------------------------------------------------------------------
//		� EmMemCheckAddress
// ---------------------------------------------------------------------------

STATIC_INLINE int EmMemCheckAddress(emuptr addr, uint32 size) {
    return EmMemGetBank(addr).checkaddr(addr, size);
}

// ---------------------------------------------------------------------------
//		� EmMemAddOpcodeCycles
// ---------------------------------------------------------------------------

STATIC_INLINE void EmMemAddOpcodeCycles(emuptr addr) {
    EmAssert(EmMemGetBank(addr).EmMemAddOpcodeCycles);
    EmMemGetBank(addr).EmMemAddOpcodeCycles();
}

// ---------------------------------------------------------------------------
//		� EmMemGetMetaAddress
// ---------------------------------------------------------------------------

STATIC_INLINE uint8* EmMemGetMetaAddress(emuptr addr) {
    EmAssert(EmMemGetBank(addr).xlatemetaaddr);
    return EmMemGetBank(addr).xlatemetaaddr(addr);
}

// ---------------------------------------------------------------------------
//		� EmMemDoGet32
// ---------------------------------------------------------------------------

STATIC_INLINE uint32 EmMemDoGet32(void* a) {
#if WORDSWAP_MEMORY || !UNALIGNED_LONG_ACCESS
    return (((uint32) * (((uint16*)a) + 0)) << 16) | (((uint32) * (((uint16*)a) + 1)));
#else
    return *(uint32*)a;
#endif
}

// ---------------------------------------------------------------------------
//		� EmMemDoGet16
// ---------------------------------------------------------------------------

STATIC_INLINE uint16 EmMemDoGet16(void* a) { return *(uint16*)a; }

// ---------------------------------------------------------------------------
//		� EmMemDoGet8
// ---------------------------------------------------------------------------

STATIC_INLINE uint8 EmMemDoGet8(void* a) {
#if WORDSWAP_MEMORY
    return *(uint8*)((uintptr_t)a ^ 1);
#else
    return *(uint8*)a;
#endif
}

// ---------------------------------------------------------------------------
//		� EmMemDoPut32
// ---------------------------------------------------------------------------

STATIC_INLINE void EmMemDoPut32(void* a, uint32 v) {
#if WORDSWAP_MEMORY || !UNALIGNED_LONG_ACCESS
    *(((uint16*)a) + 0) = (uint16)(v >> 16);
    *(((uint16*)a) + 1) = (uint16)(v);
#else
    *(uint32*)a = v;
#endif
}

// ---------------------------------------------------------------------------
//		� EmMemDoPut16
// ---------------------------------------------------------------------------

STATIC_INLINE void EmMemDoPut16(void* a, uint16 v) { *(uint16*)a = v; }

// ---------------------------------------------------------------------------
//		� EmMemDoPut8
// ---------------------------------------------------------------------------

STATIC_INLINE void EmMemDoPut8(void* a, uint8 v) {
#if WORDSWAP_MEMORY
    *(uint8*)((uintptr_t)a ^ 1) = v;
#else
    *(uint8*)a = v;
#endif
}

// ---------------------------------------------------------------------------
//		EmMemFetch16 / EmMemFetch32
// ---------------------------------------------------------------------------
// Instruction fetch (opcode and extension words) with a one-entry cache of the
// host address of the current 64KB bank. Only plain ROM / RAM banks are cached
// (see EmMemFetch16Slow); everything else, odd addresses included, takes the
// normal bank dispatch. The cache is invalidated whenever the bank table
// changes (Memory::InitializeBanks).

extern emuptr gEmMemFetchBase;  // bank base address; 1 = invalid
extern uint8* gEmMemFetchHost;  // host address of gEmMemFetchBase

uint16 EmMemFetch16Slow(emuptr addr);

STATIC_INLINE uint16 EmMemFetch16(emuptr addr) {
#ifndef ENABLE_DEBUGGER
    if (((addr ^ gEmMemFetchBase) & 0xFFFF0001) == 0)
        return EmMemDoGet16(gEmMemFetchHost + (addr & 0xFFFF));
#endif
    return EmMemFetch16Slow(addr);
}

STATIC_INLINE uint32 EmMemFetch32(emuptr addr) {
    return ((uint32)EmMemFetch16(addr) << 16) | EmMemFetch16(addr + 2);
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

    #include "MemoryRegion.h"
    #include "savestate/ChunkType.h"

class EmStream;

// Types.

// This struct is used to control access to memory.  The first set of fields
// are booleans which, when set to true, turn on address validation for the
// various ranges of memory.  The second second set of fields are booleans
// which, when set to true, prevent access by user applications to the various
// ranges of memory.

struct MemAccessFlags {
    Bool fProtect_SRAMSet;
};

// Globals.

extern MemAccessFlags gMemAccessFlags;

// Set gMemAccessFlags.fProtect_SRAMSet (from the chip select registers). Drops RAM write
// cache entries that relied on the protection being off when it turns on.
void EmMemSetProtectSRAM(Bool protect);
extern Bool gPCInRAM;
extern Bool gPCInROM;

struct EmAddressBank;

template <typename ChunkType>
class SavestateLoader;

// Function prototypes.

class EmDevice;

class Memory {
   public:
    static bool Initialize(const uint8* romBuffer, size_t romSize, EmDevice& device);
    static void Reset(Bool hardwareReset);

    template <typename T>
    static void Save(T& savestate);
    static void Load(SavestateLoader<ChunkType>& loader);

    static void Dispose(void);

    static void InitializeBanks(EmAddressBank& iBankInitializer, int32 iStartingBankIndex,
                                int32 iNumberOfBanks);

    static void ResetBankHandlers(void);

    static void MapPhysicalMemory(const void*, uint32);
    static void UnmapPhysicalMemory(const void*);
    static void GetMappingInfo(emuptr, void**, uint32*);

    static void CheckNewPC(emuptr newPC);
    static int IsPCInRAM(void) { return gPCInRAM; }
    static int IsPCInROM(void) { return gPCInROM; }

    static uint8* GetDirtyPagesForRegion(MemoryRegion region);
    static uint8* GetForRegion(MemoryRegion region);
    static uint32 GetRegionSize(MemoryRegion region);

    static uint32 GetTotalMemorySize();
    static uint8* GetTotalMemory();
    static uint8* GetTotalDirtyPages();

    static bool LoadMemoryV1(void* ram, size_t size);
    static bool LoadMemoryV2(void* memory, size_t size);
    static bool LoadMemoryV4(void* memory, size_t size);
};

typedef Memory EmMemory;

// There are places within the emulator where we'd like to access low-memory
// and/or Dragonball registers.  If the PC happens to be in RAM, then
// the checks implied by the above booleans and switches will flag our
// access as an error.  Before making such accesses, create an instance
// of CEnableFullAccess to suspend and restore the checks.
//
// Since such accesses are typically "meta" accesses where the emulator is
// accessing memory outside the normal execution of an opcode, we also
// turn off the profiling variable that controls whether or not cycles
// spent accessing memory are counted.

class CEnableFullAccess {
   public:
    CEnableFullAccess(void);
    ~CEnableFullAccess(void);

    static Bool AccessOK(void);

   private:
    MemAccessFlags fOldMemAccessFlags;

    static long fgAccessCount;
};

// Std C Library-ish routines for manipulating data
// in emulated memory.

emuptr EmMem_memset(emuptr dst, int val, size_t len);

emuptr EmMem_memchr(emuptr src, int val, size_t len);

template <class T1, class T2>
int EmMem_memcmp(T1 src1, T2 src2, size_t len);

template <class T1, class T2>
T1 EmMem_memcpy(T1 dst, T2 src, size_t len);

template <class T1, class T2>
T1 EmMem_memmove(T1 dst, T2 src, size_t len);

size_t EmMem_strlen(emuptr str);

template <class T1, class T2>
T1 EmMem_strcpy(T1 dst, T2 src);

template <class T1, class T2>
T1 EmMem_strncpy(T1 dst, T2 src, size_t len);

template <class T1, class T2>
T1 EmMem_strcat(T1 dst, T2 src);

template <class T1, class T2>
T1 EmMem_strncat(T1 dst, T2 src, size_t len);

template <class T1, class T2>
int EmMem_strcmp(T1 dst, T2 src);

template <class T1, class T2>
int EmMem_strncmp(T1 dst, T2 src, size_t len);

#endif  // __cplusplus

#endif /* EmMemory_h */
