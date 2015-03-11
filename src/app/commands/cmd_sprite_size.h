// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifndef APP_COMMANDS_CMD_SPRITE_SIZE_H_INCLUDED
#define APP_COMMANDS_CMD_SPRITE_SIZE_H_INCLUDED
#pragma once

#include "app/commands/command.h"
#include "doc/algorithm/resize_image.h"

#include <string>

namespace ui {
  class CheckBox;
  class Entry;
}

namespace app {

  class SpriteSizeCommand : public Command {
  public:
    SpriteSizeCommand();
    Command* clone() const override;

    void setScale(double x, double y) {
      m_scaleX = x;
      m_scaleY = y;
    }

  protected:
    virtual void onLoadParams(const Params& params) override;
    virtual bool onEnabled(Context* context) override;
    virtual void onExecute(Context* context) override;

  private:
    void onLockRatioClick();
    void onWidthPxChange();
    void onHeightPxChange();
    void onWidthPercChange();
    void onHeightPercChange();

    ui::CheckBox* m_lockRatio;
    ui::Entry* m_widthPx;
    ui::Entry* m_heightPx;
    ui::Entry* m_widthPerc;
    ui::Entry* m_heightPerc;

    int m_width;
    int m_height;
    double m_scaleX;
    double m_scaleY;
    doc::algorithm::ResizeMethod m_resizeMethod;
  };

} // namespace app

#endif
