#include "descriptors.h"
#include "vivid-private.h"
#include "vivid-user.h"

#include <psp2kern/ctrl.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/udcd.h>
//#include <psp2kern/bt.h>
#include <stdio.h>
#include <string.h>
#include <taihen.h>
int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t *addr);

// missing in vitasdk
int ksceOledDisplayOn(void);
int ksceOledDisplayOff(void);
int ksceOledGetBrightness(void);
int ksceOledSetBrightness(int brightness);

int ksceLcdDisplayOn(void);
int ksceLcdDisplayOff(void);
int ksceLcdGetBrightness(void);
int ksceLcdSetBrightness(int brightness);

static SceUID g_usb_thread_id;
static SceUID g_event_flag_id;
static uint8_t g_usb_attached = 2;

static uint8_t g_exit_thread = 0;
static uint8_t g_is_oled     = 0;
static uint8_t g_is_lcd      = 0;

static uint8_t g_l2_pressed;
static uint8_t g_r2_pressed;
static uint8_t g_l2_value;
static uint8_t g_r2_value;
static uint8_t g_l3_pressed;
static uint8_t g_r3_pressed;
static uint8_t g_led_mask    = 0;
static uint8_t g_my_mac[6]   = {0};
static uint8_t g_host_mac[6] = {0};

static uint16_t g_acc_x = 0;
static uint16_t g_acc_y = 0;
static uint16_t g_acc_z = 0;
static uint16_t g_gyro_z = 0;

static uint8_t g_high_res_gyro = 0;

static int g_prev_brightness;

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

int (*ksceCtrlPeekBufferPositive2)(int port, SceCtrlData *pad_data, int count);

static void fillGamepadReport(const SceCtrlData *pad, gamepad_report_t *gamepad)
{
  memset(gamepad, 0, sizeof(gamepad_report_t));
  gamepad->report_id = 1;

  gamepad->buttons1  = 0;
  gamepad->buttons2  = 0;
  gamepad->buttons3  = 0;
  gamepad->battery   = 0xee;

  if (pad->buttons & SCE_CTRL_SELECT)
    gamepad->buttons1 |= 1 << 0;

  if (g_l3_pressed || pad->buttons & SCE_CTRL_L3)
    gamepad->buttons1 |= 1 << 1;

  if (g_r3_pressed || pad->buttons & SCE_CTRL_R3)
    gamepad->buttons1 |= 1 << 2;

  if (pad->buttons & SCE_CTRL_START)
    gamepad->buttons1 |= 1 << 3;

  if (pad->buttons & SCE_CTRL_UP)
  {
    gamepad->buttons1 |= 1 << 4;
    gamepad->dpad_up_a = 0xFF;
  }

  if (pad->buttons & SCE_CTRL_RIGHT)
  {
    gamepad->buttons1 |= 1 << 5;
    gamepad->dpad_right_a = 0xFF;
  }

  if (pad->buttons & SCE_CTRL_DOWN)
  {
    gamepad->buttons1 |= 1 << 6;
    gamepad->dpad_down_a = 0xFF;
  }

  if (pad->buttons & SCE_CTRL_LEFT)
  {
    gamepad->buttons1 |= 1 << 7;
    gamepad->dpad_left_a = 0xFF;
  }

  if (g_l2_pressed == 1 || pad->buttons & SCE_CTRL_L2)
  {
    gamepad->buttons2 |= 1 << 0;
    gamepad->l2a = MAX(g_l2_value, pad->lt);
  }

  if (g_r2_pressed == 1 || pad->buttons & SCE_CTRL_R2)
  {
    gamepad->buttons2 |= 1 << 1;
    gamepad->r2a = MAX(g_r2_value, pad->rt);
  }

  if (pad->buttons & SCE_CTRL_L1)
  {
    gamepad->buttons2 |= 1 << 2;
    gamepad->l1a = 0xFF;
  }
  if (pad->buttons & SCE_CTRL_R1)
  {
    gamepad->buttons2 |= 1 << 3;
    gamepad->r1a = 0xFF;
  }

  if (pad->buttons & SCE_CTRL_TRIANGLE)
  {
    gamepad->buttons2 |= 1 << 4;
    gamepad->triangle_a = 0xFF;
  }
  if (pad->buttons & SCE_CTRL_CIRCLE)
  {
    gamepad->buttons2 |= 1 << 5;
    gamepad->circle_a = 0xFF;
  }
  if (pad->buttons & SCE_CTRL_CROSS)
  {
    gamepad->buttons2 |= 1 << 6;
    gamepad->cross_a = 0xFF;
  }
  if (pad->buttons & SCE_CTRL_SQUARE)
  {
    gamepad->buttons2 |= 1 << 7;
    gamepad->square_a = 0xFF;
  }

  if (pad->buttons & SCE_CTRL_PSBUTTON)
    gamepad->buttons3 |= 1 << 0;

  gamepad->left_x  = pad->lx;
  gamepad->left_y  = pad->ly;
  gamepad->right_x = pad->rx;
  gamepad->right_y = pad->ry;

  // gyro/acc

  gamepad->unk29 = 0x03;

  gamepad->unk31[0] = 0x16;
  gamepad->unk31[1] = 0x0;
  gamepad->unk31[2] = 0x0;
  gamepad->unk31[3] = 0x0;
  gamepad->unk31[4] = 0x0;
  gamepad->unk31[5] = 0x33;
  gamepad->unk31[6] = 0x0;
  gamepad->unk31[7] = 0x77;

  if (g_high_res_gyro)
  {
    gamepad->unk31[8] = 0x00;
    gamepad->unk31[9] = 0x1c;
  }
  else
  {
    gamepad->unk31[8] = 0x01;
    gamepad->unk31[9] = 0x9c;
  }

  gamepad->acc_x = __builtin_bswap16(g_acc_x);
  gamepad->acc_y = __builtin_bswap16(g_acc_y);
  gamepad->acc_z = __builtin_bswap16(g_acc_z);

  // low-res = gyro / 128
  uint16_t gyro = g_gyro_z;
  if (!g_high_res_gyro) gyro = gyro / 128;
  gamepad->rot = __builtin_bswap16(gyro);

  // todo: battery?
}



static int sendDescHidReport(void)
{
  static SceUdcdDeviceRequest req = {.endpoint         = &endpoints[0],
                                     .data             = hid_report_descriptor,
                                     .size             = sizeof(hid_report_descriptor),
                                     .isControlRequest = 0,
                                     .onComplete       = NULL,
                                     .transmitted      = 0,
                                     .returnCode       = 0,
                                     .next             = NULL,
                                     .unused           = NULL,
                                     .physicalAddress  = NULL};

  return ksceUdcdReqSend(&req);
}

static int sendStringDescriptor(int index)
{
  static SceUdcdDeviceRequest req = {.endpoint         = &endpoints[0],
                                     .data             = &string_descriptors[0],
                                     .size             = sizeof(string_descriptors[0]),
                                     .isControlRequest = 0,
                                     .onComplete       = NULL,
                                     .transmitted      = 0,
                                     .returnCode       = 0,
                                     .next             = NULL,
                                     .unused           = NULL,
                                     .physicalAddress  = NULL};

  return ksceUdcdReqSend(&req);
}

static int sendInitialHidReport(uint8_t report_id)
{
  static gamepad_report_t gamepad_report __attribute__((aligned(64)))
  = {
    .report_id = 1,
    .buttons1 = 0,
    .buttons2 = 0,
    .buttons3 = 0,
    .battery = 0xee,
    .acc_x = 0xFF01,
    .acc_y = 0xFF01,
    .acc_z = 0xFF01,
    .rot = 0x0400
  };

  gamepad_report.unk31[0] = 0x12;
  gamepad_report.unk31[1] = 0x0;
  gamepad_report.unk31[2] = 0x0;
  gamepad_report.unk31[3] = 0x0;
  gamepad_report.unk31[4] = 0x0;
  gamepad_report.unk31[5] = 0x12;
  gamepad_report.unk31[6] = 0x0;
  gamepad_report.unk31[7] = 0x77;
  gamepad_report.unk31[8] = 0x01;
  gamepad_report.unk31[9] = 0x9C;

  SceCtrlData pad;
  SceCtrlData pad1;

  ksceCtrlPeekBufferPositive2(0, &pad, 1);
  if (ksceCtrlPeekBufferPositive2(1, &pad1, 1) > 0)
  {
    pad.lt = pad1.lt;
    pad.rt = pad1.rt;
  }

  fillGamepadReport(&pad, &gamepad_report);
  ksceKernelDcacheCleanRange(&gamepad_report, sizeof(gamepad_report));


  static SceUdcdDeviceRequest req = {.endpoint         = &endpoints[0],
                                     .data             = &gamepad_report,
                                     .size             = sizeof(gamepad_report),
                                     .isControlRequest = 0,
                                     .onComplete       = NULL,
                                     .transmitted      = 0,
                                     .returnCode       = 0,
                                     .next             = NULL,
                                     .unused           = NULL,
                                     .physicalAddress  = NULL};

  return ksceUdcdReqSend(&req);
}

static int sendHid01Report(uint8_t report_id)
{

  static uint8_t r01_reply[0x32] __attribute__((aligned(64)))
  = {
     0x00, 0x01, 0x04, 0x00, 0x0b, 0x0c, 0x01, 0x02, 0x18, 0x18, 0x18, 0x18, 0x09, 0x0a, 0x10, 0x11,
     0x12, 0x13, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x04,
     0x04, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x02, 0x07, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00
  };

  static SceUdcdDeviceRequest req = {.endpoint         = &endpoints[0],
                                     .data             = r01_reply,
                                     .size             = 0x32,
                                     .isControlRequest = 0,
                                     .onComplete       = NULL,
                                     .transmitted      = 0,
                                     .returnCode       = 0,
                                     .next             = NULL,
                                     .unused           = NULL,
                                     .physicalAddress  = NULL};

  return ksceUdcdReqSend(&req);
}

static int sendHidF2Report(uint8_t report_id, uint16_t length) // set operational
{
  static uint8_t f2_reply[18] __attribute__((aligned(64)))
  = {0xf2,                                                 // report num
     0xff, 0xff,
     0x00,
     0x00, 0x06, 0xf5, 0xd7, 0x8b, 0x5d, // mac
     0x00,
     0x03, 0x50, 0x81, 0xd8, 0x01, 0x8b, 0x00}; // model? host mac?

  f2_reply[4] = g_my_mac[0];
  f2_reply[5] = g_my_mac[1];
  f2_reply[6] = g_my_mac[2];
  f2_reply[7] = g_my_mac[3];
  f2_reply[8] = g_my_mac[4];
  f2_reply[9] = g_my_mac[5];

  static SceUdcdDeviceRequest req = {.endpoint         = &endpoints[0],
                                     .data             = f2_reply,
                                     .size             = 18,
                                     .isControlRequest = 0,
                                     .onComplete       = NULL,
                                     .transmitted      = 0,
                                     .returnCode       = 0,
                                     .next             = NULL,
                                     .unused           = NULL,
                                     .physicalAddress  = NULL};

  req.size = length;

  return ksceUdcdReqSend(&req);
}

static int sendHidF5Report(uint8_t report_id)
{
  // size = 8
  static uint8_t f5_reply[8] __attribute__((aligned(64))) = {
      0x01,                              // report num
      0x00,
      0x2c, 0x81, 0x58, 0x15, 0x45, 0x14 // host mac
  };

  f5_reply[2] = g_host_mac[0];
  f5_reply[3] = g_host_mac[1];
  f5_reply[4] = g_host_mac[2];
  f5_reply[5] = g_host_mac[3];
  f5_reply[6] = g_host_mac[4];
  f5_reply[7] = g_host_mac[5];

  static SceUdcdDeviceRequest req = {.endpoint         = &endpoints[0],
                                     .data             = f5_reply,
                                     .size             = 8,
                                     .isControlRequest = 0,
                                     .onComplete       = NULL,
                                     .transmitted      = 0,
                                     .returnCode       = 0,
                                     .next             = NULL,
                                     .unused           = NULL,
                                     .physicalAddress  = NULL};

  return ksceUdcdReqSend(&req);
}

static int sendHidF7Report(uint8_t report_id)
{
  // maybe battery level + status?
  static uint8_t f7_reply[12] __attribute__((aligned(64)))
  = {
    0x01, 0x00, 0xFF, 0x02, 0xA6, 0x01, 0xEE, 0xFF, 0x00, 0x02, 0x00, 0x00
  };

  static SceUdcdDeviceRequest req = {.endpoint         = &endpoints[0],
                                     .data             = f7_reply,
                                     .size             = 12,
                                     .isControlRequest = 0,
                                     .onComplete       = NULL,
                                     .transmitted      = 0,
                                     .returnCode       = 0,
                                     .next             = NULL,
                                     .unused           = NULL,
                                     .physicalAddress  = NULL};

  return ksceUdcdReqSend(&req);
}

static int sendHidF8Report(uint8_t report_id)
{
  // size = 8
  static uint8_t f8_reply[4] __attribute__((aligned(64))) = {0x00,
                                                             0x02, // 01 = unpaired, 02 = paired
                                                             0x00, 0x00};

  static SceUdcdDeviceRequest req = {.endpoint         = &endpoints[0],
                                     .data             = f8_reply,
                                     .size             = 4,
                                     .isControlRequest = 0,
                                     .onComplete       = NULL,
                                     .transmitted      = 0,
                                     .returnCode       = 0,
                                     .next             = NULL,
                                     .unused           = NULL,
                                     .physicalAddress  = NULL};

  return ksceUdcdReqSend(&req);
}

static uint8_t ef_request = 0xa0;

static int sendHidEFReport(uint8_t report_id)
{
  // replies seem to contain acc / gyro calibration data
  static uint8_t ef_reply_a0[48] __attribute__((aligned(64)))
  = {
     0x00, 0xEF, 0x04, 0x00, 0x0B, 0x03, 0x01, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x01, 0xFA, 0x01, 0x8C, 0x01, 0xF6, 0x01, 0x7E, 0x01, 0xFB, 0x01, 0x8C, 0x01, 0xFA, 0x00,
     0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  static uint8_t ef_reply_b0[48] __attribute__((aligned(64)))
  = {
     0x00, 0xEF, 0x04, 0x00, 0x0B, 0x03, 0x01, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x02, 0x6E, 0x02, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

  };

  static SceUdcdDeviceRequest req_a0 = {.endpoint         = &endpoints[0],
                                        .data             = ef_reply_a0,
                                        .size             = 48,
                                        .isControlRequest = 0,
                                        .onComplete       = NULL,
                                        .transmitted      = 0,
                                        .returnCode       = 0,
                                        .next             = NULL,
                                        .unused           = NULL,
                                        .physicalAddress  = NULL};

  static SceUdcdDeviceRequest req_b0 = {.endpoint         = &endpoints[0],
                                        .data             = ef_reply_b0,
                                        .size             = 48,
                                        .isControlRequest = 0,
                                        .onComplete       = NULL,
                                        .transmitted      = 0,
                                        .returnCode       = 0,
                                        .next             = NULL,
                                        .unused           = NULL,
                                        .physicalAddress  = NULL};
  if (ef_request == 0xA0)
    return ksceUdcdReqSend(&req_a0);
  else
    return ksceUdcdReqSend(&req_b0);
}

static void handleHidReportComplete(SceUdcdDeviceRequest *req)
{
  ksceKernelSetEventFlag(g_event_flag_id, EVF_INT_REQ_COMPLETED);
}


static int sendHidReport()
{
  static gamepad_report_t gamepad __attribute__((aligned(64))) = {0};
  SceCtrlData pad;
  SceCtrlData pad1;

  ksceCtrlPeekBufferPositive2(0, &pad, 1);
  if (ksceCtrlPeekBufferPositive2(1, &pad1, 1) > 0)
  {
    pad.lt = pad1.lt;
    pad.rt = pad1.rt;
  }

  fillGamepadReport(&pad, &gamepad);
  ksceKernelDcacheCleanRange(&gamepad, sizeof(gamepad));

  static SceUdcdDeviceRequest req = {.endpoint         = &endpoints[1],
                                     .data             = &gamepad,
                                     .size             = sizeof(gamepad),
                                     .isControlRequest = 0,
                                     .onComplete       = handleHidReportComplete,
                                     .transmitted      = 0,
                                     .returnCode       = 0,
                                     .next             = NULL,
                                     .unused           = NULL,
                                     .physicalAddress  = NULL};

  return ksceUdcdReqSend(&req);
}

void usb_ep0_req_recv_on_complete(SceUdcdDeviceRequest *req)
{
  uint8_t *data = (uint8_t *)req->data;

  uint16_t wValue     = (uint16_t)(*(uint16_t *)req->unused);
  uint8_t report_type = (wValue >> 8) & 0xFF;
  uint8_t report_id   = wValue & 0xFF;
  /*
    ksceKernelPrintf("recv size 0x%08x, wValue = 0x%04x\n", req->size, wValue);
    for(int i = 0; i < req->size; i++)
        ksceKernelPrintf("0x%02X ", data[i]);
    ksceKernelPrintf("\n");
  */

  switch (wValue)
  {
    case 0x03F4: // set operating mode
      // TODO: 0xB turns gamepad off, but pstv for some reason sends it on init
      break;
    case 0x03F5: // set host mac. e.g. 0x01 0x00 0xF8 0x2F 0xA8 0x68 0x9D 0xCB
      g_host_mac[0] = data[2];
      g_host_mac[1] = data[3];
      g_host_mac[2] = data[4];
      g_host_mac[3] = data[5];
      g_host_mac[4] = data[6];
      g_host_mac[5] = data[7];
      // todo: save to config
      break;
    case 0x03EF: // some pairing shit
      ef_request = data[6];
      break;
    case 0x0201: // set rumble/leds
      // data[9] = led mask
      g_led_mask = data[9];
      // getting 0xFF in data[5] should switch gyro to hi-res mode
      g_high_res_gyro = (data[5] == 0xFF);
      break;
    default:
      break;
  }
}

static int usb_ep0_enqueue_recv_for_req(const SceUdcdEP0DeviceRequest *ep0_req, uint16_t wValue)
{
  //  ksceKernelPrintf("recv enqueue 0x%08x\n", ep0_req->wLength);
  static uint16_t last_wValue = 0;
  last_wValue                 = wValue;

  static SceUdcdDeviceRequest req;
  static uint8_t set_report_data[64] __attribute__((aligned(64))) = {0};

  req = (SceUdcdDeviceRequest) {.endpoint         = &endpoints[0],
                                .data             = (void *)set_report_data,
                                .attributes       = 0,
                                .size             = ep0_req->wLength,
                                .isControlRequest = 0,
                                .onComplete       = &usb_ep0_req_recv_on_complete,
                                .transmitted      = 0,
                                .returnCode       = 0,
                                .next             = NULL,
                                .unused           = &last_wValue,
                                .physicalAddress  = NULL};

  return ksceUdcdReqRecv(&req);
}

static int processUdcdRequest(int recipient, int arg, SceUdcdEP0DeviceRequest *req, void *user_data)
{

  if (arg < 0)
    return -1;

  uint8_t req_dir       = req->bmRequestType & USB_CTRLTYPE_DIR_MASK;
  uint8_t req_type      = req->bmRequestType & USB_CTRLTYPE_TYPE_MASK;
  uint8_t req_recipient = req->bmRequestType & USB_CTRLTYPE_REC_MASK;

//  ksceKernelPrintf("got request: dir: 0x%02x, type: 0x%02x, recp: 0x%02x, wValue: 0x%04x, wLength: 0x%04x\n", req_dir, req_type, req_recipient, req->wValue, req->wLength);

  // TODO: untangle this shit
  if (req_dir == USB_CTRLTYPE_DIR_DEVICE2HOST)
  {
    switch (req_type)
    {
      case USB_CTRLTYPE_TYPE_STANDARD:
        switch (req_recipient)
        {
          case USB_CTRLTYPE_REC_DEVICE:
            switch (req->bRequest)
            {
              case USB_REQ_GET_DESCRIPTOR:
              {
                uint8_t descriptor_type = (req->wValue >> 8) & 0xFF;

                switch (descriptor_type)
                {
                  case USB_DT_STRING:
                  {
                    uint8_t descriptor_idx = req->wValue & 0xFF;
                    sendStringDescriptor(descriptor_idx);
                  }
                  break;
                }
                break;
              }
            }
            break;

          case USB_CTRLTYPE_REC_INTERFACE:
            switch (req->bRequest)
            {
              case USB_REQ_GET_DESCRIPTOR:
              {
                uint8_t descriptor_type = (req->wValue >> 8) & 0xFF;
                //  uint8_t descriptor_idx = req->wValue & 0xFF;

                switch (descriptor_type)
                {
                  case HID_DESCRIPTOR_REPORT:
                    sendDescHidReport();
                    break;
                  default:
                    ksceKernelPrintf("unknown descriptor type %x\n", descriptor_type);
                }
              }
              break;

              default:
                ksceKernelPrintf("unknown request type %x\n", req->bRequest);
                break;
            }
            break;
        }
        break;

      case USB_CTRLTYPE_TYPE_CLASS:
        switch (recipient)
        {
          case USB_CTRLTYPE_REC_INTERFACE:
            switch (req->bRequest)
            {
              case HID_REQUEST_GET_REPORT:
              {
                uint8_t report_type = (req->wValue >> 8) & 0xFF;
                uint8_t report_id   = req->wValue & 0xFF;

                if (report_type == 1) // input
                {
                  sendInitialHidReport(report_id);
                }
                else if (report_type == 2) // output
                {
                }
                else if (report_type == 3) // feature report
                {
                  switch (report_id)
                  {
                    case 0x01: // default control
                      sendHid01Report(report_id);
                      break;

                    case 0xF2: // device address
                      sendHidF2Report(report_id, req->wLength);
                      break;

                    case 0xF4: // start device? stop device? switch to bt? 
                      ksceKernelSetEventFlag(g_event_flag_id, EVF_START);
                      break;

                    case 0xF5: // master mac
                      sendHidF5Report(report_id);
                      break;

                    case 0xF7: // ??
                      sendHidF7Report(report_id);
                      break;

                    case 0xF8: // paired status
                      sendHidF8Report(report_id);
                      break;

                    case 0xEF: // ??
                      sendHidEFReport(report_id);
                      break;

                    default:
                      ksceKernelPrintf("Unknown report type %x, id %x\n", report_type, report_id);
                      break;
                  }
                }
                else
                  ksceKernelPrintf("Unknown report type %x, id %x\n", report_type, report_id);
              }
              break;

              case HID_REQUEST_SET_REPORT:
              {
                usb_ep0_enqueue_recv_for_req(req, req->wValue);
              }
              break;
            }
            break;

          default:
            ksceKernelPrintf("Unknown recipient type %x\n", recipient);
            break;
        }
        break;
    }
  }
  else if (req_dir == USB_CTRLTYPE_DIR_HOST2DEVICE)
  {
    switch (req_type)
    {
      case USB_CTRLTYPE_TYPE_CLASS:
        switch (req_recipient)
        {
          case USB_CTRLTYPE_REC_INTERFACE:
            switch (req->bRequest)
            {
              case HID_REQUEST_SET_IDLE:
                break;
              case HID_REQUEST_SET_REPORT:
              {
                usb_ep0_enqueue_recv_for_req(req, req->wValue);
              }
              break;
            }
            break;
        }
        break;
    }
  }

  return 0;
}

static int attachUdcd(int usb_version, void *user_data)
{
  ksceUdcdReqCancelAll(&endpoints[1]);
  ksceUdcdClearFIFO(&endpoints[1]);
  ksceKernelSetEventFlag(g_event_flag_id, EVF_CONNECTED);
  g_usb_attached = 1;
  return 0;
}

static void detachUdcd(void *user_data)
{
  ksceKernelSetEventFlag(g_event_flag_id, EVF_DISCONNECTED);
  g_usb_attached = 0;
}

static int changeUdcdSetting()
{
  return 0;
}

static void configureUdcd() { }

static int startVividDriver(int size, void *args, void *user_data)
{
  return 0;
}

static int stopVividDriver(int size, void *args, void *user_data)
{
  return 0;
}

SceUdcdDriver vividUdcdDriver = {DRIVER_ID,
                                 2,
                                 &endpoints[0],
                                 &interfaces[0],
                                 &devdesc_hi,
                                 &config_hi,
                                 &devdesc_full,
                                 &config_full,
                                 &string_descriptors[0],
                                 &string_descriptors[0],
                                 &string_descriptors[0],
                                 &processUdcdRequest,
                                 &changeUdcdSetting,
                                 &attachUdcd,
                                 &detachUdcd,
                                 &configureUdcd,
                                 &startVividDriver,
                                 &stopVividDriver,
                                 0,
                                 0,
                                 NULL};

static int vividUsbThread(SceSize args, void *argp)
{
  ksceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
  unsigned int EVF_OUT;

  ksceKernelPrintf("waiting initial evf\n");

  while (1)
  {
    int response = ksceKernelWaitEventFlagCB(g_event_flag_id, EVF_ALL_MASK, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT,
                                             &EVF_OUT, NULL);
    if (response < 0 || g_exit_thread == 1)
    {
      break;
    }
    sendHidReport();
  }

  return 0;
}

int startGamepadThread()
{
  int ret = ksceKernelStartThread(g_usb_thread_id, 0, NULL);
  ksceKernelPrintf("Start thread 0x%08x\n", ret);
  return ret;
}

int disconnectDriver()
{
  return ksceKernelSetEventFlag(g_event_flag_id, EVF_DISCONNECTED);
}

void stopUsbDrivers()
{
  ksceUdcdStop("USB_MTP_Driver", 0, NULL);
  ksceUdcdStop("USBPSPCommunicationDriver", 0, NULL);
  ksceUdcdStop("USBSerDriver", 0, NULL);
  ksceUdcdStop("USBDeviceControllerDriver", 0, NULL);
  ksceUdcdStopCurrentInternal(2);
}

// exports

void vividPreventSleep()
{
  uint32_t state;
  ENTER_SYSCALL(state);

  ksceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);

  EXIT_SYSCALL(state);
}

void vividStart(void)
{
  uint32_t state;
  ENTER_SYSCALL(state);

  g_exit_thread  = 0;
  g_usb_attached = 2;

  ksceUdcdDeactivate();
  stopUsbDrivers();

  startGamepadThread();

  ksceUdcdStart("USBDeviceControllerDriver", 0, NULL);

  ksceUdcdStart(DRIVER_ID, 0, 0);
  ksceUdcdActivate(USB_PID);

  ksceKernelClearEventFlag(g_event_flag_id, ~EVF_ALL_MASK);

  EXIT_SYSCALL(state);
}

void vividStop(void)
{
  uint32_t state;
  ENTER_SYSCALL(state);

  g_exit_thread = 1;

  ksceKernelClearEventFlag(g_event_flag_id, EVF_ALL_MASK);
  ksceKernelSetEventFlag(g_event_flag_id, EVF_EXIT);
  ksceKernelWaitThreadEnd(g_usb_thread_id, NULL, NULL);
  ksceUdcdDeactivate();
  ksceUdcdStop(DRIVER_ID, 0, 0);
  stopUsbDrivers();

  ksceUdcdStart("USBDeviceControllerDriver", 0, NULL);
  ksceUdcdStart("USB_MTP_Driver", 0, NULL);
  ksceUdcdActivate(0x4E4);

  g_led_mask    = 0;

  EXIT_SYSCALL(state);
}

void vividUpdateL2(uint8_t pressed, uint8_t value)
{
  uint32_t state;
  ENTER_SYSCALL(state);

  g_l2_pressed = pressed;
  g_l2_value   = value;

  EXIT_SYSCALL(state);
}

void vividUpdateR2(uint8_t pressed, uint8_t value)
{
  uint32_t state;
  ENTER_SYSCALL(state);

  g_r2_pressed = pressed;
  g_r2_value   = value;

  EXIT_SYSCALL(state);
}

void vividUpdateL3(uint8_t pressed)
{
  uint32_t state;
  ENTER_SYSCALL(state);

  g_l3_pressed = pressed;

  EXIT_SYSCALL(state);
}

void vividUpdateR3(uint8_t pressed)
{
  uint32_t state;
  ENTER_SYSCALL(state);

  g_r3_pressed = pressed;

  EXIT_SYSCALL(state);
}

void vividUpdateAcc(uint16_t x, uint16_t y, uint16_t z)
{
  uint32_t state;
  ENTER_SYSCALL(state);

  g_acc_x = x;
  g_acc_y = y;
  g_acc_z = z;

  EXIT_SYSCALL(state);
}

void vividUpdateGyro(uint16_t z)
{
  uint32_t state;
  ENTER_SYSCALL(state);

  g_gyro_z = z;

  EXIT_SYSCALL(state);
}

void vividScreenOn()
{
  uint32_t state;
  ENTER_SYSCALL(state);

  if (g_is_oled)
  {
    ksceOledDisplayOn();
    ksceOledSetBrightness(g_prev_brightness);
  }
  else if (g_is_lcd)
  {
    ksceLcdDisplayOn();
    ksceLcdSetBrightness(g_prev_brightness);
  }

  EXIT_SYSCALL(state);
}

void vividScreenOff()
{
  uint32_t state;
  ENTER_SYSCALL(state);

  if (g_is_oled)
  {
//    g_prev_brightness = ksceOledGetBrightness();
    ksceOledDisplayOff();
  }
  else if (g_is_lcd)
  {
//    g_prev_brightness = ksceLcdGetBrightness();
    ksceLcdDisplayOff();
  }

  EXIT_SYSCALL(state);
}

int vividUsbAttached()
{
  return g_usb_attached;
}

uint8_t vividLedMask()
{
  return g_led_mask;
}

uint8_t vividVersion()
{
  return VIVID_MODULE_API;
}

void _start() __attribute__((weak, alias("module_start")));

// vitasdk has wrong definition
uint ksceBtGetStatusForTest(int type, void *data, int size);

int module_start(SceSize argc, const void *args)
{
  if (ksceKernelSearchModuleByName("SceLcd") >= 0)
  {
    g_is_lcd = 1;
  }
  else if (ksceKernelSearchModuleByName("SceOled") >= 0)
  {
    g_is_oled = 1;
  }

  ksceKernelPrintf("is_lcd: %d\n", g_is_lcd);
  ksceKernelPrintf("is_oled: %d\n", g_is_oled);

  if (g_is_oled)
  {
    g_prev_brightness = ksceOledGetBrightness();
  }
  else if (g_is_lcd)
  {
    g_prev_brightness = ksceLcdGetBrightness();
  }

  g_usb_thread_id = ksceKernelCreateThread("VITAPAD_USB_THREAD", vividUsbThread, 0x3C, 0x1000, 0, 0x10000, 0);
  g_event_flag_id = ksceKernelCreateEventFlag("VIVID_EF", 0, 0, NULL);
  ksceUdcdRegister(&vividUdcdDriver);

  uint8_t data[12];
  memset(data, 0, 12);

  ksceBtGetStatusForTest(0, &data, 8);
  g_my_mac[0] = data[5];
  g_my_mac[1] = data[4];
  g_my_mac[2] = data[3];
  g_my_mac[3] = data[2];
  g_my_mac[4] = data[1];
  g_my_mac[5] = data[0];

  // TODO: read host mac from config

  int res;

  tai_module_info_t tai_info;
  tai_info.size = sizeof(tai_module_info_t);
  taiGetModuleInfoForKernel(KERNEL_PID, "SceCtrl", &tai_info);
  module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x3ef9, (uintptr_t *)&ksceCtrlPeekBufferPositive2);

  return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
  vividStop();
  ksceUdcdUnregister(&vividUdcdDriver);
  ksceKernelDeleteThread(g_usb_thread_id);
  ksceKernelDeleteEventFlag(g_event_flag_id);

  return SCE_KERNEL_STOP_SUCCESS;
}
