#include "shim.h"

#include <SDL_vulkan.h>

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

/* ── joystick identity ───────────────────────────────────────────── */

Uint16 SDL_JoystickGetVendor(SDL_Joystick *joystick) {
    (void)joystick;
    return 0;
}

Uint16 SDL_JoystickGetProduct(SDL_Joystick *joystick) {
    (void)joystick;
    return 0;
}

Uint16 SDL_JoystickGetProductVersion(SDL_Joystick *joystick) {
    (void)joystick;
    return 0;
}

SDL_bool SDL_JoystickHasRumble(SDL_Joystick *joystick) {
    (void)joystick;
    return SDL_FALSE;
}

SDL_JoystickGUID SDL_JoystickGetGUIDFromString(const char *pchGUID) {
    SDL_JoystickGUID guid;
    memset(&guid, 0, sizeof(guid));
    if (!pchGUID) return guid;

    for (size_t i = 0; i < sizeof(guid.data) && pchGUID[i * 2] && pchGUID[i * 2 + 1]; i++) {
        char byte[3] = { pchGUID[i * 2], pchGUID[i * 2 + 1], '\0' };
        guid.data[i] = (Uint8)strtoul(byte, NULL, 16);
    }
    return guid;
}

/* ── game controller mapping names ───────────────────────────────── */

static const char *const k_axis_names[SDL_CONTROLLER_AXIS_MAX] = {
    "leftx", "lefty", "rightx", "righty", "lefttrigger", "righttrigger",
};

static const char *const k_button_names[SDL_CONTROLLER_BUTTON_MAX] = {
    "a", "b", "x", "y", "back", "guide", "start",
    "leftstick", "rightstick", "leftshoulder", "rightshoulder",
    "dpup", "dpdown", "dpleft", "dpright",
    "misc1", "paddle1", "paddle2", "paddle3", "paddle4", "touchpad",
};

const char *SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis axis) {
    if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX) return NULL;
    return k_axis_names[axis];
}

const char *SDL_GameControllerGetStringForButton(SDL_GameControllerButton button) {
    if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX) return NULL;
    return k_button_names[button];
}

SDL_GameControllerButtonBind SDL_GameControllerGetBindForAxis(SDL_GameController *gamecontroller,
                                                              SDL_GameControllerAxis axis) {
    (void)gamecontroller; (void)axis;
    SDL_GameControllerButtonBind bind;
    memset(&bind, 0, sizeof(bind));
    bind.bindType = SDL_CONTROLLER_BINDTYPE_NONE;
    return bind;
}

SDL_GameControllerButtonBind SDL_GameControllerGetBindForButton(SDL_GameController *gamecontroller,
                                                                SDL_GameControllerButton button) {
    (void)gamecontroller; (void)button;
    SDL_GameControllerButtonBind bind;
    memset(&bind, 0, sizeof(bind));
    bind.bindType = SDL_CONTROLLER_BINDTYPE_NONE;
    return bind;
}

/* ── haptic effects ──────────────────────────────────────────────── */

unsigned int SDL_HapticQuery(SDL_Haptic *haptic) {
    (void)haptic;
    return 0;
}

int SDL_HapticIndex(SDL_Haptic *haptic) {
    (void)haptic;
    return -1;
}

int SDL_HapticNumAxes(SDL_Haptic *haptic) {
    (void)haptic;
    return -1;
}

int SDL_HapticNumEffects(SDL_Haptic *haptic) {
    (void)haptic;
    return -1;
}

int SDL_HapticNumEffectsPlaying(SDL_Haptic *haptic) {
    (void)haptic;
    return -1;
}

int SDL_HapticEffectSupported(SDL_Haptic *haptic, SDL_HapticEffect *effect) {
    (void)haptic; (void)effect;
    return SDL_FALSE;
}

int SDL_HapticNewEffect(SDL_Haptic *haptic, SDL_HapticEffect *effect) {
    (void)haptic; (void)effect;
    return SDL_SetError("haptic not available");
}

int SDL_HapticUpdateEffect(SDL_Haptic *haptic, int effect, SDL_HapticEffect *data) {
    (void)haptic; (void)effect; (void)data;
    return SDL_SetError("haptic not available");
}

int SDL_HapticRunEffect(SDL_Haptic *haptic, int effect, Uint32 iterations) {
    (void)haptic; (void)effect; (void)iterations;
    return SDL_SetError("haptic not available");
}

int SDL_HapticStopEffect(SDL_Haptic *haptic, int effect) {
    (void)haptic; (void)effect;
    return SDL_SetError("haptic not available");
}

void SDL_HapticDestroyEffect(SDL_Haptic *haptic, int effect) {
    (void)haptic; (void)effect;
}

int SDL_HapticGetEffectStatus(SDL_Haptic *haptic, int effect) {
    (void)haptic; (void)effect;
    return SDL_SetError("haptic not available");
}

int SDL_HapticStopAll(SDL_Haptic *haptic) {
    (void)haptic;
    return SDL_SetError("haptic not available");
}

int SDL_HapticPause(SDL_Haptic *haptic) {
    (void)haptic;
    return SDL_SetError("haptic not available");
}

int SDL_HapticUnpause(SDL_Haptic *haptic) {
    (void)haptic;
    return SDL_SetError("haptic not available");
}
