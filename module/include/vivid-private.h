#ifndef _VIVID_PRIVATE_H_
#define _VIVID_PRIVATE_H_

#define DRIVER_ID "VIVID"
#define USB_PID 0x0268 // ds3

typedef struct
{
  uint8_t report_id; // always 1 // 00
  uint8_t unk_01;    // always zero // 01

  uint8_t buttons1; // 02
  uint8_t buttons2; // 03
  uint8_t buttons3; // 04
  uint8_t unk05;    // 05

  int8_t left_x;  // 6
  int8_t left_y;  // 7
  int8_t right_x; // 8
  int8_t right_y; // 9

  uint8_t unk10[4]; // 10 - 13, always zeroes

  uint8_t dpad_up_a;    // 14
  uint8_t dpad_right_a; // 15
  uint8_t dpad_down_a;  // 16
  uint8_t dpad_left_a;  // 17

  uint8_t l2a; // 18
  uint8_t r2a; // 19

  uint8_t l1a; // 20
  uint8_t r1a; // 21

  uint8_t triangle_a; // 22
  uint8_t circle_a;   // 23
  uint8_t cross_a;    // 24
  uint8_t square_a;   // 25

  // always zero
  uint8_t unk26; // 26
  uint8_t unk27; // 27
  uint8_t unk28; // 28

  uint8_t unk29;   // 29 // status

  uint8_t battery; // 30 // power_rating

  uint8_t unk31[10]; // 31-40

  uint16_t acc_x; // 41-42
  uint16_t acc_y; // 43-44
  uint16_t acc_z; // 45-46

  uint16_t rot; // 47-48

} __attribute__((packed)) gamepad_report_t; // size = 49

#define EVF_CONNECTED (1 << 0)
#define EVF_DISCONNECTED (1 << 1)
#define EVF_EXIT (1 << 2)
#define EVF_INT_REQ_COMPLETED (1 << 3)
#define EVF_START (1 << 4)
#define EVF_ALL_MASK (EVF_INT_REQ_COMPLETED | (EVF_START - 1))

#endif //_VIVID_PRIVATE_H_