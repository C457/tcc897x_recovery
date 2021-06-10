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

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "device.h"
#include "minui/minui.h"
#include "screen_ui.h"
#include "ui.h"

// There's only (at most) one of these objects, and global callbacks
// (for pthread_create, and the input event system) need to find it,
// so use a global variable.
static ScreenRecoveryUI* self = NULL;

#define MAX_COLS 96
#define PROGRESSBAR_INDETERMINATE_FPS 15

static gr_surface gFactoryReset;

// Return the current time as a double (including fractions of a second).
static double now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

ScreenRecoveryUI::ScreenRecoveryUI() :
    currentIcon(NONE),
    installingFrame(0),
    rtl_locale(false),
    progressBarType(EMPTY),
    progressScopeStart(0),
    progressScopeSize(0),
    progress(0),
	spinProgress(false),
	spinProgressPosition(0),
    pagesIdentical(false),
    text_cols(0),
    text_rows(0),
    text_col(0),
    text_row(0),
    text_top(0),
    show_text(false),
    show_text_ever(false),
    show_system_icon(true),
    show_menu(false),
    menu_top(0),
    menu_items(0),
    menu_sel(0),
    menu_show_start(0),
    max_menu_rows(0),
    // These values are correct for the default image resources
    // provided with the android platform.  Devices which use
    // different resources should have a subclass of ScreenRecoveryUI
    // that overrides Init() to set these values appropriately and
    // then call the superclass Init().
    animation_fps(20),
    indeterminate_frames(6),
    installing_frames(7),
    overlay_offset_x(-1),
    overlay_offset_y(-1),
    install_overlay_offset_x(13),
    install_overlay_offset_y(190) {

    for (int i = 0; i < 5; i++)
        backgroundIcon[i] = NULL;

    pthread_mutex_init(&updateMutex, NULL);
    self = this;
}

// Draw the given frame over the installation overlay animation.  The
// background is not cleared or draw with the base icon first; we
// assume that the frame already contains some other frame of the
// animation.  Does nothing if no overlay animation is defined.
// Should only be called with updateMutex locked.
void ScreenRecoveryUI::draw_install_overlay_locked(int frame) {
    if (installationOverlay == NULL || overlay_offset_x < 0)
        return;

    gr_surface surface = installationOverlay[frame];
    int iconWidth = gr_get_width(surface);
    int iconHeight = gr_get_height(surface);
    gr_blit(surface, 0, 0, iconWidth, iconHeight,
            overlay_offset_x, overlay_offset_y);
}

// Clear the screen and draw the currently selected background icon (if any).
// Should only be called with updateMutex locked.
void ScreenRecoveryUI::draw_background_locked(Icon icon)
{
    pagesIdentical = false;

	gr_color(0, 0, 0, 255);
	gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    if (icon) {
        int iconWidth =0;
        int iconHeight = 0;
        int iconX = 0;
        int iconY = 0;

        gr_surface surface = backgroundIcon[icon];
        iconWidth = gr_get_width(surface);
        iconHeight = gr_get_height(surface);
        iconX = (gr_fb_width() - iconWidth) / 2;
        iconY = (gr_fb_height() - iconHeight) / 2;
        gr_blit(surface, 0, 0, iconWidth, iconHeight, iconX, iconY);

        //title text draw
        iconWidth = gr_get_width(gTitleText);
        iconHeight = gr_get_height(gTitleText);

		/*
        //if(ar_locale) {
        //    iconX = TITLE_AR_TEXT_DX;
        //} else {
            iconX = TITLE_TEXT_DX;
        //} // CJUNG TODO : middle east text - right sort
		*/

        iconX = TITLE_TEXT_DX;
        iconY = TITLE_TEXT_DY;

        if(show_system_icon)
            gr_blit(gTitleText, 0, 0, iconWidth, iconHeight, iconX, iconY);
    }
}

void ScreenRecoveryUI::draw_status_message_locked(gr_surface icon)
{
    int width = gr_get_width(icon);
    int height = gr_get_height(icon);

    int dx = (gr_fb_width() - width) / 2;
    int dy = UPDATE_STATUS_MESSAGE_DY;

    if(gCurUpdateLableType >= CUR_UPDATE_LABLE_TYPE_DISAPPEARED){
        dy = (UPDATE_SCREEN_HEIGHT - height)/2;
    }

    if(icon) {
		gr_color(0, 0, 0, 255);
		gr_fill(dx, dy, width, height);
        gr_blit(icon, 0, 0, width, height, dx, dy);
    }
}

void ScreenRecoveryUI::draw_error_number_locked(gr_surface *icon)
{
    int iconHeight = UPDATE_SCREEN_HEIGHT;
    int width = gr_get_width(icon[2]);
    int height = gr_get_height(icon[2]);

    int dx = gr_fb_width() - (width * 3);
    int dy = gr_fb_height() - height;

    if(icon[2]) {
		gr_color(0, 0, 0, 255);
		gr_fill(dx, dy, width, height);

        width = gr_get_width(icon[2]);
        height = gr_get_height(icon[2]);
        gr_blit(icon[0], 0, 0, width, height, dx, dy);
        dx = dx + width;
        gr_blit(icon[1], 0, 0, width, height, dx, dy);
        dx = dx + width;
        gr_blit(icon[2], 0, 0, width, height, dx, dy);
    }
}

void ScreenRecoveryUI::draw_cur_update_lable_locked(gr_surface icon)
{
    int width = 0;
    int height = 0;
    int dx = 0;
    int dy = 0;

    if(gCurUpdateLableType == RecoveryUI::CUR_UPDATE_LABLE_TYPE_NONE
        || gCurUpdateLableType == RecoveryUI::CUR_UPDATE_LABLE_TYPE_DISAPPEARED) {
        return;
    }

	if (icon) {
		width = gr_get_width(icon);
		height = gr_get_height(icon);

		dx = (gr_fb_width() - width) / 2;
		dy = UPDATE_LABEL_DY; 

		gr_color(0, 0, 0, 255);
		gr_fill(dx, dy, width, height);
        gr_blit(icon, 0, 0, width, height, dx, dy);

		width = gr_get_width(gErrorCodeImg[countPresent]);
		height = gr_get_height(gErrorCodeImg[countPresent]);
		gr_blit(gErrorCodeImg[countPresent], 0, 0, width, height, PROGRESS_LABLE_CURRENT_POSITION_DX, PROGRESS_LABLE_DY);

		width = gr_get_width(gSlash);
		height = gr_get_height(gSlash);
		gr_blit(gSlash, 0, 0, width, height, PROGRESS_LABLE_SLASH_POSITION_DX, PROGRESS_LABLE_DY);

		width = gr_get_width(gErrorCodeImg[countTotal]);
		height = gr_get_height(gErrorCodeImg[countTotal]);
		gr_blit(gErrorCodeImg[countTotal], 0, 0, width, height, PROGRESS_LABLE_TOTAL_POSITION_DX, PROGRESS_LABLE_DY);
	}
}

// Draw the progress bar (if any) on the screen.  Does not flip pages.
// Should only be called with updateMutex locked.
void ScreenRecoveryUI::draw_progress_locked()
{

	int width = gr_get_width(progressBarEmpty);
	int height = gr_get_height(progressBarEmpty);

	int dx = (gr_fb_width() - width)/2;
	int dy = UPDATE_PROGRESS_DY;

    if(progressBarType == EMPTY)
        return ;

	// Erase behind the progress bar (in case this was a progress-only update)
	gr_color(0, 0, 0, 255);
	gr_fill(dx, dy, width, height);

	if (progressBarType == DETERMINATE) {
		float p = progressScopeStart + progress * progressScopeSize;
		int pos = (int) (p * width);
/*
		if (rtl_locale) {
			// Fill the progress bar from right to left.
			if (pos > 0) {
				gr_blit(progressBarEmpty, width-pos, 0, pos, height, dx+width-pos, dy);
				gr_blit(progressBarFill, width-pos, 0, pos, height, dx+width-pos, dy);
			}
			if (pos < width-1) {
				gr_blit(progressBarEmpty, 0, 0, width-pos, height, dx, dy);
			}
		} else */{
			// Fill the progress bar from left to right.
			if (pos > 0) {
				gr_blit(progressBarEmpty, 0, 0, width, height, dx, dy);
				gr_blit(progressBarFill, 0, 0, pos, height, dx, dy);
			}
			if (pos < width-1) {
				gr_blit(progressBarEmpty, pos, 0, width-pos, height, dx+pos, dy);
			}
		}
	}
}

/* jason.kuk 2018.01.09 : Add draw progress function for Delta Update : Star --> */
void ScreenRecoveryUI::ui_draw_progress_locked()
{
	int width = gr_get_width(progressBarEmpty);
	int height = gr_get_height(progressBarEmpty);

	int dx = (gr_fb_width() - width)/2;
	int dy = UPDATE_PROGRESS_DY;

    if(progressBarType == EMPTY)
        return ;

	// Erase behind the progress bar (in case this was a progress-only update)
	gr_color(0, 0, 0, 255);
	gr_fill(dx, dy, width, height);

	if (progressBarType == DETERMINATE) {
		float p = progressScopeStart + progress * progressScopeSize;
		int pos = (int) (p * width);

		// Fill the progress bar from left to right.
		if (pos > 0) {
			gr_blit(progressBarEmpty, width-pos, 0, pos, height, dx+width-pos, dy);
			gr_blit(progressBarFill, width-pos, 0, pos, height, dx+width-pos, dy);
		}
		if (pos < width-1) {
			gr_blit(progressBarEmpty, 0, 0, width-pos, height, dx, dy);
		}
    }
}
/* jason.kuk 2018.01.09 : Add draw progress function for Delta Update : <-- */

void ScreenRecoveryUI::draw_spin_progress_locked()
{
    int width = gr_get_width(gSpinProgressBar);
    int height = gr_get_height(gSpinProgressBar);

    int dx = (gr_fb_width() - width) / 2;
    int dy = SPIN_PROGRESS_DY;

    gr_blit(gSpinProgressBar, 0, 0, width, height, dx, dy);
}


void ScreenRecoveryUI::draw_text_line(int row, const char* t) {
    if (t[0] != '\0') {
        gr_text(0, (row+1)*CHAR_HEIGHT-1, t, 0);
    }
}

void ScreenRecoveryUI::update_versioninfo_display(const char *version) {
    if (version[0] != '\0') {
        gr_text(10, 50+CHAR_HEIGHT, version, 0);
    }
}

void ScreenRecoveryUI::SetColor(UIElement e) {
    switch (e) {
        case HEADER:
            gr_color(247, 0, 6, 255);
            break;
        case MENU:
        case MENU_SEL_BG:
            gr_color(0, 106, 157, 255);
            break;
        case MENU_SEL_FG:
            gr_color(255, 255, 255, 255);
            break;
        case LOG:
            gr_color(249, 194, 0, 255);
            break;
        case TEXT_FILL:
            gr_color(0, 0, 0, 160);
            break;
        default:
            gr_color(255, 255, 255, 255);
            break;
    }
}

// Redraw everything on the screen.  Does not flip pages.
// Should only be called with updateMutex locked.
void ScreenRecoveryUI::draw_screen_locked()
{
    draw_background_locked(currentIcon);
    draw_progress_locked();

    switch(gCurUpdateLableType){
        case CUR_UPDATE_LABLE_TYPE_NORMAL:
            draw_cur_update_lable_locked(gCurUpdateLable);
            break;
        case CUR_UPDATE_LABLE_TYPE_MICOM:
            draw_cur_update_lable_locked(gCurUpdateLable_Micom);
            break;
        case CUR_UPDATE_LABLE_TYPE_MONITOR:
            draw_cur_update_lable_locked(gCurUpdateLable_Monitor);
            break;
        case CUR_UPDATE_LABLE_TYPE_MODEM:
            draw_cur_update_lable_locked(gCurUpdateLable_Modem);
            break;
        case CUR_UPDATE_LABLE_TYPE_GPS:
            draw_cur_update_lable_locked(gCurUpdateLable_GPS);
            break;
        case CUR_UPDATE_LABLE_TYPE_SXM:
            draw_cur_update_lable_locked(gCurUpdateLable_SXM);
            break;
        case CUR_UPDATE_LABLE_TYPE_AMP:
            draw_cur_update_lable_locked(gCurUpdateLable_AMP);
            break;
        case CUR_UPDATE_LABLE_TYPE_HD:
            draw_cur_update_lable_locked(gCurUpdateLable_HD);
            break;
        case CUR_UPDATE_LABLE_TYPE_TOUCH:
            draw_cur_update_lable_locked(gCurUpdateLable_Touch);
            break;
        case CUR_UPDATE_LABLE_TYPE_KEYBOARD:
            draw_cur_update_lable_locked(gCurUpdateLable_Keyboard);
            break;
        case CUR_UPDATE_LABLE_TYPE_CDP:
            draw_cur_update_lable_locked(gCurUpdateLable_CDP);
            break;
        case CUR_UPDATE_LABLE_TYPE_HERO:
            draw_cur_update_lable_locked(gCurUpdateLable_Hero);
            break;
        case CUR_UPDATE_LABLE_TYPE_HIFI2:
            draw_cur_update_lable_locked(gCurUpdateLable_Hifi2);
			break;
        case CUR_UPDATE_LABLE_TYPE_EPICS:
            draw_cur_update_lable_locked(gCurUpdateLable_Epics);
			break;
        case CUR_UPDATE_LABLE_TYPE_DRM:
            draw_cur_update_lable_locked(gCurUpdateLable_Drm);
			break;
        case CUR_UPDATE_LABLE_TYPE_DAB:
            draw_cur_update_lable_locked(gCurUpdateLable_DAB);
            break;
        case CUR_UPDATE_LABLE_TYPE_MAP:
            draw_cur_update_lable_locked(gCurUpdateLable_Map);
            break;
    }

    draw_status_message_locked(gCurrentStatusMessageIcon);
    draw_error_number_locked(gCurrentErrorNumberIcon);

    if (show_text) {

        SetColor(TEXT_FILL);
        gr_fill(0, 0, gr_fb_width(), gr_fb_height());

        int y = 0;
        int i = 0;
        if (show_menu) {
            SetColor(HEADER);

            for (; i < menu_top + menu_items; ++i) {
                if (i == menu_top) SetColor(MENU);

                if (i == menu_top + menu_sel) {
                    // draw the highlight bar
                    SetColor(MENU_SEL_BG);
                    gr_fill(0, y-2, gr_fb_width(), y+char_height+2);
                    // white text of selected item
                    SetColor(MENU_SEL_FG);
                    if (menu[i][0]) gr_text(4, y, menu[i], 1);
                    SetColor(MENU);
                } else {
                    if (menu[i][0]) gr_text(4, y, menu[i], i < menu_top);
                }
                y += char_height+4;
            }
            SetColor(MENU);
            y += 4;
            gr_fill(0, y, gr_fb_width(), y+2);
            y += 4;
            ++i;
        }

        SetColor(LOG);

        // display from the bottom up, until we hit the top of the
        // screen, the bottom of the menu, or we've displayed the
        // entire text buffer.
        int ty;
        int row = (text_top+text_rows-1) % text_rows;
        for (int ty = gr_fb_height() - char_height, count = 0;
             ty > y+2 && count < text_rows;
             ty -= char_height, ++count) {
            gr_text(4, ty, text[row], 0);
            --row;
            if (row < 0) row = text_rows-1;
        }
    }
}

// Redraw everything on the screen and flip the screen (make it visible).
// Should only be called with updateMutex locked.
void ScreenRecoveryUI::update_screen_locked()
{
    draw_screen_locked();
    gr_flip();
}

// Updates only the progress bar, if possible, otherwise redraws the screen.
// Should only be called with updateMutex locked.
void ScreenRecoveryUI::update_progress_locked()
{
    if (show_text || !pagesIdentical) {
        draw_screen_locked();    // Must redraw the whole screen
        pagesIdentical = true;
    } else {
        draw_progress_locked();  // Draw only the progress bar and overlays
    }
    gr_flip();
}

void ScreenRecoveryUI::update_spin_progress_locked()
{
    draw_spin_progress_locked();
    gr_flip();
}

// Keeps the progress bar updated, even when the process is otherwise busy.
void* ScreenRecoveryUI::progress_thread(void *cookie) {
    self->progress_loop();
    return NULL;
}

void ScreenRecoveryUI::progress_loop() {
    double interval = 1.0 / animation_fps;
    int redraw = 0;

    for (;;) {
        double start = now();
        pthread_mutex_lock(&updateMutex);

        redraw = 0;

        // update the installation animation, if active
        // skip this if we have a text overlay (too expensive to update)
        if ((currentIcon == INSTALLING_UPDATE || currentIcon == ERASING) &&
            installing_frames > 0 && !show_text) {
            installingFrame = (installingFrame + 1) % installing_frames;
            redraw = 1;
        }

        // move the progress bar forward on timed intervals, if configured
        int duration = progressScopeDuration;
        if (progressBarType == DETERMINATE && duration > 0) {
            double elapsed = now() - progressScopeTime;
            float p = 1.0 * elapsed / duration;
            if (p > 1.0) p = 1.0;
            if (p > progress) {
                progress = p;
                redraw = 1;
            }
        }

        if(redraw)
            update_progress_locked();

        pthread_mutex_unlock(&updateMutex);
        double end = now();
        // minimum of 20ms delay between frames
        double delay = interval - (end-start);
        if (delay < 0.02) 
			delay = 0.02;

        usleep((long)(delay * 2000000));
    }
}

void *ScreenRecoveryUI::spin_progress_thread(void *cookie) 
{
	self->spin_progress_loop();
	return NULL;
}

void ScreenRecoveryUI::spin_progress_loop() 
{
    for(;;)
    {
        if(spinProgress)
        {
            gSpinProgressBar = spinProgressBar[spinProgressPosition];
            ++spinProgressPosition;

            if(spinProgressPosition > 15)
                spinProgressPosition = 0;

            pthread_mutex_lock(&updateMutex);

            update_spin_progress_locked();

            pthread_mutex_unlock(&updateMutex);
            usleep((long)100000);
        }
        else
        {
            usleep((long)100000);
        }
    }
}

void ScreenRecoveryUI::LoadBitmap(const char* filename, gr_surface* surface) {
    int result = res_create_surface(filename, surface);
    if (result < 0) {
        LOGE("missing bitmap %s\n(Code %d)\n", filename, result);
    }
}

void ScreenRecoveryUI::LoadLocalizedBitmap(const char* filename, gr_surface* surface) {
    int result = res_create_localized_surface(filename, surface, setting_local);
    if (result < 0) {
        LOGE("missing bitmap %s\n(Code %d)\n", filename, result);
    }
}

void ScreenRecoveryUI::Init()
{
    int i;
	char filename[40];

    gr_init();

    gr_font_size(&char_width, &char_height);

    text_col = text_row = 0;
    text_rows = gr_fb_height() / char_height;
    if (max_menu_rows > kMaxMenuRows)
        max_menu_rows = kMaxMenuRows;
    if (text_rows > kMaxRows) text_rows = kMaxRows;
    text_top = 1;

    text_cols = gr_fb_width() / char_width;
    if (text_cols > kMaxCols - 1) text_cols = kMaxCols - 1;

    text_first_row = 7;
    menu_item_start = text_first_row * char_height;
    max_menu_rows = (text_rows - text_first_row) / 3;

    LoadLocalizedBitmap("contactServiceCenter", &gStatusMessageIcon[INSTALL_ERROR_CONTACN_CUSTOMER]);
    LoadLocalizedBitmap("cur_update_lable_touch", &gCurUpdateLable_Touch);

//    LoadBitmap("set_bg_installing", &backgroundIcon[INSTALLING_UPDATE]);

    backgroundIcon[ERASING] = backgroundIcon[INSTALLING_UPDATE];
    LoadBitmap("factory_reset", &backgroundIcon[FACTORY_RESET]);
    LoadBitmap("icon_error", &backgroundIcon[ERROR]);
    backgroundIcon[NO_COMMAND] = backgroundIcon[ERROR];

    LoadLocalizedBitmap("set_etc_lid_2655", &gStatusMessageIcon[INSTALLING_MESSAGE]);
    LoadLocalizedBitmap("set_etc_lid_2661", &gTitleText);
    LoadLocalizedBitmap("set_etc_lid_2659", &gStatusMessageIcon[INSATLL_ERROR_MESSAGE]);
    LoadBitmap("set_etc_lid_2805", &gError_Code_Label);
    LoadLocalizedBitmap("set_etc_lid_3886", &gCurUpdateLable);

    LoadLocalizedBitmap("set_etc_lid_9521", &gCurUpdateLable_SXM);
    LoadLocalizedBitmap("set_etc_lid_9522", &gCurUpdateLable_Modem);
    LoadLocalizedBitmap("set_etc_lid_9523", &gCurUpdateLable_GPS);
    LoadLocalizedBitmap("set_etc_lid_9524", &gCurUpdateLable_Micom);
    LoadLocalizedBitmap("set_etc_lid_9525", &gCurUpdateLable_HD);
    LoadLocalizedBitmap("set_etc_lid_9526", &gCurUpdateLable_AMP);
    LoadLocalizedBitmap("set_etc_lid_9527", &gCurUpdateLable_Monitor);
    LoadLocalizedBitmap("set_etc_lid_11424", &gCurUpdateLable_Keyboard);
    LoadLocalizedBitmap("set_etc_lid_11425", &gCurUpdateLable_CDP);
    LoadLocalizedBitmap("set_etc_lid_11426", &gCurUpdateLable_Hero);
    LoadLocalizedBitmap("set_etc_lid_15941", &gCurUpdateLable_DAB);
    LoadLocalizedBitmap("set_etc_lid_16001", &gCurUpdateLable_Hifi2);
    LoadLocalizedBitmap("set_etc_lid_16002", &gCurUpdateLable_Epics);
    LoadLocalizedBitmap("set_etc_lid_16442", &gCurUpdateLable_Drm);
    LoadLocalizedBitmap("set_etc_lid_50600", &gCurUpdateLable_Map);

    LoadLocalizedBitmap("set_etc_lid_37921", &gStatusMessageIcon[INSTALL_SUCCESS_MESSAGE]);

    LoadBitmap("set_etc_pro_empty", &progressBarEmpty);
    LoadBitmap("set_etc_pro_fill", &progressBarFill);
    LoadBitmap("set_etc_progress_slash", &gSlash);


    if (installing_frames > 0) {
        installationOverlay = (gr_surface*)malloc(installing_frames * sizeof(gr_surface));
        for (i = 0; i < installing_frames; ++i) {
            char filename[40];
            sprintf(filename, "set_etc_progress_%02d", i+1);
            LoadBitmap(filename, installationOverlay+i);
        }
    } else {
        installationOverlay = NULL;
    }
	
	for (i = 0; i < 10; ++i) {
		memset(&filename,0x00,sizeof(char)*40);
		sprintf(filename, "set_etc_progress_%02d", i);
		LoadBitmap(filename, gErrorCodeImg+i);
	}

	for(i = 1;i < 17; ++i)
    {
		memset(&filename,0x00,sizeof(char)*40);
		sprintf(filename, "pho_etc_searchani_%02d", i + 1);
		LoadBitmap(filename, spinProgressBar + i);
	}

    pthread_create(&progress_t, NULL, progress_thread, NULL);
	pthread_create(&spin_progress_t, NULL, spin_progress_thread, NULL);

    RecoveryUI::Init();
}

void ScreenRecoveryUI::SetLocale(const char* locale) {
    if(locale) {
        memset(setting_local, 0, 3);
        strncpy(setting_local, locale, 2);

        // A bit cheesy: keep an explicit list of supported languages
        // that are RTL.
        if (strcmp(locale, "ar") == 0 ||   // Arabic
            strcmp(locale, "fa") == 0 ||   // Persian (Farsi)
            strcmp(locale, "he") == 0 ||   // Hebrew (new language code)
            strcmp(locale, "iw") == 0 ||   // Hebrew (old language code)
            strcmp(locale, "ur") == 0) {   // Urdu
            rtl_locale = true;
        }
    }

    printf("setting_local = [%s]", setting_local);
}

void ScreenRecoveryUI::SetBackground(Icon icon)
{
    pthread_mutex_lock(&updateMutex);

    // Adjust the offset to account for the positioning of the
    // base image on the screen.
    if (backgroundIcon[icon] != NULL) {
        gr_surface bg = backgroundIcon[icon];
        overlay_offset_x = install_overlay_offset_x + (gr_fb_width() - gr_get_width(bg)) / 2;
        overlay_offset_y = install_overlay_offset_y +
            (gr_fb_height() - (gr_get_height(bg) +  40)) / 2;
    }

    currentIcon = icon;
    update_screen_locked();

    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::ui_set_status_message(Messages icon)
{
    pthread_mutex_lock(&updateMutex);
    gCurrentStatusMessageIcon = gStatusMessageIcon[icon];
    if(icon == INSTALL_SUCCESS_MESSAGE) {
        progressBarType = EMPTY;
        gCurUpdateLableType = CUR_UPDATE_LABLE_TYPE_DISAPPEARED;
    } else if(icon == INSATLL_ERROR_MESSAGE || icon == INSTALL_ERROR_CONTACN_CUSTOMER){
        progressBarType = EMPTY;
    }
    update_screen_locked();
//	draw_status_message_locked(gCurrentStatusMessageIcon);
    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::ui_set_error_number(int error)
{
    pthread_mutex_lock(&updateMutex);

    gCurrentErrorNumberIcon[0] = gErrorCodeImg[error / 100];
    gCurrentErrorNumberIcon[1] = gErrorCodeImg[(error % 100) / 10];
    gCurrentErrorNumberIcon[2] = gErrorCodeImg[error % 10];

    update_screen_locked();
//	draw_error_number_locked(gCurrentErrorNumberIcon);

    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::SetProgressType(ProgressType type)
{
    pthread_mutex_lock(&updateMutex);
    if (progressBarType != type) {
        progressBarType = type;
    }
    progressScopeStart = 0;
    progressScopeSize = 0;
    progress = 0;
    update_progress_locked();
    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::ShowProgress(float portion, float seconds)
{
    pthread_mutex_lock(&updateMutex);
    progressBarType = DETERMINATE;
    progressScopeStart += progressScopeSize;
    progressScopeSize = portion;
    progressScopeTime = now();
    progressScopeDuration = seconds;
    progress = 0;
    update_progress_locked();
    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::SetProgress(float fraction)
{
    pthread_mutex_lock(&updateMutex);
    if (fraction < 0.0) 
		fraction = 0.0;
    if (fraction > 1.0) 
		fraction = 1.0;

    if (progressBarType == DETERMINATE && fraction > progress) {
        // Skip updates that aren't visibly different.
        int width = gr_get_width(progressBarEmpty);
        float scale = width * progressScopeSize;
        if ((int) (progress * scale) != (int) (fraction * scale)) {
            progress = fraction;
            update_progress_locked();
        }
    }
    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::ShowSpinProgress(bool show)
{
    printf("ShowSpinProgress() start\n");
    pthread_mutex_lock(&updateMutex);

    spinProgress = show;
	spinProgressPosition = 0;

    pthread_mutex_unlock(&updateMutex);
}

/* jason.ku 2018.0108 : Shown Progress for Delta update : Start --> */
void ScreenRecoveryUI::RB_UI_Progress(float fraction)
{
    pthread_mutex_lock(&updateMutex);
    progressBarType = DETERMINATE;
    progressScopeStart += progressScopeSize;
    progressScopeSize = fraction;
    progress = 0;
    ui_draw_progress_locked();
    pthread_mutex_unlock(&updateMutex);
}
/* jason.ku 2018.0108 : Shown Progress for Delta update : <-- End */

extern int print_enable;
void ScreenRecoveryUI::Print(const char *fmt, ...)
{
	char buf[256];

	if(print_enable == 1){
	    va_list ap;
	    va_start(ap, fmt);
	    vsnprintf(buf, 256, fmt, ap);
	    va_end(ap);

	    fputs(buf, stdout);

	    // This can get called before ui_init(), so be careful.
	    pthread_mutex_lock(&updateMutex);
	    if (text_rows > 0 && text_cols > 0) {
	        char *ptr;
	        for (ptr = buf; *ptr != '\0'; ++ptr) {
	            if (*ptr == '\n' || text_col >= text_cols) {
	                text[text_row][text_col] = '\0';
	                text_col = 0;
	                text_row = (text_row + 1) % text_rows;
	                if (text_row == text_top) text_top = (text_top + 1) % text_rows;
	            }
	            if (*ptr != '\n') text[text_row][text_col++] = *ptr;
	        }
	        text[text_row][text_col] = '\0';
	        update_screen_locked();
	    }
	    pthread_mutex_unlock(&updateMutex);
	}
}

void ScreenRecoveryUI::version_print(const char *fmt, ...)
{
    char buf[MAX_COLS];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, MAX_COLS, fmt, ap);
    va_end(ap);

    fputs(buf, stdout);
    strcpy(version_info_text,buf);

    // This can get called before ui_init(), so be careful.
    pthread_mutex_lock(&updateMutex);
    update_screen_locked();
    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::StartMenu(const char* const * headers, const char* const * items,
                                 int initial_selection) {
    int i = 0;
    pthread_mutex_lock(&updateMutex);
    if (text_rows > 0 && text_cols > 0) {
        for (i = 0; i < text_rows; ++i) {
            if (headers[i] == NULL) break;
            strncpy(menu[i], headers[i], text_cols-1);
            menu[i][text_cols-1] = '\0';
        }
        menu_top = i;
        for (; i < kMaxMenuRows; ++i) {
            if (items[i-menu_top] == NULL) break;
            strncpy(menu[i], items[i-menu_top], text_cols-1);
            menu[i][text_cols-1] = '\0';
        }
        menu_items = i - menu_top;
        show_menu = 1;
        menu_sel = initial_selection;
        if (menu_show_start <= menu_sel - max_menu_rows ||
                menu_show_start > menu_sel) {
            menu_show_start = menu_sel;
        }
        update_screen_locked();
    }
    pthread_mutex_unlock(&updateMutex);
}

int ScreenRecoveryUI::SelectMenu(int sel, bool abs) {
    int old_sel;
    pthread_mutex_lock(&updateMutex);
    if (abs) {
        sel += menu_show_start;
    }
    if (show_menu > 0) {
        old_sel = menu_sel;
        menu_sel = sel;
        if (menu_sel < 0) menu_sel = menu_items + menu_sel;
        if (menu_sel >= menu_items) menu_sel = menu_sel - menu_items;
        if (menu_sel < menu_show_start && menu_show_start > 0) {
            menu_show_start = menu_sel;
        }
        if (menu_sel - menu_show_start >= max_menu_rows) {
            menu_show_start = menu_sel - max_menu_rows + 1;
        }
        sel = menu_sel;
        if (menu_sel != old_sel) update_screen_locked();
    }
    pthread_mutex_unlock(&updateMutex);
    return sel;
}

void ScreenRecoveryUI::EndMenu() {
    int i;
    pthread_mutex_lock(&updateMutex);
    if (show_menu > 0 && text_rows > 0 && text_cols > 0) {
        show_menu = 0;
        update_screen_locked();
    }
    pthread_mutex_unlock(&updateMutex);
}

bool ScreenRecoveryUI::IsTextVisible()
{
    pthread_mutex_lock(&updateMutex);
    int visible = show_text;
    pthread_mutex_unlock(&updateMutex);
    return visible;
}

bool ScreenRecoveryUI::WasTextEverVisible()
{
    pthread_mutex_lock(&updateMutex);
    int ever_visible = show_text_ever;
    pthread_mutex_unlock(&updateMutex);
    return ever_visible;
}

void ScreenRecoveryUI::ShowText(bool visible)
{
    pthread_mutex_lock(&updateMutex);
    show_text = visible;
    if (show_text) show_text_ever = 1;
    update_screen_locked();
    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::Redraw()
{
    pthread_mutex_lock(&updateMutex);
    update_screen_locked();
    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::ShowSystemIcon(bool visible)
{
    pthread_mutex_lock(&updateMutex);
    show_system_icon = visible;
    pthread_mutex_unlock(&updateMutex);
}

void ScreenRecoveryUI::draw_factory_reset() {
    pthread_mutex_lock(&updateMutex);

    gr_color(0, 0, 0, 255);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    int iconWidth = gr_get_width(gFactoryReset);
    int iconHeight = gr_get_height(gFactoryReset);
    gr_blit(gFactoryReset, 0, 0, iconWidth, iconHeight, 0, 0);

    gr_flip();

    pthread_mutex_unlock(&updateMutex);
}

void* ScreenRecoveryUI::factory_reset_thread(void *cookie) {
    for (;;) {
        usleep(1000000 / PROGRESSBAR_INDETERMINATE_FPS);
        self->draw_factory_reset();
    }
    return NULL;
}

void ScreenRecoveryUI::ui_factory_reset() {
    gr_init();
    int ret = res_create_surface("factory_reset", &gFactoryReset);
    if (ret < 0) {
        LOGE("Missing bitmap factory_reset\n(Code %d)", ret);
        return ;
    }

    pthread_t t;
    pthread_create(&t, NULL, factory_reset_thread, NULL);
}

void ScreenRecoveryUI::SetUpdateLableType(int mtype){
    switch(mtype){
        case CUR_UPDATE_LABLE_TYPE_MICOM_ERROR:
            gErrorCodeNum[0]=1;gErrorCodeNum[1]=4;gErrorCodeNum[2]=0;gErrorCodeNum[3]=1;
            break;
        case CUR_UPDATE_LABLE_TYPE_MODEM_ERROR:
            gErrorCodeNum[0]=6;gErrorCodeNum[1]=0;gErrorCodeNum[2]=1;gErrorCodeNum[3]=-1;
            break;
        case CUR_UPDATE_LABLE_TYPE_GPS_ERROR:
            gErrorCodeNum[0]=7;gErrorCodeNum[1]=0;gErrorCodeNum[2]=1;gErrorCodeNum[3]=-1;
            break;
        case CUR_UPDATE_LABLE_TYPE_SXM_ERROR:
            gErrorCodeNum[0]=1;gErrorCodeNum[1]=0;gErrorCodeNum[2]=1;gErrorCodeNum[3]=-1;
            break;
        case CUR_UPDATE_LABLE_TYPE_AMP_ERROR:
            gErrorCodeNum[0]=1;gErrorCodeNum[1]=2;gErrorCodeNum[2]=0;gErrorCodeNum[3]=1;
            break;
        case CUR_UPDATE_LABLE_TYPE_HD_ERROR:
            gErrorCodeNum[0]=1;gErrorCodeNum[1]=1;gErrorCodeNum[2]=0;gErrorCodeNum[3]=1;
            break;
        case CUR_UPDATE_LABLE_TYPE_MONITOR_ERROR:
            gErrorCodeNum[0]=1;gErrorCodeNum[1]=0;gErrorCodeNum[2]=0;gErrorCodeNum[3]=1;
            break;
        case CUR_UPDATE_LABLE_TYPE_TOUCH_ERROR:
            gErrorCodeNum[0]=9;gErrorCodeNum[1]=0;gErrorCodeNum[2]=1;gErrorCodeNum[3]=-1;
            break;
        case CUR_UPDATE_LABLE_TYPE_CONTACT_CUSTOMER:
            gErrorCodeNum[0]=1;gErrorCodeNum[1]=8;gErrorCodeNum[2]=1;gErrorCodeNum[3]=1;
            break;
        case CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR:
            gErrorCodeNum[0]=3;gErrorCodeNum[1]=0;gErrorCodeNum[2]=1;gErrorCodeNum[3]=-1;
            break;
    }
    gCurUpdateLableType = mtype;
    progressScopeStart = 0.0;
    progress = 0.0;
    progressScopeSize = 0.0;
    pagesIdentical = false;
}

