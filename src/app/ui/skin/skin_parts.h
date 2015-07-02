// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifndef APP_UI_SKIN_SKIN_PARTS_H_INCLUDED
#define APP_UI_SKIN_SKIN_PARTS_H_INCLUDED
#pragma once

namespace app {
  namespace skin {

#define SKIN_PART_NESW(name) \
      name##_NW,             \
      name##_N,              \
      name##_NE,             \
      name##_E,              \
      name##_SE,             \
      name##_S,              \
      name##_SW,             \
      name##_W

    // TODO remove this enum, we use skin::Style now.
    // Available parts in the skin sheet
    enum SkinParts {

      PART_NONE,

      PART_RADIO_NORMAL,
      PART_RADIO_SELECTED,
      PART_RADIO_DISABLED,

      PART_CHECK_NORMAL,
      PART_CHECK_SELECTED,
      PART_CHECK_DISABLED,

      SKIN_PART_NESW(PART_CHECK_FOCUS),
      SKIN_PART_NESW(PART_RADIO_FOCUS),
      SKIN_PART_NESW(PART_BUTTON_NORMAL),
      SKIN_PART_NESW(PART_BUTTON_HOT),
      SKIN_PART_NESW(PART_BUTTON_FOCUSED),
      SKIN_PART_NESW(PART_BUTTON_SELECTED),
      SKIN_PART_NESW(PART_SUNKEN_NORMAL),
      SKIN_PART_NESW(PART_SUNKEN_FOCUSED),
      SKIN_PART_NESW(PART_SUNKEN2_NORMAL),
      SKIN_PART_NESW(PART_SUNKEN2_FOCUSED),
      SKIN_PART_NESW(PART_SUNKEN_MINI_NORMAL),
      SKIN_PART_NESW(PART_SUNKEN_MINI_FOCUSED),

      PART_WINDOW_CLOSE_BUTTON_NORMAL,
      PART_WINDOW_CLOSE_BUTTON_HOT,
      PART_WINDOW_CLOSE_BUTTON_SELECTED,

      PART_WINDOW_PLAY_BUTTON_NORMAL,
      PART_WINDOW_PLAY_BUTTON_HOT,
      PART_WINDOW_PLAY_BUTTON_SELECTED,

      PART_WINDOW_STOP_BUTTON_NORMAL,
      PART_WINDOW_STOP_BUTTON_HOT,
      PART_WINDOW_STOP_BUTTON_SELECTED,

      PART_WINDOW_CENTER_BUTTON_NORMAL,
      PART_WINDOW_CENTER_BUTTON_HOT,
      PART_WINDOW_CENTER_BUTTON_SELECTED,

      SKIN_PART_NESW(PART_SLIDER_FULL),
      SKIN_PART_NESW(PART_SLIDER_EMPTY),
      SKIN_PART_NESW(PART_SLIDER_FULL_FOCUSED),
      SKIN_PART_NESW(PART_SLIDER_EMPTY_FOCUSED),
      SKIN_PART_NESW(PART_MINI_SLIDER_FULL),
      SKIN_PART_NESW(PART_MINI_SLIDER_EMPTY),
      SKIN_PART_NESW(PART_MINI_SLIDER_FULL_FOCUSED),
      SKIN_PART_NESW(PART_MINI_SLIDER_EMPTY_FOCUSED),

      PART_MINI_SLIDER_THUMB,
      PART_MINI_SLIDER_THUMB_FOCUSED,

      PART_SEPARATOR_HORZ,
      PART_SEPARATOR_VERT,

      PART_COMBOBOX_ARROW_DOWN,
      PART_COMBOBOX_ARROW_DOWN_SELECTED,
      PART_COMBOBOX_ARROW_DOWN_DISABLED,
      PART_COMBOBOX_ARROW_UP,
      PART_COMBOBOX_ARROW_UP_SELECTED,
      PART_COMBOBOX_ARROW_UP_DISABLED,
      PART_COMBOBOX_ARROW_LEFT,
      PART_COMBOBOX_ARROW_LEFT_SELECTED,
      PART_COMBOBOX_ARROW_LEFT_DISABLED,
      PART_COMBOBOX_ARROW_RIGHT,
      PART_COMBOBOX_ARROW_RIGHT_SELECTED,
      PART_COMBOBOX_ARROW_RIGHT_DISABLED,

      PART_NEWFOLDER,
      PART_NEWFOLDER_SELECTED,

      SKIN_PART_NESW(PART_TOOLBUTTON_NORMAL),
      SKIN_PART_NESW(PART_TOOLBUTTON_HOT),
      SKIN_PART_NESW(PART_TOOLBUTTON_LAST),
      SKIN_PART_NESW(PART_TOOLBUTTON_PUSHED),

      SKIN_PART_NESW(PART_EDITOR_NORMAL),
      SKIN_PART_NESW(PART_EDITOR_SELECTED),

      SKIN_PART_NESW(PART_COLORBAR_0),
      SKIN_PART_NESW(PART_COLORBAR_1),
      SKIN_PART_NESW(PART_COLORBAR_2),
      SKIN_PART_NESW(PART_COLORBAR_3),
      SKIN_PART_NESW(PART_COLORBAR_BORDER_FG),
      SKIN_PART_NESW(PART_COLORBAR_BORDER_BG),
      SKIN_PART_NESW(PART_COLORBAR_BORDER_HOTFG),
      SKIN_PART_NESW(PART_TOOLTIP),
      SKIN_PART_NESW(PART_TOOLTIP_ARROW),

      PART_ANI_FIRST,
      PART_ANI_PREVIOUS,
      PART_ANI_PLAY,
      PART_ANI_STOP,
      PART_ANI_NEXT,
      PART_ANI_LAST,

      PART_PAL_EDIT,
      PART_PAL_SORT,
      PART_PAL_PRESETS,
      PART_PAL_OPTIONS,

      PART_TARGET_ONE,
      PART_TARGET_ONE_SELECTED,
      PART_TARGET_FRAMES,
      PART_TARGET_FRAMES_SELECTED,
      PART_TARGET_LAYERS,
      PART_TARGET_LAYERS_SELECTED,
      PART_TARGET_FRAMES_LAYERS,
      PART_TARGET_FRAMES_LAYERS_SELECTED,

      PART_SCALE_ARROW_1,
      PART_SCALE_ARROW_2,
      PART_SCALE_ARROW_3,
      PART_ROTATE_ARROW_1,
      PART_ROTATE_ARROW_2,
      PART_ROTATE_ARROW_3,

      PART_SELECTION_REPLACE,
      PART_SELECTION_REPLACE_SELECTED,
      PART_SELECTION_ADD,
      PART_SELECTION_ADD_SELECTED,
      PART_SELECTION_SUBTRACT,
      PART_SELECTION_SUBTRACT_SELECTED,

      PART_UNPINNED,
      PART_PINNED,

      SKIN_PART_NESW(PART_DROP_DOWN_BUTTON_LEFT_NORMAL),
      SKIN_PART_NESW(PART_DROP_DOWN_BUTTON_LEFT_HOT),
      SKIN_PART_NESW(PART_DROP_DOWN_BUTTON_LEFT_FOCUSED),
      SKIN_PART_NESW(PART_DROP_DOWN_BUTTON_LEFT_SELECTED),
      SKIN_PART_NESW(PART_DROP_DOWN_BUTTON_RIGHT_NORMAL),
      SKIN_PART_NESW(PART_DROP_DOWN_BUTTON_RIGHT_HOT),
      SKIN_PART_NESW(PART_DROP_DOWN_BUTTON_RIGHT_FOCUSED),
      SKIN_PART_NESW(PART_DROP_DOWN_BUTTON_RIGHT_SELECTED),

      PART_TRANSFORMATION_HANDLE,
      PART_PIVOT_HANDLE,

      PART_DROP_PIXELS_OK,
      PART_DROP_PIXELS_OK_SELECTED,
      PART_DROP_PIXELS_CANCEL,
      PART_DROP_PIXELS_CANCEL_SELECTED,

      PART_FREEHAND_ALGO_DEFAULT,
      PART_FREEHAND_ALGO_DEFAULT_SELECTED,
      PART_FREEHAND_ALGO_PIXEL_PERFECT,
      PART_FREEHAND_ALGO_PIXEL_PERFECT_SELECTED,
      PART_FREEHAND_ALGO_DOTS,
      PART_FREEHAND_ALGO_DOTS_SELECTED,

      PART_INK_DEFAULT,
      PART_INK_COMPOSITE,
      PART_INK_LOCK_ALPHA,

      PARTS
    };

  } // namespace skin
} // namespace app

#endif
