#include "shim.h"

#include <SDL_vulkan.h>

/* ── joystick ────────────────────────────────────────────────────── */

int SDL_NumJoysticks(void) {
    return 0;
}

SDL_Joystick *SDL_JoystickOpen(int device_index) {
    shim_set_error("no joystick %d", device_index);
    return NULL;
}

void SDL_JoystickClose(SDL_Joystick *joystick) {
    (void)joystick;
}

const char *SDL_JoystickName(SDL_Joystick *joystick) {
    (void)joystick;
    return NULL;
}

const char *SDL_JoystickNameForIndex(int device_index) {
    (void)device_index;
    return NULL;
}

SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *joystick) {
    (void)joystick;
    return -1;
}

int SDL_JoystickNumAxes(SDL_Joystick *joystick) {
    (void)joystick;
    return 0;
}

int SDL_JoystickNumButtons(SDL_Joystick *joystick) {
    (void)joystick;
    return 0;
}

int SDL_JoystickNumHats(SDL_Joystick *joystick) {
    (void)joystick;
    return 0;
}

int SDL_JoystickNumBalls(SDL_Joystick *joystick) {
    (void)joystick;
    return 0;
}

Sint16 SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis) {
    (void)joystick; (void)axis;
    return 0;
}

Uint8 SDL_JoystickGetButton(SDL_Joystick *joystick, int button) {
    (void)joystick; (void)button;
    return 0;
}

Uint8 SDL_JoystickGetHat(SDL_Joystick *joystick, int hat) {
    (void)joystick; (void)hat;
    return SDL_HAT_CENTERED;
}

SDL_JoystickPowerLevel SDL_JoystickCurrentPowerLevel(SDL_Joystick *joystick) {
    (void)joystick;
    return SDL_JOYSTICK_POWER_UNKNOWN;
}

SDL_JoystickGUID SDL_JoystickGetGUID(SDL_Joystick *joystick) {
    (void)joystick;
    SDL_JoystickGUID guid;
    memset(&guid, 0, sizeof(guid));
    return guid;
}

SDL_JoystickGUID SDL_JoystickGetDeviceGUID(int device_index) {
    (void)device_index;
    SDL_JoystickGUID guid;
    memset(&guid, 0, sizeof(guid));
    return guid;
}

void SDL_JoystickGetGUIDString(SDL_JoystickGUID guid, char *pszGUID, int cbGUID) {
    (void)guid;
    if (pszGUID && cbGUID > 0) pszGUID[0] = '\0';
}

SDL_bool SDL_JoystickGetAttached(SDL_Joystick *joystick) {
    (void)joystick;
    return SDL_FALSE;
}

void SDL_JoystickUpdate(void) {
}

int SDL_JoystickEventState(int state) {
    (void)state;
    return SDL_ENABLE;
}

int SDL_JoystickRumble(SDL_Joystick *joystick, Uint16 low, Uint16 high, Uint32 ms) {
    (void)joystick; (void)low; (void)high; (void)ms;
    return -1;
}

/* ── game controller ─────────────────────────────────────────────── */

SDL_bool SDL_IsGameController(int joystick_index) {
    (void)joystick_index;
    return SDL_FALSE;
}

SDL_GameController *SDL_GameControllerOpen(int joystick_index) {
    shim_set_error("no game controller %d", joystick_index);
    return NULL;
}

void SDL_GameControllerClose(SDL_GameController *gamecontroller) {
    (void)gamecontroller;
}

SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *gamecontroller) {
    (void)gamecontroller;
    return NULL;
}

char *SDL_GameControllerMapping(SDL_GameController *gamecontroller) {
    (void)gamecontroller;
    return NULL;
}

char *SDL_GameControllerMappingForGUID(SDL_JoystickGUID guid) {
    (void)guid;
    return NULL;
}

const char *SDL_GameControllerName(SDL_GameController *gamecontroller) {
    (void)gamecontroller;
    return NULL;
}

const char *SDL_GameControllerNameForIndex(int joystick_index) {
    (void)joystick_index;
    return NULL;
}

int SDL_GameControllerRumble(SDL_GameController *gamecontroller, Uint16 low, Uint16 high, Uint32 ms) {
    (void)gamecontroller; (void)low; (void)high; (void)ms;
    return -1;
}

int SDL_GameControllerAddMapping(const char *mappingString) {
    (void)mappingString;
    return 0;
}

int SDL_GameControllerAddMappingsFromRW(SDL_RWops *rw, int freerw) {
    (void)rw; (void)freerw;
    return 0;
}

int SDL_GameControllerEventState(int state) {
    (void)state;
    return SDL_ENABLE;
}

void SDL_GameControllerUpdate(void) {
}

SDL_bool SDL_GameControllerGetAttached(SDL_GameController *gamecontroller) {
    (void)gamecontroller;
    return SDL_FALSE;
}

Sint16 SDL_GameControllerGetAxis(SDL_GameController *gamecontroller, SDL_GameControllerAxis axis) {
    (void)gamecontroller; (void)axis;
    return 0;
}

Uint8 SDL_GameControllerGetButton(SDL_GameController *gamecontroller, SDL_GameControllerButton button) {
    (void)gamecontroller; (void)button;
    return 0;
}

/* ── haptic ──────────────────────────────────────────────────────── */

int SDL_NumHaptics(void) {
    return 0;
}

SDL_Haptic *SDL_HapticOpenFromJoystick(SDL_Joystick *joystick) {
    (void)joystick;
    shim_set_error("haptic not available");
    return NULL;
}

void SDL_HapticClose(SDL_Haptic *haptic) {
    (void)haptic;
}

int SDL_HapticRumbleInit(SDL_Haptic *haptic) {
    (void)haptic;
    return -1;
}

int SDL_HapticRumblePlay(SDL_Haptic *haptic, float strength, Uint32 length) {
    (void)haptic; (void)strength; (void)length;
    return -1;
}

int SDL_HapticRumbleStop(SDL_Haptic *haptic) {
    (void)haptic;
    return -1;
}

int SDL_HapticSetAutocenter(SDL_Haptic *haptic, int autocenter) {
    (void)haptic; (void)autocenter;
    return -1;
}

int SDL_JoystickIsHaptic(SDL_Joystick *joystick) {
    (void)joystick;
    return 0;
}

/* ── sensor ──────────────────────────────────────────────────────── */

int SDL_NumSensors(void) {
    return 0;
}

SDL_Sensor *SDL_SensorOpen(int device_index) {
    (void)device_index;
    return NULL;
}

void SDL_SensorClose(SDL_Sensor *sensor) {
    (void)sensor;
}

SDL_Sensor *SDL_SensorFromInstanceID(SDL_SensorID instance_id) {
    (void)instance_id;
    return NULL;
}

SDL_SensorType SDL_SensorGetDeviceType(int device_index) {
    (void)device_index;
    return SDL_SENSOR_INVALID;
}

SDL_SensorType SDL_SensorGetType(SDL_Sensor *sensor) {
    (void)sensor;
    return SDL_SENSOR_INVALID;
}

SDL_SensorID SDL_SensorGetInstanceID(SDL_Sensor *sensor) {
    (void)sensor;
    return -1;
}

SDL_SensorID SDL_SensorGetDeviceInstanceID(int device_index) {
    (void)device_index;
    return -1;
}

int SDL_SensorGetData(SDL_Sensor *sensor, float *data, int num_values) {
    (void)sensor; (void)data; (void)num_values;
    return -1;
}

void SDL_SensorUpdate(void) {
}

/* ── touch ───────────────────────────────────────────────────────── */

int SDL_GetNumTouchDevices(void) {
    return 0;
}

SDL_TouchID SDL_GetTouchDevice(int index) {
    (void)index;
    return 0;
}

int SDL_GetNumTouchFingers(SDL_TouchID touchID) {
    (void)touchID;
    return 0;
}

SDL_Finger *SDL_GetTouchFinger(SDL_TouchID touchID, int index) {
    (void)touchID; (void)index;
    return NULL;
}

/* ── vulkan ──────────────────────────────────────────────────────── */

int SDL_Vulkan_LoadLibrary(const char *path) {
    (void)path;
    shim_set_error("vulkan not available");
    return -1;
}

void SDL_Vulkan_UnloadLibrary(void) {
}

void *SDL_Vulkan_GetVkGetInstanceProcAddr(void) {
    shim_set_error("vulkan not available");
    return NULL;
}

SDL_bool SDL_Vulkan_GetInstanceExtensions(SDL_Window *window, unsigned int *pCount, const char **pNames) {
    (void)window; (void)pNames;
    if (pCount) *pCount = 0;
    shim_set_error("vulkan not available");
    return SDL_FALSE;
}

SDL_bool SDL_Vulkan_CreateSurface(SDL_Window *window, VkInstance instance, VkSurfaceKHR *surface) {
    (void)window; (void)instance; (void)surface;
    shim_set_error("vulkan not available");
    return SDL_FALSE;
}

void SDL_Vulkan_GetDrawableSize(SDL_Window *window, int *w, int *h) {
    SDL_GL_GetDrawableSize(window, w, h);
}
