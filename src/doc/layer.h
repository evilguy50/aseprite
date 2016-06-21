// Aseprite Document Library
// Copyright (c) 2001-2016 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef DOC_LAYER_H_INCLUDED
#define DOC_LAYER_H_INCLUDED
#pragma once

#include "doc/blend_mode.h"
#include "doc/cel_list.h"
#include "doc/frame.h"
#include "doc/layer_list.h"
#include "doc/object.h"
#include "doc/with_user_data.h"

#include <string>

namespace doc {

  class Cel;
  class Image;
  class Sprite;
  class Layer;
  class LayerImage;
  class LayerGroup;

  //////////////////////////////////////////////////////////////////////
  // Layer class

  enum class LayerFlags {
    None       = 0,
    Visible    = 1,             // Can be read
    Editable   = 2,             // Can be written
    LockMove   = 4,             // Cannot be moved
    Background = 8,             // Stack order cannot be changed
    Continuous = 16,            // Prefer to link cels when the user copy them
    Collapsed  = 32,            // Prefer to show this group layer collapsed

    BackgroundLayerFlags = LockMove | Background,
  };

  class Layer : public WithUserData {
  protected:
    Layer(ObjectType type, Sprite* sprite);

  public:
    virtual ~Layer();

    virtual int getMemSize() const override;

    std::string name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    Sprite* sprite() const { return m_sprite; }
    LayerGroup* parent() const { return m_parent; }
    void setParent(LayerGroup* group) { m_parent = group; }

    // Gets the previous sibling of this layer.
    Layer* getPrevious() const;
    Layer* getNext() const;

    Layer* getPreviousInWholeHierarchy() const;
    Layer* getNextInWholeHierarchy() const;

    bool isImage() const { return type() == ObjectType::LayerImage; }
    bool isGroup() const { return type() == ObjectType::LayerGroup; }
    virtual bool isBrowsable() const { return false; }

    bool isBackground() const  { return hasFlags(LayerFlags::Background); }
    bool isTransparent() const { return !hasFlags(LayerFlags::Background); }
    bool isVisible() const     { return hasFlags(LayerFlags::Visible); }
    bool isEditable() const    { return hasFlags(LayerFlags::Editable); }
    bool isMovable() const     { return !hasFlags(LayerFlags::LockMove); }
    bool isContinuous() const  { return hasFlags(LayerFlags::Continuous); }
    bool isCollapsed() const   { return hasFlags(LayerFlags::Collapsed); }
    bool isExpanded() const    { return !hasFlags(LayerFlags::Collapsed); }

    bool isVisibleHierarchy() const;
    bool isEditableHierarchy() const;

    void setBackground(bool state) { switchFlags(LayerFlags::Background, state); }
    void setVisible   (bool state) { switchFlags(LayerFlags::Visible, state); }
    void setEditable  (bool state) { switchFlags(LayerFlags::Editable, state); }
    void setMovable   (bool state) { switchFlags(LayerFlags::LockMove, !state); }
    void setContinuous(bool state) { switchFlags(LayerFlags::Continuous, state); }
    void setCollapsed (bool state) { switchFlags(LayerFlags::Collapsed, state); }

    LayerFlags flags() const {
      return m_flags;
    }

    bool hasFlags(LayerFlags flags) const {
      return (int(m_flags) & int(flags)) == int(flags);
    }

    void setFlags(LayerFlags flags) {
      m_flags = flags;
    }

    void switchFlags(LayerFlags flags, bool state) {
      if (state)
        m_flags = LayerFlags(int(m_flags) | int(flags));
      else
        m_flags = LayerFlags(int(m_flags) & ~int(flags));
    }

    virtual Cel* cel(frame_t frame) const;
    virtual void getCels(CelList& cels) const = 0;
    virtual void displaceFrames(frame_t fromThis, frame_t delta) = 0;

  private:
    std::string m_name;           // layer name
    Sprite* m_sprite;             // owner of the layer
    LayerGroup* m_parent;        // parent layer
    LayerFlags m_flags;           // stack order cannot be changed

    // Disable assigment
    Layer& operator=(const Layer& other);
  };

  //////////////////////////////////////////////////////////////////////
  // LayerImage class

  class LayerImage : public Layer {
  public:
    explicit LayerImage(Sprite* sprite);
    virtual ~LayerImage();

    virtual int getMemSize() const override;

    BlendMode blendMode() const { return m_blendmode; }
    void setBlendMode(BlendMode blendmode) { m_blendmode = blendmode; }

    int opacity() const { return m_opacity; }
    void setOpacity(int opacity) { m_opacity = opacity; }

    void addCel(Cel *cel);
    void removeCel(Cel *cel);
    void moveCel(Cel *cel, frame_t frame);

    Cel* cel(frame_t frame) const override;
    void getCels(CelList& cels) const override;
    void displaceFrames(frame_t fromThis, frame_t delta) override;

    Cel* getLastCel() const;
    CelConstIterator findCelIterator(frame_t frame) const;
    CelIterator findCelIterator(frame_t frame);
    CelIterator findFirstCelIteratorAfter(frame_t firstAfterFrame);

    void configureAsBackground();

    CelIterator getCelBegin() { return m_cels.begin(); }
    CelIterator getCelEnd() { return m_cels.end(); }
    CelConstIterator getCelBegin() const { return m_cels.begin(); }
    CelConstIterator getCelEnd() const { return m_cels.end(); }
    int getCelsCount() const { return (int)m_cels.size(); }

  private:
    void destroyAllCels();

    BlendMode m_blendmode;
    int m_opacity;
    CelList m_cels;   // List of all cels inside this layer used by frames.
  };

  //////////////////////////////////////////////////////////////////////
  // LayerGroup class

  class LayerGroup : public Layer {
  public:
    explicit LayerGroup(Sprite* sprite);
    virtual ~LayerGroup();

    virtual int getMemSize() const override;

    const LayerList& layers() const { return m_layers; }
    int layersCount() const { return (int)m_layers.size(); }

    void addLayer(Layer* layer);
    void removeLayer(Layer* layer);
    void stackLayer(Layer* layer, Layer* after);

    Layer* firstLayer() const { return (m_layers.empty() ? nullptr: m_layers.front()); }
    Layer* lastLayer() const { return (m_layers.empty() ? nullptr: m_layers.back()); }

    void getCels(CelList& cels) const override;
    void displaceFrames(frame_t fromThis, frame_t delta) override;

    bool isBrowsable() const override {
      return isGroup() && isExpanded() && !m_layers.empty();
    }

  private:
    void destroyAllLayers();

    LayerList m_layers;
  };

} // namespace doc

#endif
