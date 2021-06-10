/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RECOVERY_UI_H
#define RECOVERY_UI_H

#include <linux/input.h>
#include <pthread.h>
#include <time.h>

#define MAX_NR_INPUT_DEVICES    8
#define MAX_NR_VKEYS            8
/*
 * Simple representation of a (x,y) coordinate with convenience operators
 */
struct point {
    point() : x(0), y(0) {}
    point operator+(const point& rhs) const {
        point tmp;
        tmp.x = x + rhs.x;
        tmp.y = y + rhs.y;
        return tmp;
    }
    point operator-(const point& rhs) const {
        point tmp;
        tmp.x = x - rhs.x;
        tmp.y = y - rhs.y;
        return tmp;
    }

    int         x;
    int         y;
};

/*
 * Virtual key representation.  Valid when keycode != -1.
 */
struct vkey {
    vkey() : keycode(-1) {}
    int         keycode;
    point       min;
    point       max;
};

/*
 * Input device representation.  Valid when fd != -1.
 * This holds all information and state related to a given input device.
 */
struct input_device {
    input_device() : fd(-1) {}

    int         fd;
    vkey        virtual_keys[MAX_NR_VKEYS];
    point       touch_min;
    point       touch_max;

    int         rel_sum;            // Accumulated relative movement

    bool        saw_pos_x;          // Did sequence have ABS_MT_POSITION_X?
    bool        saw_pos_y;          // Did sequence have ABS_MT_POSITION_Y?
    bool        saw_mt_report;      // Did sequence have SYN_MT_REPORT?
    bool        saw_tracking_id;    // Did sequence have SYN_TRACKING_ID?
    bool        in_touch;           // Are we in a touch event?
    bool        in_swipe;           // Are we in a swipe event?

    point       touch_pos;          // Current touch coordinates
    point       touch_start;        // Coordinates of touch start
    point       touch_track;        // Last tracked coordinates

    int         slot_nr_active;
    int         slot_first;
    int         slot_current;
    int         tracking_id;
};

// Abstract class for controlling the user interface during recovery.
class RecoveryUI {
  public:
    RecoveryUI();

    virtual ~RecoveryUI() { }

    // Initialize the object; called before anything else.
    virtual void Init();

    // After calling Init(), you can tell the UI what locale it is operating in.
    virtual void SetLocale(const char* locale) { }

    enum Icon { NONE, INSTALLING_UPDATE, ERASING, NO_COMMAND, FACTORY_RESET, ERROR };
    enum Messages{ INSTALLING_MESSAGE, INSATLL_ERROR_MESSAGE,INSTALL_SUCCESS_MESSAGE,INSTALL_ERROR_CONTACN_CUSTOMER };
    enum TitleText{
        CUR_UPDATE_LABLE_TYPE_NONE,
        CUR_UPDATE_LABLE_TYPE_NORMAL,
        CUR_UPDATE_LABLE_TYPE_MICOM,
        CUR_UPDATE_LABLE_TYPE_MODEM,
        CUR_UPDATE_LABLE_TYPE_GPS,
        CUR_UPDATE_LABLE_TYPE_SXM,
        CUR_UPDATE_LABLE_TYPE_eCom,
        CUR_UPDATE_LABLE_TYPE_AMP,
        CUR_UPDATE_LABLE_TYPE_HD,
        CUR_UPDATE_LABLE_TYPE_MONITOR,
        CUR_UPDATE_LABLE_TYPE_TOUCH,
        CUR_UPDATE_LABLE_TYPE_KEYBOARD,
        CUR_UPDATE_LABLE_TYPE_CDP,
        CUR_UPDATE_LABLE_TYPE_HERO,
        CUR_UPDATE_LABLE_TYPE_HIFI2,
        CUR_UPDATE_LABLE_TYPE_EPICS,
        CUR_UPDATE_LABLE_TYPE_DRM,
        CUR_UPDATE_LABLE_TYPE_DAB,
        CUR_UPDATE_LABLE_TYPE_MAP,
        CUR_UPDATE_LABLE_TYPE_DISAPPEARED,

        CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR      = 300,
        CUR_UPDATE_LABLE_TYPE_DMB_ERROR         = 400,
        CUR_UPDATE_LABLE_TYPE_DAB_ERROR         = 500,
        CUR_UPDATE_LABLE_TYPE_MODEM_ERROR       = 600,
        CUR_UPDATE_LABLE_TYPE_GPS_ERROR         = 700,
        CUR_UPDATE_LABLE_TYPE_TOUCH_ERROR       = 900,
        CUR_UPDATE_LABLE_TYPE_MONITOR_ERROR     = 1000,
        CUR_UPDATE_LABLE_TYPE_HD_ERROR          = 1100,
        CUR_UPDATE_LABLE_TYPE_AMP_ERROR         = 1200,
        CUR_UPDATE_LABLE_TYPE_MICOM_ERROR       = 1400,
        CUR_UPDATE_LABLE_TYPE_SXM_ERROR         = 1500,
        CUR_UPDATE_LABLE_TYPE_KEYBOARD_ERROR    = 1600,
        CUR_UPDATE_LABLE_TYPE_CDP_ERROR         = 1700,
        CUR_UPDATE_LABLE_TYPE_CONTACT_CUSTOMER  = 1800,
        CUR_UPDATE_LABLE_TYPE_HIFI2_ERROR       = 1900,
        CUR_UPDATE_LABLE_TYPE_EPICS_ERROR       = 2000,
        CUR_UPDATE_LABLE_TYPE_DRM_ERROR         = 2100,
    };

    // Set the overall recovery state ("background image").
    virtual void SetBackground(Icon icon) = 0;
    virtual void ui_set_status_message(Messages icon) =0;
    virtual void ui_set_error_number(int error) = 0;
    virtual void SetUpdateLableType(int mtype) = 0;

    // --- progress indicator ---
    enum ProgressType { EMPTY, INDETERMINATE, DETERMINATE };
    virtual void SetProgressType(ProgressType determinate) = 0;

    // Show a progress bar and define the scope of the next operation:
    //   portion - fraction of the progress bar the next operation will use
    //   seconds - expected time interval (progress bar moves at this minimum rate)
    virtual void ShowProgress(float portion, float seconds) = 0;

    // Set progress bar position (0.0 - 1.0 within the scope defined
    // by the last call to ShowProgress).
    virtual void SetProgress(float fraction) = 0;
    /* jason.ku 2018.01.08 */
    virtual void RB_UI_Progress(float portion) = 0;

    // --- text log ---

    virtual void ShowText(bool visible) = 0;

    virtual bool IsTextVisible() = 0;

    virtual bool WasTextEverVisible() = 0;

    // Write a message to the on-screen log (shown if the user has
    // toggled on the text display).
    virtual void Print(const char* fmt, ...) = 0; // __attribute__((format(printf, 1, 2))) = 0;

    // --- key handling ---

    // Wait for keypress and return it.  May return -1 after timeout.
    virtual int WaitKey();

    virtual bool IsKeyPressed(int key);

    // Erase any queued-up keys.
    virtual void FlushKeys();

    // Called on each keypress, even while operations are in progress.
    // Return value indicates whether an immediate operation should be
    // triggered (toggling the display, rebooting the device), or if
    // the key should be enqueued for use by the main thread.
    enum KeyAction { ENQUEUE, TOGGLE, REBOOT, IGNORE };
    virtual KeyAction CheckKey(int key);

    // Called immediately before each call to CheckKey(), tell you if
    // the key was long-pressed.
    virtual void NextCheckKeyIsLong(bool is_long_press);

    // Called when a key is held down long enough to have been a
    // long-press (but before the key is released).  This means that
    // if the key is eventually registered (released without any other
    // keys being pressed in the meantime), NextCheckKeyIsLong() will
    // be called with "true".
    virtual void KeyLongPress(int key);

    // --- menu display ---

    virtual int MenuItemStart() const = 0;
    virtual int MenuItemHeight() const = 0;

    // Display some header text followed by a menu of items, which appears
    // at the top of the screen (in place of any scrolling ui_print()
    // output, if necessary).
    virtual void StartMenu(const char* const * headers, const char* const * items,
                           int initial_selection) = 0;

    // Set the menu highlight to the given index, and return it (capped to
    // the range [0..numitems).
    virtual int SelectMenu(int sel, bool abs = false) = 0;

    // End menu mode, resetting the text overlay so that ui_print()
    // statements will be displayed.
    virtual void EndMenu() = 0;

    // System update icon delete
    virtual void ShowSystemIcon(bool visible) = 0;

	// Loading Icons
	virtual void ShowSpinProgress(bool show) = 0;

    // set daudio update step count (ex : 1/1, 1/2)
    int countTotal;
    int countPresent;

protected:
    void EnqueueKey(int key_code);

private:
    // Key event input queue
    pthread_mutex_t key_queue_mutex;
    pthread_cond_t key_queue_cond;
    int key_queue[256], key_queue_len;
    char key_pressed[KEY_MAX + 1];     // under key_queue_mutex
    int key_last_down;                 // under key_queue_mutex
    bool key_long_press;               // under key_queue_mutex
    int key_down_count;                // under key_queue_mutex
    bool enable_reboot;
    int rel_sum;

    input_device input_devices[MAX_NR_INPUT_DEVICES];

    point fb_dimensions;
    point min_swipe_px;

    typedef struct {
        RecoveryUI* ui;
        int key_code;
        int count;
    } key_timer_t;

    pthread_t input_t;

    static void* input_thread(void* cookie);
    static int input_callback(int fd, uint32_t epevents, void* data);
    void process_key(input_device* dev, int key_code, int updown);
    void process_syn(input_device* dev, int code, int value);
    void process_abs(input_device* dev, int code, int value);
    void process_rel(input_device* dev, int code, int value);
    bool usb_connected();

    static void* time_key_helper(void* cookie);
    void time_key(int key_code, int count);

    void calibrate_touch(input_device* dev);
    void setup_vkeys(input_device* dev);
    void calibrate_swipe();
    void handle_press(input_device* dev);
    void handle_release(input_device* dev);
    void handle_gestures(input_device* dev);
};

#endif  // RECOVERY_UI_H
