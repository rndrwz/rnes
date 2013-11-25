#include "mmc.h"
#include <cassert>
#include <iostream>

//
// Shared MMC logic.
//

uint16_t Mmc::translateHorizMirror(uint16_t addr)
{
    if (addr >= nameTable1 && addr < (nameTable2)) {
        addr = (addr & (nameTableSize - 1)) + nameTable0;
    }
    else if (addr >= nameTable3 && addr < (nameTable3 + nameTableSize)) {
        addr = (addr & (nameTableSize - 1)) + nameTable2;
    }
    return addr;
}

uint16_t Mmc::translateVerticalMirror(uint16_t addr)
{
    if (addr >= nameTable3 && addr < (nameTable3 + nameTableSize)) {
        addr = (addr & (nameTableSize - 1)) + nameTable1;
    }
    else if (addr >= nameTable2 && addr < nameTable3) {
        addr = (addr & (nameTableSize - 1)) + nameTable0;
    }
    return addr;
}

uint16_t Mmc::translateSingleMirror(uint16_t addr)
{
    if (addr >= nameTable0 and addr < nameTable3 + nameTableSize) {
        addr = (addr & (nameTableSize - 1)) + nameTable0; 
    }
    return addr;
}

//
// No MMC logic.
//

MmcNone::MmcNone(const std::vector<uint8_t*> &prgRoms,
                 const std::vector<uint8_t*> &chrRoms,
                 uint32_t prgRam,
                 bool vertMirror) :
    progRoms{prgRoms},
    charRoms{chrRoms},
    numPrgRam{prgRam},
    verticalMirror{vertMirror}
{
    assert(numPrgRam == 0 || numPrgRam == 1);
    assert(prgRoms.size() <= 2);
    assert(chrRoms.size() <= 1);
}

MmcNone::~MmcNone()
{}

void MmcNone::cpuMemWrite(uint16_t addr, uint8_t val)
{
    assert(addr >= mmcCpuAddrBase);
    // sram region
    if ((numPrgRam == 1) and
        (addr >= prgSramBase) and
        (addr < prgSramBase + prgSramSize)) {
        cpuSram[addr - prgSramBase] = val;                
    }
}

uint8_t MmcNone::cpuMemRead(uint16_t addr)
{
    assert(addr >= mmcCpuAddrBase);
    // sram region
    if ((numPrgRam == 1) and
        (addr >= prgSramBase) and
        (addr < prgSramBase + prgSramSize)) {
        return cpuSram[addr - prgSramBase];
    }
    // prg rom 1
    if ((addr >= 0x8000) and
        (addr < 0x8000 + 0x4000) and
        (progRoms.size() == 2)) {
        return *(progRoms[0] + addr - 0x8000);
    }
    // prg rom 2
    if ((addr >= 0xc000) and
        (progRoms.size() >= 1)) {
        if (progRoms.size() == 1) {
            return *(progRoms[0] + addr - 0xc000);
        }
        else {
            return *(progRoms[1] + addr - 0xc000);
        }
    }
    return 0;
}

uint16_t MmcNone::vidAddrTranslate(uint16_t addr) 
{
    if (verticalMirror) {
        return translateVerticalMirror(addr);
    }
    else {
        return translateHorizMirror(addr);
    }
}

void MmcNone::vidMemWrite(uint16_t addr, uint8_t val)
{
    addr = vidAddrTranslate(addr);
    if (addr >= 0x2000 and addr < 0x2000 + 0x1000) {
        vidSram[addr] = val;    
    }
    else if (addr >= 0x3f00 and addr <= 0x3f1f) {
        vidSram[addr] = val;    
    }
}

uint8_t MmcNone::vidMemRead(uint16_t addr)
{
    addr = vidAddrTranslate(addr);
    if ((charRoms.size() == 1) and
        (addr < 0x2000)) {
        return *(charRoms[0] + addr);
    }
    if ((addr >= 0x2000) and (addr < 0x2000 + 0x1000)) {
        return vidSram[addr];
    }
    if (addr >= 0x3f00 and addr <= 0x3f1f) {
        return vidSram[addr];
    }
    return 0;
}

//
// MMC1 logic
//

Mmc1::Mmc1(const std::vector<uint8_t*> &prgRoms,
           const std::vector<uint8_t*> &chrRoms,
           uint32_t prgRam,
           bool vertMirror) :
    progRoms{prgRoms},
    charRoms{chrRoms},
    numPrgRam{prgRam}
{
    assert(numPrgRam == 0 || numPrgRam == 1);
}

Mmc1::~Mmc1() {}

void Mmc1::updateMmcRegister(uint16_t addr, uint8_t shiftRegister)
{
    switch ((addr >> 13) & 0x3) {
        case 0:
            controlReg = shiftRegister;
            if (debug) {
                std::cerr << "mmc1: control reg: " << std::hex << (int)controlReg << std::endl;
            }
            break;
        case 1:
            chr0Bank = shiftRegister;
            if (debug) {
                std::cerr << "mmc1: chr0Bank reg: " << std::hex << (int)chr0Bank << std::endl;
            }
            break;
        case 2:
            chr1Bank = shiftRegister;
            if (debug) {
                std::cerr << "mmc1: chr1Bank reg: " << std::hex << (int)chr1Bank << std::endl;
            }
            break;
        case 3:
            prgBank = shiftRegister;
            if (debug) {
                std::cerr << "mmc1: chr1Bank reg: " << std::hex << (int)prgBank << std::endl;
            }
            break;
        default:
            break;
    }
    
}

void Mmc1::cpuMemWrite(uint16_t addr, uint8_t val)
{
    assert(addr >= mmcCpuAddrBase);
    // sram region
    if ((isPrgSramEnabled()) and
        (addr >= prgSramBase) and
        (addr < prgSramBase + prgSramSize)) {
        cpuSram[addr - prgSramBase] = val;                
    }
    // shift register writes
    if ((addr >= shiftWriteAddr) and 
        (addr <= shiftWriteAddrLimit)) {
        // initialize shift register when 7th bit is set.
        if (val & (1 << 7)) {
            shiftRegister = shiftInit;
            controlReg |= 0x2 << 2;
            if (debug) {
                std::cerr << "mmc1: control reg reset: " << std::hex << (int)controlReg << std::endl;
            }
        }
        else {
            uint8_t oldShiftRegister = shiftRegister;
            shiftRegister >>= 1;    
            shiftRegister |= (val & 0x1) << 4;
            // lowest bit is set on last write.
            if (oldShiftRegister & 0x1) {
                updateMmcRegister(addr, shiftRegister);
                shiftRegister = shiftInit;
            }
        }
    }
}

uint8_t Mmc1::cpuMemRead(uint16_t addr)
{
    assert(addr >= mmcCpuAddrBase);
    // sram region
    if ((isPrgSramEnabled()) and
        (addr >= prgSramBase) and
        (addr < prgSramBase + prgSramSize)) {
        return cpuSram[addr - prgSramBase];
    }
    // prg rom @ 0x8000
    uint32_t prgRomMode = getPrgRomMode();
    uint8_t bank = prgBank & 0xf;
    if ((addr >= 0x8000) and (addr < 0x8000 + 0x4000)) {
        switch (prgRomMode) {
            case 0:
            case 1:
                return *(progRoms[bank & ~0x1] + addr - 0x8000);
                break;
            case 2:
                return *(progRoms[0] + addr - 0x8000);
                break;
            case 3:
                return *(progRoms[bank] + addr - 0x8000);
                break;
            default:
                assert(0);
                break;
        }
    }
    // prg rom @ 0xc000
    if (addr >= 0xc000) {
        switch (prgRomMode) {
            case 0:
            case 1:
                return *(progRoms[(bank & ~0x1) + 1] + addr - 0xc000);
                break;
            case 2:
                return *(progRoms[bank] + addr - 0xc000);
                break;
            case 3:
                return *(progRoms[progRoms.size()-1] + addr - 0xc000);
                break;
            default:
                assert(0);
                break;
        }
    }
    return 0;
}

void Mmc1::vidMemWrite(uint16_t addr, uint8_t val)
{
    addr = vidAddrTranslate(addr);
    bool chr8kMode = getChrRomMode() == 0;
    if ((charRoms.size() == 0) and (addr < 0x1000)) {
        if (chr8kMode) {
            vidSram[addr] = val;
        }
        else {
            vidSram[addr + ((chr0Bank & 0x1) ? 0x1000 : 0)] = val;
        }
    }
    else if ((charRoms.size() == 0) and (addr >= 0x1000) and (addr <= 0x1fff)) {
        if (chr8kMode) {
            vidSram[addr] = val;
        }
        else {
            vidSram[addr - 0x1000 + ((chr1Bank & 0x1) ? 0x1000 : 0)] = val;
        }

    }
    else if (addr >= 0x2000 and addr < 0x2000 + 0x1000) {
        vidSram[addr] = val;    
    }
    else if (addr >= 0x3f00 and addr <= 0x3f1f) {
        vidSram[addr] = val;    
    }
}

uint8_t Mmc1::vidMemRead(uint16_t addr)
{
    addr = vidAddrTranslate(addr);

    bool chr8kMode = getChrRomMode() == 0;
    if (charRoms.size() > 0) {
        // chr rom @ 0x0
        if (addr < 0x1000) {
            if (chr8kMode) {
                return *(charRoms[chr0Bank >> 1] + addr);
            }
            else {
                return *(charRoms[chr0Bank >> 1] + addr + ((chr0Bank & 0x1) ? 0x1000 : 0));
            }
        }
        // chr rom @ 0x1000
        if ((addr >= 0x1000) and (addr <= 0x1fff)) {
            if (chr8kMode) {
                return *(charRoms[chr0Bank >> 1] + addr);
            }
            else {
                return *(charRoms[chr1Bank >> 1] + addr + ((chr1Bank & 0x1) ? 0x1000 : 0) - 0x1000);
            }
        }
    } 
    else {
        if (chr8kMode) {
            return vidSram[addr];
        }
        else {
            if (addr < 0x1000) {
                return vidSram[addr + (chr0Bank & 0x1) ? 0x1000 : 0];
            }
            if ((addr >= 0x1000) and (addr <= 0x1fff)) {
                return vidSram[addr - 0x1000 + (chr1Bank & 0x1) ? 0x1000 : 0];
            }
        }
    }

    // name table sram
    if ((addr >= 0x2000) and (addr <= 0x2fff)) {
        return vidSram[addr];
    }
    if (addr >= 0x3f00 and addr <= 0x3f1f) {
        return vidSram[addr];
    }
    return 0;
}

uint16_t Mmc1::vidAddrTranslate(uint16_t addr) 
{
    uint32_t mirrorMode = getMirroringMode();
    switch (mirrorMode) {
        case 0: return translateSingleMirror(addr);
        case 1: return translateSingleMirror(addr) + nameTableSize;
        case 2: return translateVerticalMirror(addr);
        case 3: return translateHorizMirror(addr);
        default: assert(0); 
    }
    return 0; 
}

