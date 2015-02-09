/* Aseprite
 * Copyright (C) 2001-2015  David Capello
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/cmd/set_cel_data.h"

#include "doc/cel.h"
#include "doc/image.h"
#include "doc/image_io.h"
#include "doc/image_ref.h"
#include "doc/sprite.h"
#include "doc/subobjects_io.h"

namespace app {
namespace cmd {

using namespace doc;

SetCelData::SetCelData(Cel* cel, const CelDataRef& newData)
  : WithCel(cel)
  , m_oldDataId(cel->data()->id())
  , m_oldImageId(cel->image()->id())
  , m_newDataId(newData->id())
  , m_newData(newData)
{
}

void SetCelData::onExecute()
{
  Cel* cel = this->cel();
  if (!cel->links())
    createCopy();

  cel->setDataRef(m_newData);
  m_newData.reset(nullptr);
}

void SetCelData::onUndo()
{
  Cel* cel = this->cel();

  if (m_dataCopy) {
    ASSERT(!cel->sprite()->getCelDataRef(m_oldDataId));
    m_dataCopy->setId(m_oldDataId);
    m_dataCopy->image()->setId(m_oldImageId);

    cel->setDataRef(m_dataCopy);
    m_dataCopy.reset(nullptr);
  }
  else {
    CelDataRef oldData = cel->sprite()->getCelDataRef(m_oldDataId);
    ASSERT(oldData);
    cel->setDataRef(oldData);
  }
}

void SetCelData::onRedo()
{
  Cel* cel = this->cel();
  if (!cel->links())
    createCopy();

  CelDataRef newData = cel->sprite()->getCelDataRef(m_newDataId);
  ASSERT(newData);
  cel->setDataRef(newData);
}

void SetCelData::createCopy()
{
  Cel* cel = this->cel();

  ASSERT(!m_dataCopy);
  m_dataCopy.reset(new CelData(*cel->data()));
  m_dataCopy->setImage(ImageRef(Image::createCopy(cel->image())));
}

} // namespace cmd
} // namespace app
