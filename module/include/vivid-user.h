#ifndef _VIVID_USER_H_
#define _VIVID_USER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  void vividStart(void);
  void vividStop(void);
  void vividPreventSleep(void);
  void vividUpdateL2(uint8_t pressed, uint8_t value);
  void vividUpdateR2(uint8_t pressed, uint8_t value);
  void vividUpdateL3(uint8_t pressed);
  void vividUpdateR3(uint8_t pressed);
  void vividScreenOn(void);
  void vividScreenOff(void);

  int vividUsbAttached(void);
  uint8_t vividLedMask(void);

#ifdef __cplusplus
}
#endif

#endif // __VIVID_USER_H_
