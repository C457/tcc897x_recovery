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

#ifndef RECOVERY_SCREEN_UI_H
#define RECOVERY_SCREEN_UI_H

#include <pthread.h>

#include "ui.h"
#include "minui/minui.h"

#define CHAR_WIDTH 10
#define CHAR_HEIGHT 18

#define ERROR_LABLE_BG_DY 159
#define ERROR_LABLE_TITLE_DX 41
#define ERROR_LABLE_TITLE_DY 191

#define ERROR_LABLE_CODE_1ST_DX 690
#define ERROR_LABLE_CODE_2ND_DX 704
#define ERROR_LABLE_CODE_3TH_DX 718
#define ERROR_LABLE_CODE_4TH_DX 732

#define UPDATE_SCREEN_WIDTH 				1920
#define UPDATE_SCREEN_HEIGHT 				720

#define MESSAGE_HEIGHT_FIX_FIRST_DY         186
#define MESSAGE_HEIGHT_DEFAULT_DY           0
#define ERROR_LABLE_CODE_DY                 432

// TITLE : 90:720
#define TITLE_TEXT_DX                       105
#define TITLE_TEXT_DY                        30
#define TITLE_AR_TEXT_DX                    1500

#define SPIN_PROGRESS_DY                    200//213		// TITLE (60*1.5) + SPIN (82*1.5)

#define PROGRESS_LABLE_CURRENT_POSITION_DX  1290    //1190
#define PROGRESS_LABLE_SLASH_POSITION_DX    1304	//1204
#define PROGRESS_LABLE_TOTAL_POSITION_DX    1318	//1218
#define PROGRESS_LABLE_DY                    384	// 345

#define UPDATE_LABEL_DY                     318		// 90+123 + Label (62+8) * 1.5
#define UPDATE_PROGRESS_DY					384		// 90+123+105 + progress (44) *1.5
#define UPDATE_STATUS_MESSAGE_DY			459		// 90+123+105+66+ message (50*1.5)

// Implementation of RecoveryUI appropriate for devices with a screen
// (shows an icon + a progress bar, text logging, menu, etc.)
class ScreenRecoveryUI : public RecoveryUI {
  public:
    ScreenRecoveryUI();

    void Init();
    void SetLocale(const char* locale);

    // overall recovery state ("background image")
    void SetBackground(Icon icon);
    void ui_set_status_message(Messages icon);
    void ui_factory_reset();
    void ui_set_error_number(int error);
    void SetUpdateLableType(int mtype);

    // progress indicator
    void SetProgressType(ProgressType type);
    void ShowProgress(float portion, float seconds);
    void SetProgress(float fraction);

	// loading Icon
	void ShowSpinProgress(bool show);

    /* jason.ku 2018.01.08 */
    void RB_UI_Progress(float portion);

    // text log
    void ShowText(bool visible);
    bool IsTextVisible();
    bool WasTextEverVisible();

    void ShowSystemIcon(bool visible);

    // printing messages
    void Print(const char* fmt, ...); // __attribute__((format(printf, 1, 2)));

    // menu display
    virtual int MenuItemStart() const { return menu_item_start; }
    virtual int MenuItemHeight() const { return char_height; }
    void StartMenu(const char* const * headers, const char* const * items,
                           int initial_selection);
    int SelectMenu(int sel, bool abs = false);
    void EndMenu();

    void Redraw();

    enum UIElement { HEADER, MENU, TOP, MENU_SEL_BG, MENU_SEL_FG, LOG, TEXT_FILL, ERROR_TEXT };
    virtual void SetColor(UIElement e);

  private:
    Icon currentIcon;
    int installingFrame;
    bool rtl_locale;
	char setting_local[3];

    pthread_mutex_t updateMutex;
    gr_surface backgroundIcon[6];
    gr_surface *installationOverlay;
    gr_surface *progressBarIndeterminate;
    gr_surface progressBarEmpty;
    gr_surface progressBarFill;
	gr_surface spinProgressBar[16];
	gr_surface gSpinProgressBar;

    ProgressType progressBarType;

	bool spinProgress;
	int spinProgressPosition;
    float progressScopeStart, progressScopeSize, progress;
    double progressScopeTime, progressScopeDuration;

    // true when both graphics pages are the same (except for the
    // progress bar)
    bool pagesIdentical;

    static const int kMaxCols = 96;
    static const int kMaxRows = 96;

    static const int kMaxMenuCols = 96;
    static const int kMaxMenuRows = 250;
    // Log text overlay, displayed when a magic key is pressed
    char text[kMaxRows][kMaxCols];
    int text_cols, text_rows;
    int text_col, text_row, text_top;
    bool show_text;
    bool show_text_ever;   // has show_text ever been true?
    bool show_system_icon;

    char menu[kMaxRows][kMaxCols];
    bool show_menu;
    int menu_top, menu_items, menu_sel;
    int menu_show_start;
    int max_menu_rows;

    int menu_item_start;

    pthread_t progress_t;
    pthread_t spin_progress_t;

    int animation_fps;
    int indeterminate_frames;
    int installing_frames;
    int overlay_offset_x, overlay_offset_y;

    // daudio update images
    gr_surface gStatusMessageIcon[4];
    gr_surface gCurrentStatusMessageIcon;
    gr_surface gCurrentErrorNumberIcon[3];
    gr_surface gCurUpdateLable;
    gr_surface gCurUpdateLable_Micom;
    gr_surface gCurUpdateLable_Modem;
    gr_surface gCurUpdateLable_GPS;
    gr_surface gCurUpdateLable_SXM;
    gr_surface gCurUpdateLable_Monitor;
    gr_surface gCurUpdateLable_AMP;
    gr_surface gCurUpdateLable_HD;
    gr_surface gCurUpdateLable_Touch;
    gr_surface gCurUpdateLable_Keyboard;
    gr_surface gCurUpdateLable_CDP;
    gr_surface gCurUpdateLable_Hero;
    gr_surface gCurUpdateLable_Hifi2;
    gr_surface gCurUpdateLable_Epics;
    gr_surface gCurUpdateLable_Drm;
    gr_surface gCurUpdateLable_DAB;
    gr_surface gCurUpdateLable_Map;
    gr_surface gError_Code_Label;
    gr_surface gTitleText;
    gr_surface gSlash;
    gr_surface gErrorCodeImg[10];
    int gErrorCodeNum[4];

    char version_info_text[kMaxCols];
    int gCurUpdateLableType;

  protected:
    int install_overlay_offset_x, install_overlay_offset_y;

  private:

    int iconX, iconY;

    int stage, max_stage;

    int log_char_height, log_char_width;
    int char_height, char_width;

    int header_height;
    int header_width;
    int text_first_row;
    void draw_background_locked(Icon icon);
    void draw_cur_update_lable_locked(gr_surface icon);
    void draw_error_number_locked(gr_surface *icon);
    void draw_factory_reset();
    void draw_install_overlay_locked(int frame);
    void draw_progress_locked();
    void draw_screen_locked();
    void draw_status_message_locked(gr_surface icon);
    void draw_text_line(int row, const char* t);
    static void* factory_reset_thread(void *cookie);
	static void* progress_thread(void* cookie);
	static void* spin_progress_thread(void* cookie);
    void progress_loop();
    void update_progress_locked();
    void update_screen_locked();
    void update_versioninfo_display(const char *version);
    void version_print(const char *fmt, ...);

	void spin_progress_loop();
	void update_spin_progress_locked();
	void draw_spin_progress_locked();

    void LoadBitmap(const char* filename, gr_surface* surface);
    void LoadBitmapArray(const char* filename, int* frames, gr_surface** surface);
    void LoadLocalizedBitmap(const char* filename, gr_surface* surface);

    /* jason.ku 2018.01.09*/
    void ui_draw_progress_locked();
};

#endif  // RECOVERY_UI_H
