#include "vivid-user.h"

#include <SDL.h>
#include <psp2/appmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr/thread.h>
#include <psp2/power.h>
#include <psp2/shellutil.h>
#include <psp2/vshbridge.h>
#include <taihen.h>

#define MOD_PATH "ux0:app/VVID00001/module/vividk.skprx"

unsigned int _newlib_heap_size_user = 32 * 1024 * 1024;

static SDL_Window *g_window        = NULL;
static SDL_Renderer *g_renderer    = NULL;
static SDL_Texture *g_tex_inactive = NULL;
static SDL_Texture *g_tex_active   = NULL;
static int g_mode                  = 0;
static int g_start_pressed         = 0;
static int g_select_pressed        = 0;
static int g_screen_off            = 0;

static int g_arm_clock;
static int g_bus_clock;
static int g_gpu_clock;
static int g_xbar_clock;

enum
{
  B_L2 = 0,
  B_R2 = 1,
  B_L3 = 2,
  B_R3 = 3
};

typedef struct
{
  SDL_FingerID finger;
  uint8_t pressed;
  uint8_t value;
} shoulders;

static shoulders buttons[4] = {0};

void saveSystemClocks()
{
  g_arm_clock  = scePowerGetArmClockFrequency();
  g_bus_clock  = scePowerGetBusClockFrequency();
  g_gpu_clock  = scePowerGetGpuClockFrequency();
  g_xbar_clock = scePowerGetGpuClockFrequency();
}

void applySystemClocks(int arm, int bus, int gpu, int gpuXbar)
{
  scePowerSetArmClockFrequency(arm);
  scePowerSetBusClockFrequency(bus);
  scePowerSetGpuClockFrequency(gpu);
  scePowerSetGpuXbarClockFrequency(gpuXbar);
}

void applyUnderClock()
{
  applySystemClocks(111, 111, 111, 111);
}

void resetSystemClocks()
{
  applySystemClocks(g_arm_clock, g_bus_clock, g_gpu_clock, g_xbar_clock);
}

void lockUsbAndControls()
{
  sceShellUtilInitEvents(0);
  sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2);
  sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_POWEROFF_MENU);
  sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_USB_CONNECTION);
}

void unlockUsbAndControls()
{
  sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2);
  sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_POWEROFF_MENU);
  sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_USB_CONNECTION);
}

int init()
{
  SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");

  if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    return -1;

  if ((g_window
       = SDL_CreateWindow("ViViD", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 960, 544, SDL_WINDOW_SHOWN))
      == NULL)
    return -1;

  if ((g_renderer = SDL_CreateRenderer(g_window, -1, 0)) == NULL)
    return -1;

  SDL_Surface *temp = SDL_LoadBMP("data/inactive.bmp");
  if (!temp)
    return -1;
  g_tex_inactive = SDL_CreateTextureFromSurface(g_renderer, temp);
  if (!g_tex_inactive)
    return -1;
  SDL_FreeSurface(temp);

  temp = SDL_LoadBMP("data/active.bmp");
  if (!temp)
    return -1;
  g_tex_active = SDL_CreateTextureFromSurface(g_renderer, temp);
  if (!g_tex_active)
    return -1;
  SDL_FreeSurface(temp);

  SDL_GameControllerOpen(0);

  return 0;
}

void startPad()
{
  g_mode = 1;
  vividStart();
  vividPreventSleep();
  lockUsbAndControls();
  applyUnderClock();
}

void stopPad()
{
  g_mode = 0;
  vividStop();
  if (g_screen_off)
    vividScreenOn();
  unlockUsbAndControls();
  resetSystemClocks();
}

void toggleScreen()
{
  sceClibPrintf("toggle screen!\n");
  g_screen_off = !g_screen_off;
  if (g_screen_off)
  {
    vividScreenOff();
  }
  else
  {
    vividScreenOn();
  }
}

void updateInput()
{
  vividUpdateL2(buttons[B_L2].pressed, buttons[B_L2].value);
  vividUpdateR2(buttons[B_R2].pressed, buttons[B_R2].value);
  vividUpdateL3(buttons[B_L3].pressed);
  vividUpdateR3(buttons[B_R3].pressed);
}

void pollInput()
{
  SDL_Event event;

  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      case SDL_CONTROLLERBUTTONDOWN:
        if (g_mode == 0)
        {
          if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
            startPad();
        }
        else
        {
          if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)
          {
            g_select_pressed = 1;
          }
          if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START)
          {
            g_start_pressed = 1;
          }

          if (g_start_pressed && g_select_pressed)
          {
            stopPad();
          }
        }
        break;
      case SDL_CONTROLLERBUTTONUP:
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)
          g_select_pressed = 0;
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START)
          g_start_pressed = 0;
        break;
      case SDL_FINGERDOWN:
        if (g_mode == 1)
        {
          if (event.tfinger.touchId == 1) // back
          {
            if (event.tfinger.x < 0.5 && event.tfinger.y >= 0 && event.tfinger.y <= 0.3) // L2
            {
              buttons[B_L2].finger  = event.tfinger.fingerId;
              buttons[B_L2].pressed = 1;
              buttons[B_L2].value   = event.tfinger.y * (1.0f / 0.3f) * 255.0f;
            }
            else if (event.tfinger.x > 0.5 && event.tfinger.y >= 0 && event.tfinger.y <= 0.3) // L2
            {
              buttons[B_R2].finger  = event.tfinger.fingerId;
              buttons[B_R2].pressed = 1;
              buttons[B_R2].value   = event.tfinger.y * (1.0f / 0.3f) * 255.0f;
            }
            else if (event.tfinger.x < 0.5 && event.tfinger.y >= 0.6 && event.tfinger.y <= 1.0) // L3
            {
              buttons[B_L3].finger  = event.tfinger.fingerId;
              buttons[B_L3].pressed = 1;
              buttons[B_L3].value   = 255;
            }
            else if (event.tfinger.x > 0.5 && event.tfinger.y >= 0.6 && event.tfinger.y <= 1.0) // R3
            {
              buttons[B_R3].finger  = event.tfinger.fingerId;
              buttons[B_R3].pressed = 1;
              buttons[B_R3].value   = 255;
            }
          }
          else // front
          {
            toggleScreen();
          }
        }
        break;
      case SDL_FINGERUP:
        for (int i = B_L2; i <= B_R3; i++)
        {
          if (buttons[i].finger == event.tfinger.fingerId)
          {
            buttons[i].finger  = 0;
            buttons[i].pressed = 0;
            buttons[i].value   = 0;
          }
        }
        break;
      case SDL_FINGERMOTION:
        if (g_mode == 1)
        {
          for (int i = B_L2; i <= B_R3; i++)
          {
            if (buttons[i].finger == event.tfinger.fingerId)
            {
              // clamp
              switch (i)
              {
                case B_L2:
                case B_R2:
                {
                  float y = event.tfinger.y;
                  if (y > 0.3f)
                    y = 0.3f;
                  buttons[i].value = y * (1.0f / 0.3f) * 255;
                }
                break;
                default:
                  break;
              }
            }
          }
        }
        break;
      default:
        break;
    }
  }
}

void pollStatus()
{
  if (!vividUsbAttached())
  {
    stopPad();
  }
}

void drawLeds()
{
  uint8_t mask = vividLedMask();
  //    sceClibPrintf("mask: 0x%08x\n", mask);
  SDL_Rect l1 = {392, 16, 32, 16};
  SDL_Rect l2 = {440, 16, 32, 16};

  SDL_Rect l3 = {488, 16, 32, 16};
  SDL_Rect l4 = {536, 16, 32, 16};

  if (mask & 0x2)
    SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
  else
    SDL_SetRenderDrawColor(g_renderer, 60, 60, 60, 255);
  SDL_RenderFillRect(g_renderer, &l1);

  if (mask & 0x4)
    SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
  else
    SDL_SetRenderDrawColor(g_renderer, 60, 60, 60, 255);
  SDL_RenderFillRect(g_renderer, &l2);

  if (mask & 0x8)
    SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
  else
    SDL_SetRenderDrawColor(g_renderer, 60, 60, 60, 255);
  SDL_RenderFillRect(g_renderer, &l3);

  if (mask & 0x10)
    SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
  else
    SDL_SetRenderDrawColor(g_renderer, 60, 60, 60, 255);
  SDL_RenderFillRect(g_renderer, &l4);
}

int main(int argc, char *argv[])
{
  int search_param[2];
  SceUID res = _vshKernelSearchModuleByName("vividk", search_param);
  if (res <= 0)
  {
    SceUID mod_id;
    mod_id = taiLoadStartKernelModule(MOD_PATH, 0, NULL, 0);
    sceClibPrintf("0x%08x\n", mod_id);
    sceKernelDelayThread(1000000);
    sceAppMgrLoadExec("app0:eboot.bin", NULL, NULL);
  }

  if (init() < 0)
    return 0;

  while (1)
  {
    SDL_RenderClear(g_renderer);
    pollStatus();
    SDL_RenderCopy(g_renderer, g_mode ? g_tex_active : g_tex_inactive, NULL, NULL);
    pollInput();
    if (g_mode == 1)
    {
      updateInput();
      drawLeds();
    }
    SDL_RenderPresent(g_renderer);
    SDL_Delay(100); // yield
  }

  SDL_DestroyTexture(g_tex_inactive);
  SDL_DestroyTexture(g_tex_active);
  SDL_DestroyRenderer(g_renderer);
  SDL_DestroyWindow(g_window);

  SDL_Quit();
  return 0;
}
