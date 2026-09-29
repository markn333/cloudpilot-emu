/* -*- mode: C++; tab-width: 4 -*- */
/* ===================================================================== *\
        Copyright (c) 2000-2001 Palm, Inc. or its subsidiaries.
        All rights reserved.

        This file is part of the Palm OS Emulator.

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.
\* ===================================================================== */

#ifndef EmRegs_h
#define EmRegs_h

#include <vector>

#include "EmCommon.h"
#include "savestate/ChunkType.h"

template <typename ChunkType>
class Savestate;

template <typename ChunkType>
class SavestateProbe;

template <typename ChunkType>
class SavestateLoader;

struct EmAddressBank;

class EmRegs {
   public:
    EmRegs(void);
    virtual ~EmRegs(void);

    virtual void Initialize(void);
    virtual void Reset(Bool hardwareReset);
    virtual void Save(Savestate<ChunkType>&);
    virtual void Save(SavestateProbe<ChunkType>&);
    virtual void Load(SavestateLoader<ChunkType>&);
    virtual void Dispose(void);

    void SetBankHandlers(EmAddressBank&);
    virtual void SetSubBankHandlers(void) = 0;

    virtual uint32 GetLong(emuptr address);
    virtual uint32 GetWord(emuptr address);
    virtual uint32 GetByte(emuptr address);
    virtual void SetLong(emuptr address, uint32 value);
    virtual void SetWord(emuptr address, uint32 value);
    virtual void SetByte(emuptr address, uint32 value);
    virtual int ValidAddress(emuptr address, uint32 size);
    virtual uint8* GetRealAddress(emuptr address) = 0;
    virtual emuptr GetAddressStart(void) = 0;
    virtual uint32 GetAddressRange(void) = 0;

    virtual bool AllowUnalignedAccess(emuptr address, int size);

    // PalmCYD: if the 64KB bank at base is plain memory whose writes only need the dirty page
    // bitmap and the screen dirty marking (a framebuffer), return its host address and set
    // *dirtyPages / *phy (offset for the bitmap) so the memory caches can access it directly.
    virtual uint8* GetCacheableHost(emuptr base, uint8** dirtyPages, emuptr* phy) { return nullptr; }

   protected:
    typedef uint32 (EmRegs::*ReadFunction)(emuptr address, int size);
    typedef void (EmRegs::*WriteFunction)(emuptr address, int size, uint32 value);

    void SetHandler(ReadFunction read, WriteFunction write, uint32 start, int count);

    uint32 UnsupportedRead(emuptr address, int size);
    uint32 StdRead(emuptr address, int size);
    uint32 StdReadBE(emuptr address, int size);
    uint32 ZeroRead(emuptr address, int size);

    void UnsupportedWrite(emuptr address, int size, uint32 value);
    void StdWrite(emuptr address, int size, uint32 value);
    void StdWriteBE(emuptr address, int size, uint32 value);
    void NullWrite(emuptr address, int size, uint32 value);

   private:
    typedef vector<ReadFunction> ReadFunctionList;
    typedef vector<WriteFunction> WriteFunctionList;

    // PalmCYD: the handlers are kept as a list of distinct (read, write) pairs plus one byte
    // per address that selects the pair. A pair of member function pointers per address
    // (16 bytes) took several MB for the large register ranges of the CLIE devices
    // (MediaQ framebuffer 256KB, Sony DSP 64KB).
    ReadFunctionList fReadFunctions;    // distinct pairs: read handler
    WriteFunctionList fWriteFunctions;  // distinct pairs: write handler
    vector<uint8> fHandlerIndex;        // per address: index into the pair lists

    ReadFunction ReadHandlerAt(unsigned long offset) const {
        return fReadFunctions[fHandlerIndex[offset]];
    }
    WriteFunction WriteHandlerAt(unsigned long offset) const {
        return fWriteFunctions[fHandlerIndex[offset]];
    }
};

using EmRegsList = vector<EmRegs*>;

#endif /* EmRegs_h */
