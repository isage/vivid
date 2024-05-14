#ifndef _VIVID_USER_H_
#define _VIVID_USER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  #define VIVID_MODULE_API 2

  void vividStart(void);
  void vividStop(void);

  void vividUpdateL2(uint8_t pressed, uint8_t value);
  void vividUpdateR2(uint8_t pressed, uint8_t value);
  void vividUpdateL3(uint8_t pressed);
  void vividUpdateR3(uint8_t pressed);
  void vividUpdateAcc(uint16_t x, uint16_t y, uint16_t z);
  void vividUpdateGyro(uint16_t z);

  void vividScreenOn(void);
  void vividScreenOff(void);
  void vividPreventSleep(void);

  int vividUsbAttached(void);
  uint8_t vividLedMask(void);
  uint8_t vividVersion(void);

#ifdef __cplusplus
}
#endif

#endif // __VIVID_USER_H_
