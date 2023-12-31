/*
 *  Copyright (C) 2013-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MediaSourceSettings.h"

#include "ServiceBroker.h"
#include "URL.h"
#include "Util.h"
#include "media/MediaLockState.h"
#include "network/WakeOnAccess.h"
#include "profiles/ProfileManager.h"
#include "settings/SettingsComponent.h"
#include "utils/FileUtils.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML.h"
#include "utils/XMLUtils.h"
#include "utils/log.h"

#include <cstdlib>
#include <string>

#define SOURCES_FILE  "sources.xml"
#define XML_SOURCES   "sources"
#define XML_SOURCE    "source"

CMediaSourceSettings::CMediaSourceSettings()
{
  Clear();
}

CMediaSourceSettings::~CMediaSourceSettings() = default;

CMediaSourceSettings& CMediaSourceSettings::GetInstance()
{
  static CMediaSourceSettings sMediaSourceSettings;
  return sMediaSourceSettings;
}

std::string CMediaSourceSettings::GetSourcesFile()
{
  const std::shared_ptr<CProfileManager> profileManager = CServiceBroker::GetSettingsComponent()->GetProfileManager();

  std::string file;
  if (profileManager->GetCurrentProfile().hasSources())
    file = profileManager->GetProfileUserDataFolder();
  else
    file = profileManager->GetUserDataFolder();

  return URIUtils::AddFileToFolder(file, SOURCES_FILE);
}

void CMediaSourceSettings::OnSettingsLoaded()
{
  Load();
}

void CMediaSourceSettings::OnSettingsUnloaded()
{
  Clear();
}

bool CMediaSourceSettings::Load()
{
  return Load(GetSourcesFile());
}

bool CMediaSourceSettings::Load(const std::string &file)
{
  Clear();

  if (!CFileUtils::Exists(file))
    return false;

  CLog::Log(LOGINFO, "CMediaSourceSettings: loading media sources from {}", file);

  // load xml file
  CXBMCTinyXML xmlDoc;
  if (!xmlDoc.LoadFile(file))
  {
    CLog::Log(LOGERROR, "CMediaSourceSettings: error loading {}: Line {}, {}", file,
              xmlDoc.ErrorRow(), xmlDoc.ErrorDesc());
    return false;
  }

  TiXmlElement *pRootElement = xmlDoc.RootElement();
  if (pRootElement == NULL || !StringUtils::EqualsNoCase(pRootElement->ValueStr(), XML_SOURCES))
    CLog::Log(LOGERROR, "CMediaSourceSettings: sources.xml file does not contain <sources>");

  // parse sources
  std::string dummy;
  GetSources(pRootElement, "video", m_videoSources, dummy);
  GetSources(pRootElement, "programs", m_programSources, m_defaultProgramSource);
  GetSources(pRootElement, "pictures", m_pictureSources, m_defaultPictureSource);
  GetSources(pRootElement, "files", m_fileSources, m_defaultFileSource);
  GetSources(pRootElement, "music", m_musicSources, m_defaultMusicSource);
  GetSources(pRootElement, "games", m_gameSources, dummy);

  return true;
}

bool CMediaSourceSettings::Save()
{
  return Save(GetSourcesFile());
}

bool CMediaSourceSettings::Save(const std::string &file) const
{
  //! @todo Should we be specifying utf8 here??
  CXBMCTinyXML doc;
  TiXmlElement xmlRootElement(XML_SOURCES);
  TiXmlNode *pRoot = doc.InsertEndChild(xmlRootElement);
  if (pRoot == NULL)
    return false;

  // ok, now run through and save each sources section
  SetSources(pRoot, "programs", m_programSources, m_defaultProgramSource);
  SetSources(pRoot, "video", m_videoSources, "");
  SetSources(pRoot, "music", m_musicSources, m_defaultMusicSource);
  SetSources(pRoot, "pictures", m_pictureSources, m_defaultPictureSource);
  SetSources(pRoot, "files", m_fileSources, m_defaultFileSource);
  SetSources(pRoot, "games", m_gameSources, "");

  CWakeOnAccess::GetInstance().QueueMACDiscoveryForAllRemotes();

  return doc.SaveFile(file);
}

void CMediaSourceSettings::Clear()
{
  m_programSources.clear();
  m_pictureSources.clear();
  m_fileSources.clear();
  m_musicSources.clear();
  m_videoSources.clear();
  m_gameSources.clear();
}

VECSOURCES* CMediaSourceSettings::GetSources(const std::string &type)
{
  if (type == "programs" || type == "myprograms")
    return &m_programSources;
  else if (type == "files")
    return &m_fileSources;
  else if (type == "music")
    return &m_musicSources;
  else if (type == "video" || type == "videos")
    return &m_videoSources;
  else if (type == "pictures")
    return &m_pictureSources;
  else if (type == "games")
    return &m_gameSources;

  return NULL;
}

const std::string& CMediaSourceSettings::GetDefaultSource(const std::string &type) const
{
  if (type == "programs" || type == "myprograms")
    return m_defaultProgramSource;
  else if (type == "files")
    return m_defaultFileSource;
  else if (type == "music")
    return m_defaultMusicSource;
  else if (type == "pictures")
    return m_defaultPictureSource;

  return StringUtils::Empty;
}

void CMediaSourceSettings::SetDefaultSource(const std::string &type, const std::string &source)
{
  if (type == "programs" || type == "myprograms")
    m_defaultProgramSource = source;
  else if (type == "files")
    m_defaultFileSource = source;
  else if (type == "music")
    m_defaultMusicSource = source;
  else if (type == "pictures")
    m_defaultPictureSource = source;
}

// NOTE: This function does NOT save the sources.xml file - you need to call SaveSources() separately.
bool CMediaSourceSettings::UpdateSource(const std::string &strType, const std::string &strOldName, const std::string &strUpdateChild, const std::string &strUpdateValue)
{
  VECSOURCES *pShares = GetSources(strType);
  if (pShares == NULL)
    return false;

  for (IVECSOURCES it = pShares->begin(); it != pShares->end(); ++it)
  {
    if (it->strName == strOldName)
    {
      if (strUpdateChild == "name")
        it->strName = strUpdateValue;
      else if (strUpdateChild == "lockmode")
        it->m_iLockMode = (LockType)std::strtol(strUpdateValue.c_str(), NULL, 10);
      else if (strUpdateChild == "lockcode")
        it->m_strLockCode = strUpdateValue;
      else if (strUpdateChild == "badpwdcount")
        it->m_iBadPwdCount = (int)std::strtol(strUpdateValue.c_str(), NULL, 10);
      else if (strUpdateChild == "thumbnail")
        it->m_strThumbnailImage = strUpdateValue;
      else if (strUpdateChild == "path")
      {
        it->vecPaths.clear();
        it->strPath = strUpdateValue;
        it->vecPaths.push_back(strUpdateValue);
      }
      else
        return false;

      return true;
    }
  }

  return false;
}

bool CMediaSourceSettings::DeleteSource(const std::string &strType, const std::string &strName, const std::string &strPath, bool virtualSource /* = false */)
{
  VECSOURCES *pShares = GetSources(strType);
  if (pShares == NULL)
    return false;

  bool found = false;

  for (IVECSOURCES it = pShares->begin(); it != pShares->end(); ++it)
  {
    if (it->strName == strName && it->strPath == strPath)
    {
      CLog::Log(LOGDEBUG, "CMediaSourceSettings: found share, removing!");
      pShares->erase(it);
      found = true;
      break;
    }
  }

  if (virtualSource)
    return found;

  return Save();
}

bool CMediaSourceSettings::AddShare(const std::string &type, const CMediaSource &share)
{
  VECSOURCES *pShares = GetSources(type);
  if (pShares == NULL)
    return false;

  // translate dir and add to our current shares
  std::string strPath1 = share.strPath;
  if (strPath1.empty())
  {
    CLog::Log(LOGERROR, "CMediaSourceSettings: unable to add empty path");
    return false;
  }
  StringUtils::ToUpper(strPath1);

  CMediaSource shareToAdd = share;
  if (strPath1.at(0) == '$')
  {
    shareToAdd.strPath = CUtil::TranslateSpecialSource(strPath1);
    if (!share.strPath.empty())
      CLog::Log(LOGDEBUG, "CMediaSourceSettings: translated ({}) to path ({})", strPath1,
                shareToAdd.strPath);
    else
    {
      CLog::Log(LOGDEBUG, "CMediaSourceSettings: skipping invalid special directory token ({})",
                strPath1);
      return false;
    }
  }
  pShares->push_back(shareToAdd);

  if (!share.m_ignore)
    return Save();

  return true;
}

bool CMediaSourceSettings::UpdateShare(const std::string &type, const std::string &oldName, const CMediaSource &share)
{
  VECSOURCES *pShares = GetSources(type);
  if (pShares == NULL)
    return false;

  // update our current share list
  CMediaSource* pShare = NULL;
  for (IVECSOURCES it = pShares->begin(); it != pShares->end(); ++it)
  {
    if (it->strName == oldName)
    {
      it->strName = share.strName;
      it->strPath = share.strPath;
      it->vecPaths = share.vecPaths;
      pShare = &(*it);
      break;
    }
  }

  if (pShare == NULL)
    return false;

  // Update our XML file as well
  return Save();
}

bool CMediaSourceSettings::GetSource(const std::string &category, const TiXmlNode *source, CMediaSource &share)
{
  const TiXmlNode *pNodeName = source->FirstChild("name");
  std::string strName;
  if (pNodeName && pNodeName->FirstChild())
    strName = pNodeName->FirstChild()->ValueStr();

  // get multiple paths
  std::vector<std::string> vecPaths;
  const TiXmlElement *pPathName = source->FirstChildElement("path");
  while (pPathName != NULL)
  {
    if (pPathName->FirstChild())
    {
      std::string strPath = pPathName->FirstChild()->ValueStr();

      // make sure there are no virtualpaths or stack paths defined in sources.xml
      if (!URIUtils::IsStack(strPath))
      {
        // translate special tags
        if (!strPath.empty() && strPath.at(0) == '$')
          strPath = CUtil::TranslateSpecialSource(strPath);

        // need to check path validity again as CUtil::TranslateSpecialSource() may have failed
        if (!strPath.empty())
        {
          URIUtils::AddSlashAtEnd(strPath);
          vecPaths.push_back(strPath);
        }
      }
      else
        CLog::Log(LOGERROR, "CMediaSourceSettings:    invalid path type ({}) in source", strPath);
    }

    pPathName = pPathName->NextSiblingElement("path");
  }

  const TiXmlNode *pLockMode = source->FirstChild("lockmode");
  const TiXmlNode *pLockCode = source->FirstChild("lockcode");
  const TiXmlNode *pBadPwdCount = source->FirstChild("badpwdcount");
  const TiXmlNode *pThumbnailNode = source->FirstChild("thumbnail");

  if (strName.empty() || vecPaths.empty())
    return false;

  std::vector<std::string> verifiedPaths;
  // disallowed for files, or there's only a single path in the vector
  if (StringUtils::EqualsNoCase(category, "files") || vecPaths.size() == 1)
    verifiedPaths.push_back(vecPaths[0]);
  // multiple paths?
  else
  {
    // validate the paths
    for (std::vector<std::string>::const_iterator path = vecPaths.begin(); path != vecPaths.end(); ++path)
    {
      CURL url(*path);
      bool bIsInvalid = false;

      // for my programs
      if (StringUtils::EqualsNoCase(category, "programs") || StringUtils::EqualsNoCase(category, "myprograms"))
      {
        // only allow HD and plugins
        if (url.IsLocal() || url.IsProtocol("plugin"))
          verifiedPaths.push_back(*path);
        else
          bIsInvalid = true;
      }
      // for others allow everything (if the user does something silly, we can't stop them)
      else
        verifiedPaths.push_back(*path);

      // error message
      if (bIsInvalid)
        CLog::Log(LOGERROR, "CMediaSourceSettings:    invalid path type ({}) for multipath source",
                  path->c_str());
    }

    // no valid paths? skip to next source
    if (verifiedPaths.empty())
    {
      CLog::Log(LOGERROR,"CMediaSourceSettings:    missing or invalid <name> and/or <path> in source");
      return false;
    }
  }

  share.FromNameAndPaths(category, strName, verifiedPaths);

  share.m_iBadPwdCount = 0;
  if (pLockMode)
  {
    share.m_iLockMode = (LockType)std::strtol(pLockMode->FirstChild()->Value(), NULL, 10);
    share.m_iHasLock = LOCK_STATE_LOCKED;
  }

  if (pLockCode && pLockCode->FirstChild())
    share.m_strLockCode = pLockCode->FirstChild()->Value();

  if (pBadPwdCount && pBadPwdCount->FirstChild())
    share.m_iBadPwdCount = (int)std::strtol(pBadPwdCount->FirstChild()->Value(), NULL, 10);

  if (pThumbnailNode && pThumbnailNode->FirstChild())
    share.m_strThumbnailImage = pThumbnailNode->FirstChild()->Value();

  XMLUtils::GetBoolean(source, "allowsharing", share.m_allowSharing);

  return true;
}

void CMediaSourceSettings::GetSources(const TiXmlNode* pRootElement, const std::string& strTagName, VECSOURCES& items, std::string& strDefault)
{
  strDefault = "";
  items.clear();

  const TiXmlNode *pChild = pRootElement->FirstChild(strTagName.c_str());
  if (pChild == NULL)
  {
    CLog::Log(LOGDEBUG, "CMediaSourceSettings: <{}> tag is missing or sources.xml is malformed",
              strTagName);
    return;
  }

  pChild = pChild->FirstChild();
  while (pChild != NULL)
  {
    std::string strValue = pChild->ValueStr();
    if (strValue == XML_SOURCE || strValue == "bookmark") // "bookmark" left in for backwards compatibility
    {
      CMediaSource share;
      if (GetSource(strTagName, pChild, share))
        items.push_back(share);
      else
        CLog::Log(LOGERROR, "CMediaSourceSettings:    Missing or invalid <name> and/or <path> in source");
    }
    else if (strValue == "default")
    {
      const TiXmlNode *pValueNode = pChild->FirstChild();
      if (pValueNode)
      {
        std::string pszText = pChild->FirstChild()->ValueStr();
        if (!pszText.empty())
          strDefault = pszText;
        CLog::Log(LOGDEBUG, "CMediaSourceSettings:    Setting <default> source to : {}",
                  strDefault);
      }
    }

    pChild = pChild->NextSibling();
  }
}

bool CMediaSourceSettings::SetSources(TiXmlNode *root, const char *section, const VECSOURCES &shares, const std::string &defaultPath) const
{
  TiXmlElement sectionElement(section);
  TiXmlNode *sectionNode = root->InsertEndChild(sectionElement);
  if (sectionNode == NULL)
    return false;

  XMLUtils::SetPath(sectionNode, "default", defaultPath);
  for (CIVECSOURCES it = shares.begin(); it != shares.end(); ++it)
  {
    const CMediaSource &share = *it;
    if (share.m_ignore)
      continue;

    TiXmlElement source(XML_SOURCE);
    XMLUtils::SetString(&source, "name", share.strName);

    for (unsigned int i = 0; i < share.vecPaths.size(); i++)
      XMLUtils::SetPath(&source, "path", share.vecPaths[i]);

    if (share.m_iHasLock)
    {
      XMLUtils::SetInt(&source, "lockmode", share.m_iLockMode);
      XMLUtils::SetString(&source, "lockcode", share.m_strLockCode);
      XMLUtils::SetInt(&source, "badpwdcount", share.m_iBadPwdCount);
    }

    if (!share.m_strThumbnailImage.empty())
      XMLUtils::SetPath(&source, "thumbnail", share.m_strThumbnailImage);

    XMLUtils::SetBoolean(&source, "allowsharing", share.m_allowSharing);

    sectionNode->InsertEndChild(source);
  }

  return true;
}
