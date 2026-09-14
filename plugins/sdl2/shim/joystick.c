/* Virtual SDL gamepad.
 *
 * The host sends abstract core buttons (up/down/left/right/a/b/c/d/e/f/menu)
 * over IPC as GECND_SDL2_PKT_PAD. Here they become a single joystick that is
 * also a game controller, so LOVE's love.joystick / love.gamepad* callbacks
 * work without the client ever touching a real input device.
 *
 * Both layers are emitted, the way real SDL does it: SDL_JOY* for the raw
 * device and SDL_CONTROLLER* for the mapped one. Clients listen to one or the
 * other; LOVE listens to both and dispatches joystick* and gamepad* from them.
 *
 * Buttons b0..b6 are the face/shoulder/start buttons. The four directions are
 * hat 0 instead of buttons, which is what a d-pad is on real hardware and what
 * the mapping string below expects. There are no analog axes: the axis entry
 * points exist and report centred, so clients that poll them every frame (the
 * common case) see a stick at rest rather than crashing.
 */

#include "shim.h"

/* device index of our one pad, and its instance id — deliberately different
 * values, because SDL_JOYDEVICEADDED.which is a device index while every other
 * event's .which is an instance id, and mixing them up is the classic bug. */
#define PAD_INDEX    0
#define PAD_INSTANCE 1

#define PAD_NUM_AXES    6
#define PAD_NUM_BUTTONS GECND_SDL2_PAD_BUTTONS
#define PAD_NUM_HATS    1

static const char k_mapping[] =
    GECND_SDL2_PAD_GUID "," GECND_SDL2_PAD_NAME ","
    "a:b0,b:b1,x:b2,y:b3,"
    "leftshoulder:b4,rightshoulder:b5,start:b6,"
    "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,"
    "platform:Linux,";

static struct {
    bool  announced;
    bool  joystick_open;
    bool  controller_open;
    Uint8 buttons[PAD_NUM_BUTTONS];
    Uint8 hat;
} s_pad = { .hat = SDL_HAT_CENTERED };

/* opaque handles: clients only ever compare against NULL and hand them back */
static SDL_Joystick       *const k_joystick  = (SDL_Joystick *)&s_pad;
static SDL_GameController *const k_controller = (SDL_GameController *)&s_pad;

static bool pad_is_ours(const void *handle) {
    return handle == (const void *)&s_pad;
}

/* ── events ──────────────────────────────────────────────────────── */

static void pad_push_added(void) {
    SDL_Event ev;

    memset(&ev, 0, sizeof(ev));
    ev.type             = SDL_JOYDEVICEADDED;
    ev.jdevice.timestamp = SDL_GetTicks();
    ev.jdevice.which     = PAD_INDEX;      /* device index */
    shim_events_push(&ev);

    memset(&ev, 0, sizeof(ev));
    ev.type              = SDL_CONTROLLERDEVICEADDED;
    ev.cdevice.timestamp = SDL_GetTicks();
    ev.cdevice.which     = PAD_INDEX;      /* device index */
    shim_events_push(&ev);
}

static void pad_push_button(Uint8 button, bool pressed) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type            = pressed ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
    ev.jbutton.timestamp = SDL_GetTicks();
    ev.jbutton.which     = PAD_INSTANCE;   /* instance id */
    ev.jbutton.button    = button;
    ev.jbutton.state     = pressed ? SDL_PRESSED : SDL_RELEASED;
    shim_events_push(&ev);
}

static void pad_push_cbutton(Uint8 button, bool pressed) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type              = pressed ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
    ev.cbutton.timestamp = SDL_GetTicks();
    ev.cbutton.which     = PAD_INSTANCE;   /* instance id */
    ev.cbutton.button    = button;
    ev.cbutton.state     = pressed ? SDL_PRESSED : SDL_RELEASED;
    shim_events_push(&ev);
}

static void pad_push_hat(Uint8 value) {
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type           = SDL_JOYHATMOTION;
    ev.jhat.timestamp = SDL_GetTicks();
    ev.jhat.which     = PAD_INSTANCE;      /* instance id */
    ev.jhat.hat       = 0;
    ev.jhat.value     = value;
    shim_events_push(&ev);
}

/* b0..b6 sit in the same order as the SDL_CONTROLLER_BUTTON_* they map to?
 * No — SDL's enum puts back/guide/start before the shoulders, so spell the
 * mapping out instead of relying on the numbering lining up. */
static int pad_controller_button(uint8_t pad) {
    switch (pad) {
        case GECND_SDL2_PAD_A:    return SDL_CONTROLLER_BUTTON_A;
        case GECND_SDL2_PAD_B:    return SDL_CONTROLLER_BUTTON_B;
        case GECND_SDL2_PAD_C:    return SDL_CONTROLLER_BUTTON_X;
        case GECND_SDL2_PAD_D:    return SDL_CONTROLLER_BUTTON_Y;
        case GECND_SDL2_PAD_E:    return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        case GECND_SDL2_PAD_F:    return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        case GECND_SDL2_PAD_MENU: return SDL_CONTROLLER_BUTTON_START;
        case GECND_SDL2_PAD_UP:   return SDL_CONTROLLER_BUTTON_DPAD_UP;
        case GECND_SDL2_PAD_DOWN: return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        case GECND_SDL2_PAD_LEFT: return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
        case GECND_SDL2_PAD_RIGHT:return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        default:                  return -1;
    }
}

static Uint8 pad_hat_bit(uint8_t pad) {
    switch (pad) {
        case GECND_SDL2_PAD_UP:    return SDL_HAT_UP;
        case GECND_SDL2_PAD_DOWN:  return SDL_HAT_DOWN;
        case GECND_SDL2_PAD_LEFT:  return SDL_HAT_LEFT;
        case GECND_SDL2_PAD_RIGHT: return SDL_HAT_RIGHT;
        default:                   return 0;
    }
}

void shim_joystick_announce(void) {
    if (s_pad.announced) return;
    s_pad.announced = true;
    pad_push_added();
}

void shim_joystick_input(uint8_t pad, bool pressed) {
    if (pad >= GECND_SDL2_PAD_COUNT) return;

    /* A press that arrives before anyone asked for the device still has to
     * work, so make sure the added events are queued ahead of it. */
    shim_joystick_announce();

    int cbutton = pad_controller_button(pad);

    if (pad < PAD_NUM_BUTTONS) {
        if (s_pad.buttons[pad] == (pressed ? 1 : 0)) return;
        s_pad.buttons[pad] = pressed ? 1 : 0;
        pad_push_button(pad, pressed);
    } else {
        Uint8 bit = pad_hat_bit(pad);
        Uint8 hat = pressed ? (Uint8)(s_pad.hat | bit) : (Uint8)(s_pad.hat & ~bit);
        if (hat == s_pad.hat) return;
        s_pad.hat = hat;
        pad_push_hat(hat);
    }

    if (cbutton >= 0) pad_push_cbutton((Uint8)cbutton, pressed);
}

void shim_joystick_quit(void) {
    memset(s_pad.buttons, 0, sizeof(s_pad.buttons));
    s_pad.hat             = SDL_HAT_CENTERED;
    s_pad.announced       = false;
    s_pad.joystick_open   = false;
    s_pad.controller_open = false;
}

/* ── joystick api ────────────────────────────────────────────────── */

int SDL_NumJoysticks(void) {
    return 1;
}

SDL_Joystick *SDL_JoystickOpen(int device_index) {
    if (device_index != PAD_INDEX) {
        shim_set_error("no joystick %d", device_index);
        return NULL;
    }
    s_pad.joystick_open = true;
    return k_joystick;
}

void SDL_JoystickClose(SDL_Joystick *joystick) {
    if (pad_is_ours(joystick)) s_pad.joystick_open = false;
}

const char *SDL_JoystickName(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? GECND_SDL2_PAD_NAME : NULL;
}

const char *SDL_JoystickNameForIndex(int device_index) {
    return device_index == PAD_INDEX ? GECND_SDL2_PAD_NAME : NULL;
}

SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? PAD_INSTANCE : -1;
}

int SDL_JoystickNumAxes(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? PAD_NUM_AXES : -1;
}

int SDL_JoystickNumButtons(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? PAD_NUM_BUTTONS : -1;
}

int SDL_JoystickNumHats(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? PAD_NUM_HATS : -1;
}

int SDL_JoystickNumBalls(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? 0 : -1;
}

Sint16 SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis) {
    (void)joystick; (void)axis;
    return 0;
}

Uint8 SDL_JoystickGetButton(SDL_Joystick *joystick, int button) {
    if (!pad_is_ours(joystick) || button < 0 || button >= PAD_NUM_BUTTONS) return 0;
    return s_pad.buttons[button];
}

Uint8 SDL_JoystickGetHat(SDL_Joystick *joystick, int hat) {
    if (!pad_is_ours(joystick) || hat != 0) return SDL_HAT_CENTERED;
    return s_pad.hat;
}

SDL_JoystickPowerLevel SDL_JoystickCurrentPowerLevel(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? SDL_JOYSTICK_POWER_WIRED : SDL_JOYSTICK_POWER_UNKNOWN;
}

SDL_JoystickGUID SDL_JoystickGetDeviceGUID(int device_index) {
    if (device_index != PAD_INDEX) {
        SDL_JoystickGUID guid;
        memset(&guid, 0, sizeof(guid));
        return guid;
    }
    return SDL_JoystickGetGUIDFromString(GECND_SDL2_PAD_GUID);
}

SDL_JoystickGUID SDL_JoystickGetGUID(SDL_Joystick *joystick) {
    return SDL_JoystickGetDeviceGUID(pad_is_ours(joystick) ? PAD_INDEX : -1);
}

void SDL_JoystickGetGUIDString(SDL_JoystickGUID guid, char *pszGUID, int cbGUID) {
    if (!pszGUID || cbGUID <= 0) return;
    int n = 0;
    for (size_t i = 0; i < sizeof(guid.data) && n + 2 < cbGUID; i++) {
        n += snprintf(pszGUID + n, (size_t)(cbGUID - n), "%02x", guid.data[i]);
    }
    pszGUID[n < cbGUID ? n : cbGUID - 1] = '\0';
}

SDL_bool SDL_JoystickGetAttached(SDL_Joystick *joystick) {
    return pad_is_ours(joystick) ? SDL_TRUE : SDL_FALSE;
}

void SDL_JoystickUpdate(void) {
    /* event driven; state is already current */
}

int SDL_JoystickEventState(int state) {
    (void)state;
    return SDL_ENABLE;
}

int SDL_JoystickRumble(SDL_Joystick *joystick, Uint16 low, Uint16 high, Uint32 ms) {
    (void)joystick; (void)low; (void)high; (void)ms;
    return -1;   /* no rumble hardware behind this pad */
}

/* ── game controller api ─────────────────────────────────────────── */

SDL_bool SDL_IsGameController(int joystick_index) {
    return joystick_index == PAD_INDEX ? SDL_TRUE : SDL_FALSE;
}

SDL_GameController *SDL_GameControllerOpen(int joystick_index) {
    if (joystick_index != PAD_INDEX) {
        shim_set_error("no game controller %d", joystick_index);
        return NULL;
    }
    s_pad.controller_open = true;
    return k_controller;
}

void SDL_GameControllerClose(SDL_GameController *gamecontroller) {
    if (pad_is_ours(gamecontroller)) s_pad.controller_open = false;
}

SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *gamecontroller) {
    return pad_is_ours(gamecontroller) ? k_joystick : NULL;
}

/* SDL hands back heap memory here and the caller SDL_free()s it. */
static char *mapping_dup(void) {
    char *out = SDL_malloc(sizeof(k_mapping));
    if (out) memcpy(out, k_mapping, sizeof(k_mapping));
    return out;
}

char *SDL_GameControllerMapping(SDL_GameController *gamecontroller) {
    return pad_is_ours(gamecontroller) ? mapping_dup() : NULL;
}

char *SDL_GameControllerMappingForGUID(SDL_JoystickGUID guid) {
    SDL_JoystickGUID ours = SDL_JoystickGetGUIDFromString(GECND_SDL2_PAD_GUID);
    if (memcmp(&guid, &ours, sizeof(guid)) != 0) return NULL;
    return mapping_dup();
}

const char *SDL_GameControllerName(SDL_GameController *gamecontroller) {
    return pad_is_ours(gamecontroller) ? GECND_SDL2_PAD_NAME : NULL;
}

const char *SDL_GameControllerNameForIndex(int joystick_index) {
    return joystick_index == PAD_INDEX ? GECND_SDL2_PAD_NAME : NULL;
}

int SDL_GameControllerRumble(SDL_GameController *gamecontroller, Uint16 low, Uint16 high, Uint32 ms) {
    (void)gamecontroller; (void)low; (void)high; (void)ms;
    return -1;
}

int SDL_GameControllerAddMapping(const char *mappingString) {
    (void)mappingString;
    return 0;   /* accepted, but ours is fixed: 0 = "already known" */
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
    /* event driven; state is already current */
}

SDL_bool SDL_GameControllerGetAttached(SDL_GameController *gamecontroller) {
    return pad_is_ours(gamecontroller) ? SDL_TRUE : SDL_FALSE;
}

Sint16 SDL_GameControllerGetAxis(SDL_GameController *gamecontroller, SDL_GameControllerAxis axis) {
    (void)gamecontroller; (void)axis;
    return 0;   /* no analog input; sticks and triggers read as centred */
}

Uint8 SDL_GameControllerGetButton(SDL_GameController *gamecontroller, SDL_GameControllerButton button) {
    if (!pad_is_ours(gamecontroller)) return 0;
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A:             return s_pad.buttons[GECND_SDL2_PAD_A];
        case SDL_CONTROLLER_BUTTON_B:             return s_pad.buttons[GECND_SDL2_PAD_B];
        case SDL_CONTROLLER_BUTTON_X:             return s_pad.buttons[GECND_SDL2_PAD_C];
        case SDL_CONTROLLER_BUTTON_Y:             return s_pad.buttons[GECND_SDL2_PAD_D];
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return s_pad.buttons[GECND_SDL2_PAD_E];
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return s_pad.buttons[GECND_SDL2_PAD_F];
        case SDL_CONTROLLER_BUTTON_START:         return s_pad.buttons[GECND_SDL2_PAD_MENU];
        case SDL_CONTROLLER_BUTTON_DPAD_UP:       return (s_pad.hat & SDL_HAT_UP)    ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return (s_pad.hat & SDL_HAT_DOWN)  ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return (s_pad.hat & SDL_HAT_LEFT)  ? 1 : 0;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return (s_pad.hat & SDL_HAT_RIGHT) ? 1 : 0;
        default:                                  return 0;
    }
}
