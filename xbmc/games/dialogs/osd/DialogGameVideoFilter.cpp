/*
 *      Copyright (C) 2017 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "DialogGameVideoFilter.h"
#include "cores/RetroPlayer/guibridge/GUIGameVideoHandle.h"
#include "cores/RetroPlayer/rendering/RenderVideoSettings.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/WindowIDs.h"
#include "settings/GameSettings.h"
#include "settings/MediaSettings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/XBMCTinyXML.h"
#include "utils/XMLUtils.h"
#include "URL.h"

#include <stdlib.h>
#include "ServiceBroker.h"
#include "games/GameServices.h"
#include "cores/RetroPlayer/rendering/VideoShaders/VideoShaderPresetFactory.h"

using namespace KODI;
using namespace GAME;

#define SHADER_MANIFEST_PATH  "special://xbmc/system/shaders/presets/shader-manifest.xml"

namespace
{
  struct ScalingMethodProperties
  {
    int nameIndex;
    int categoryIndex;
    int descriptionIndex;
    RETRO::SCALINGMETHOD scalingMethod;
  };

  const std::vector<ScalingMethodProperties> scalingMethods =
  {
    { 16301, 16296, 16298, RETRO::SCALINGMETHOD::NEAREST },
    { 16302, 16297, 16299, RETRO::SCALINGMETHOD::LINEAR },
  };
}

CDialogGameVideoFilter::CDialogGameVideoFilter() :
  CDialogGameVideoSelect(WINDOW_DIALOG_GAME_VIDEO_FILTER)
{
}

std::string CDialogGameVideoFilter::GetHeading()
{
  return g_localizeStrings.Get(35225); // "Video filter"
}

void CDialogGameVideoFilter::PreInit()
{
  m_items.Clear();

  InitVideoFilters();

  if (m_items.Size() == 0)
  {
    CFileItemPtr item = std::make_shared<CFileItem>(g_localizeStrings.Get(231)); // "None"
    m_items.Add(std::move(item));
  }

  m_bHasDescription = false;
}

void CDialogGameVideoFilter::InitVideoFilters()
{
  // Add scaling methods
  if (m_gameVideoHandle)
  {
    for (const auto &scalingMethodProps : scalingMethods)
    {
      if (m_gameVideoHandle->SupportsScalingMethod(scalingMethodProps.scalingMethod))
      {
        RETRO::CRenderVideoSettings videoSettings;
        videoSettings.SetScalingMethod(scalingMethodProps.scalingMethod);

        CFileItemPtr item = std::make_shared<CFileItem>(g_localizeStrings.Get(scalingMethodProps.nameIndex));
        item->SetLabel2(g_localizeStrings.Get(scalingMethodProps.categoryIndex));
        item->SetProperty("game.videofilter", CVariant{ videoSettings.GetVideoFilter() });
        item->SetProperty("game.videofilterdescription", CVariant{ g_localizeStrings.Get(scalingMethodProps.descriptionIndex) });
        m_items.Add(std::move(item));
      }
    }
  }

  // Add shader presets
  std::vector<ShaderPresetProperties> shaderPresets;

  static const std::string xmlPath = SHADER_MANIFEST_PATH;
  const std::string basePath = URIUtils::GetBasePath(xmlPath);

  CXBMCTinyXML xml = CXBMCTinyXML(xmlPath);

  if (!xml.LoadFile())
  {
    CLog::Log(LOGERROR, "%s - Couldn't load shader presets: %s", __FUNCTION__, xml.ErrorDesc());
    return;
  }

  auto root = xml.RootElement();
  TiXmlNode* child = nullptr;

  while ((child = root->IterateChildren(child)))
  {
    ShaderPresetProperties shaderPreset;

    TiXmlNode* pathNode;
    if ((pathNode = child->FirstChild("path")))
      if ((pathNode = pathNode->FirstChild()))
        shaderPreset.path = URIUtils::AddFileToFolder(basePath, pathNode->Value());
    TiXmlNode* nameIndexNode;
    if ((nameIndexNode = child->FirstChild("name")))
      if ((nameIndexNode = nameIndexNode->FirstChild()))
        shaderPreset.nameIndex = atoi(nameIndexNode->Value());
    TiXmlNode* categoryIndexNode;
    if ((categoryIndexNode = child->FirstChild("category")))
      if ((categoryIndexNode = categoryIndexNode->FirstChild()))
        shaderPreset.categoryIndex = atoi(categoryIndexNode->Value());
    TiXmlNode* descriptionNode;
    if ((descriptionNode = child->FirstChild("description")))
      if ((descriptionNode = descriptionNode->FirstChild()))
        shaderPreset.descriptionIndex = atoi(descriptionNode->Value());

    shaderPresets.emplace_back(shaderPreset);
  }

  CLog::Log(LOGDEBUG, "Loaded %d shader presets from default .xml, %s", shaderPresets.size(), CURL::GetRedacted(xmlPath).c_str());

  for (const auto &shaderPreset : shaderPresets)
  {
    bool canLoadPreset = CServiceBroker::GetGameServices().VideoShaders().CanLoadPreset(shaderPreset.path);

    if (!canLoadPreset)
      continue;

    auto localizedName = GetLocalizedString(shaderPreset.nameIndex);
    auto localizedCategory = GetLocalizedString(shaderPreset.categoryIndex);
    auto localizedDescription = GetLocalizedString(shaderPreset.descriptionIndex);

    CFileItemPtr item = std::make_shared<CFileItem>(localizedName);
    item->SetLabel2(localizedCategory);
    item->SetProperty("game.videofilter", CVariant{ shaderPreset.path });
    item->SetProperty("game.videofilterdescription", CVariant{ localizedDescription });

    m_items.Add(std::move(item));
  }
}

void CDialogGameVideoFilter::GetItems(CFileItemList &items)
{
  for (const auto &item : m_items)
    items.Add(item);
}

void CDialogGameVideoFilter::OnItemFocus(unsigned int index)
{
  if (static_cast<int>(index) < m_items.Size())
  {
    CFileItemPtr item = m_items[index];

    std::string videoFilter;
    std::string description;
    GetProperties(*item, videoFilter, description);

    ::CGameSettings &gameSettings = CMediaSettings::GetInstance().GetCurrentGameSettings();

    if (gameSettings.VideoFilter() != videoFilter)
    {
      gameSettings.SetVideoFilter(videoFilter);
      gameSettings.NotifyObservers(ObservableMessageSettingsChanged);

      OnDescriptionChange(description);
      m_bHasDescription = true;
    }
    else if (!m_bHasDescription)
    {
      OnDescriptionChange(description);
      m_bHasDescription = true;
    }
  }
}

unsigned int CDialogGameVideoFilter::GetFocusedItem() const
{
  ::CGameSettings &gameSettings = CMediaSettings::GetInstance().GetCurrentGameSettings();

  for (int i = 0; i < m_items.Size(); i++)
  {
    std::string videoFilter;
    std::string description;
    GetProperties(*m_items[i], videoFilter, description);

    if (videoFilter == gameSettings.VideoFilter())
    {
      return i;
    }
  }

  return 0;
}

void CDialogGameVideoFilter::PostExit()
{
  m_items.Clear();
}

bool CDialogGameVideoFilter::IsCompatible(const TiXmlNode* presetNode)
{
  const TiXmlElement* presetElement = presetNode->ToElement();
  if (presetElement == nullptr)
    return false;

  //! @todo Move this to RetroPlayer
  enum class SHADER_LANGUAGE
  {
    GLSL,
    HLSL,
  };

  //! @todo Get this from RetroPlayer
#if defined(TARGET_WINDOWS)
  SHADER_LANGUAGE type = SHADER_LANGUAGE::HLSL;
#else
  SHADER_LANGUAGE type = SHADER_LANGUAGE::GLSL;
#endif

  std::string strType;
  switch (type)
  {
  case SHADER_LANGUAGE::GLSL:
    strType = "glsl";
    break;
  case SHADER_LANGUAGE::HLSL:
    strType = "hlsl";
    break;
  default:
    break;
  }

  if (strType.empty())
    return false;

  if (XMLUtils::GetAttribute(presetElement, "type") != strType)
    return false;

  return true;
}

std::string CDialogGameVideoFilter::GetLocalizedString(uint32_t code)
{
  return g_localizeStrings.Get(code);
}

void CDialogGameVideoFilter::GetProperties(const CFileItem &item, std::string &videoFilter, std::string &description)
{
  videoFilter = item.GetProperty("game.videofilter").asString();
  description = item.GetProperty("game.videofilterdescription").asString();
}
