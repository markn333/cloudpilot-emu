
#ifndef _EM_REGS_VZ_PEG_N700C_
#define _EM_REGS_VZ_PEG_N700C_

#include "EmRegsSonyDSP.h"
#include "EmRegsVZ.h"

class EmRegsVzPegN700C : public EmRegsVZNoScreen {
   public:
    EmRegsVzPegN700C(EmRegsSonyDSP& dsp);
    virtual ~EmRegsVzPegN700C();

   public:
    // PalmCYD: backlight brightness (digital potentiometer on port C, see PortDataChanged)
    static constexpr int kBacklightLevels = 32;
    // The level the potentiometer starts at (it is not part of the savestate; the host keeps the
    // last level and sets it before the session is created).
    static void SetInitialBacklightLevel(int level);

    virtual Bool GetLCDScreenOn(void);
    virtual Bool GetLCDBacklightOn(void);
    virtual uint16 GetLEDState(void);

    virtual Bool GetSerialPortOn(int uartNum);
    virtual Bool GetVibrateOn(void);

    virtual uint8 GetPortInputValue(int);
    virtual uint8 GetPortInternalValue(int);
    virtual void GetKeyInfo(int* numRows, int* numCols, uint16* keyMap, Bool* rows);

   protected:
    virtual EmSPISlave* GetSPI2Slave(void);
    virtual void portDIntReqEnWrite(emuptr address, int size, uint32 value);
    virtual void PortDataChanged(int port, uint8 oldValue, uint8 newValue);


   private:
    static int sInitialBacklightLevel;
    int fBacklightLevel{sInitialBacklightLevel};

    EmSPISlave* fSPISlaveADC;
    EmRegsSonyDSP& dsp;
};

#endif  // _EM_REGS_VZ_PEG_N700C_
