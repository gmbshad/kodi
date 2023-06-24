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

#include "GameServices.h"
#include "cores/RetroPlayer/rendering/VideoShaders/VideoShaderPresetFactory.h"
#include "controllers/Controller.h"
#include "controllers/ControllerManager.h"
#include "games/GameSettings.h"
#include "profiles/ProfilesManager.h"

using namespace KODI;
using namespace GAME;

CGameServices::CGameServices(CControllerManager &controllerManager,
                             RETRO:: CGUIGameRenderManager &renderManager,
                             CSettings &settings,
                             PERIPHERALS::CPeripherals &peripheralManager,
                             const CProfilesManager &profileManager,
                             ADDON::CAddonMgr &addons,
                             ADDON::CBinaryAddonManager &binaryAddons) :
  m_controllerManager(controllerManager),
  m_gameRenderManager(renderManager),
  m_profileManager(profileManager),
  m_gameSettings(new CGameSettings(settings)),
  m_videoShaders(new SHADER::CVideoShaderPresetFactory(addons, binaryAddons))
{
}

CGameServices::~CGameServices() = default;

ControllerPtr CGameServices::GetController(const std::string& controllerId)
{
  return m_controllerManager.GetController(controllerId);
}

ControllerPtr CGameServices::GetDefaultController()
{
  return m_controllerManager.GetDefaultController();
}

ControllerPtr CGameServices::GetDefaultKeyboard()
{
  return m_controllerManager.GetDefaultKeyboard();
}

ControllerPtr CGameServices::GetDefaultMouse()
{
  return m_controllerManager.GetDefaultMouse();
}

ControllerVector CGameServices::GetControllers()
{
  return m_controllerManager.GetControllers();
}

std::string CGameServices::GetSavestatesFolder() const
{
  return m_profileManager.GetSavestatesFolder();
}