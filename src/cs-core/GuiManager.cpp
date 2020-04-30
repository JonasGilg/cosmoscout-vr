////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GuiManager.hpp"

#include <GL/freeglut.h>

#include "../cs-utils/filesystem.hpp"
#include "../cs-utils/utils.hpp"
#include "InputManager.hpp"
#include "cs-version.hpp"
#include "logger.hpp"
#include "tools/Tool.hpp"

#include <VistaKernel/DisplayManager/GlutWindowImp/VistaGlutWindowingToolkit.h>
#include <VistaKernel/DisplayManager/VistaDisplayManager.h>
#include <VistaKernel/DisplayManager/VistaDisplaySystem.h>
#include <VistaKernel/DisplayManager/VistaViewportResizeToProjectionAdapter.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernel/GraphicsManager/VistaTransformNode.h>
#include <VistaKernel/InteractionManager/VistaUserPlatform.h>
#include <VistaKernel/Stuff/VistaFramerateDisplay.h>
#include <VistaKernel/VistaFrameLoop.h>
#include <VistaKernel/VistaSystem.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>
#include <fstream>
#include <utility>

namespace cs::core {

////////////////////////////////////////////////////////////////////////////////////////////////////

GuiManager::GuiManager(std::shared_ptr<Settings> settings,
    std::shared_ptr<InputManager> pInputManager, std::shared_ptr<utils::FrameTimings> pFrameTimings)
    : mInputManager(std::move(pInputManager))
    , mSettings(std::move(settings))
    , mFrameTimings(std::move(pFrameTimings)) {

  // Tell the user what's going on.
  logger().debug("Creating GuiManager.");

  // Initialize the Chromium Embedded Framework.
  gui::init();

  // Update the main viewport when the window is resized.
  VistaViewport* pViewport(GetVistaSystem()->GetDisplayManager()->GetViewports().begin()->second);
  mViewportUpdater = std::make_unique<VistaViewportResizeToProjectionAdapter>(pViewport);
  mViewportUpdater->SetUpdateMode(VistaViewportResizeToProjectionAdapter::MAINTAIN_HORIZONTAL_FOV);

  // Create GuiAreas and attach them to the SceneGraph ---------------------------------------------

  // The global GUI is drawn in world-space, the local GUI is drawn in screen-space.
  auto* pSG          = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();
  mLocalGuiTransform = pSG->NewTransformNode(pSG->GetRoot());

  // The global GUI area is only created when the according settings key was specified.
  if (mSettings->mGuiPosition) {
    auto* platform = GetVistaSystem()
                         ->GetPlatformFor(GetVistaSystem()->GetDisplayManager()->GetDisplaySystem())
                         ->GetPlatformNode();
    mGlobalGuiTransform = pSG->NewTransformNode(platform);

    mGlobalGuiTransform->Scale(static_cast<float>(mSettings->mGuiPosition->mWidthMeter),
        static_cast<float>(mSettings->mGuiPosition->mHeightMeter), 1.0F);
    mGlobalGuiTransform->Rotate(VistaAxisAndAngle(
        VistaVector3D(1, 0, 0), static_cast<float>(mSettings->mGuiPosition->mRotX)));
    mGlobalGuiTransform->Rotate(VistaAxisAndAngle(
        VistaVector3D(0, 1, 0), static_cast<float>(mSettings->mGuiPosition->mRotY)));
    mGlobalGuiTransform->Rotate(VistaAxisAndAngle(
        VistaVector3D(0, 0, 1), static_cast<float>(mSettings->mGuiPosition->mRotZ)));
    mGlobalGuiTransform->Translate(static_cast<float>(mSettings->mGuiPosition->mPosXMeter),
        static_cast<float>(mSettings->mGuiPosition->mPosYMeter),
        static_cast<float>(mSettings->mGuiPosition->mPosZMeter));

    // Create the global GUI area.
    mGlobalGuiArea = std::make_unique<gui::WorldSpaceGuiArea>(
        mSettings->mGuiPosition->mWidthPixel, mSettings->mGuiPosition->mHeightPixel);
    mGlobalGuiArea->setUseLinearDepthBuffer(true);
  }

  // Create the local GUI area.
  mLocalGuiArea = std::make_unique<gui::ScreenSpaceGuiArea>(pViewport);

  // Make sure that the GUI is drawn at the correct position in the draw order.
  mLocalGuiOpenGLnode = pSG->NewOpenGLNode(mLocalGuiTransform, mLocalGuiArea.get());
  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mLocalGuiOpenGLnode, static_cast<int>(utils::DrawOrder::eGui));

  // Make the local GuiArea receive input events.
  mInputManager->registerSelectable(mLocalGuiArea.get());

  if (mGlobalGuiArea) {
    // Make sure that the GUI is drawn at the correct position in the draw order.
    mGlobalGuiOpenGLnode = pSG->NewOpenGLNode(mGlobalGuiTransform, mGlobalGuiArea.get());
    VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
        mGlobalGuiTransform, static_cast<int>(utils::DrawOrder::eGui));

    // Make the global GuiArea receive input events.
    mInputManager->registerSelectable(mGlobalGuiOpenGLnode);
  }

  // Now create the actual Gui and add it to the previously created GuiAreas ----------------
  mCosmoScoutGui = std::make_unique<gui::GuiItem>("file://../share/resources/gui/cosmoscout.html");
  mStatistics    = std::make_unique<gui::GuiItem>("file://../share/resources/gui/statistics.html");

  // Except for mStatistics, all GuiItems are attached to the global world-space GuiArea if it is
  // available. If not, they are added to the local screen-space GuiArea.
  if (mGlobalGuiArea) {
    mGlobalGuiArea->addItem(mCosmoScoutGui.get());
  } else {
    mLocalGuiArea->addItem(mCosmoScoutGui.get());
  }

  mLocalGuiArea->addItem(mStatistics.get());

  // Configure attributes of the loading screen. Per default, GuiItems are drawn full-screen in
  // their GuiAreas.

  mCosmoScoutGui->setRelSizeX(1.F);
  mCosmoScoutGui->setRelSizeY(1.F);
  mCosmoScoutGui->setRelPositionX(0.5F);
  mCosmoScoutGui->setRelPositionY(0.5F);
  mCosmoScoutGui->setCursorChangeCallback([](gui::Cursor c) { setCursor(c); });

  // Configure the positioning and attributes of the statistics.
  mStatistics->setSizeX(600);
  mStatistics->setSizeY(320);
  mStatistics->setOffsetX(-300);
  mStatistics->setOffsetY(500);
  mStatistics->setRelPositionY(0.F);
  mStatistics->setRelPositionX(1.F);
  mStatistics->setIsInteractive(false);
  mStatistics->setCanScroll(false);

  // Now we will call some JavaScript methods - so we have to wait until the GuiItems have been
  // fully loaded.
  mCosmoScoutGui->waitForFinishedLoading();

  // Create a string which contains the current version number of CosmoScout VR. This string is then
  // shown on the loading screen.
  std::string version("v" + CS_PROJECT_VERSION);

  if (CS_GIT_BRANCH == "HEAD") {
    version += " (@" + CS_GIT_COMMIT_HASH + ")";
  } else if (!CS_GIT_BRANCH.empty()) {
    version += " (" + CS_GIT_BRANCH;
    if (!CS_GIT_COMMIT_HASH.empty()) {
      version += " @" + CS_GIT_COMMIT_HASH;
    }
    version += ")";
  }

  mCosmoScoutGui->callJavascript("CosmoScout.loadingScreen.setVersion", version);

  // Restore history from saved file. Currently we don't update the history when reloading a
  // settings file at runtime, as overwriting the history feels a bit odd.
  if (mSettings->mCommandHistory && !mSettings->mCommandHistory.value().empty()) {
    nlohmann::json array = mSettings->mCommandHistory.value();
    mCosmoScoutGui->executeJavascript("CosmoScout.statusbar.history = " + array.dump());
    mCosmoScoutGui->executeJavascript("CosmoScout.statusbar.historyIndex = " +
                                      std::to_string(mSettings->mCommandHistory.value().size()));
  }

  // Register a callback which is used by the statusbur to store executed commands on the C++ side.
  mCosmoScoutGui->registerCallback("statusbar.addCommandToHistory",
      "Adds a string to the command history so that it can be saved between sessions.",
      std::function([this](std::string&& command) {
        if (!mSettings->mCommandHistory) {
          mSettings->mCommandHistory = std::deque<std::string>();
        }

        mSettings->mCommandHistory.value().push_back(command);

        if (mSettings->mCommandHistory.value().size() > 20) {
          mSettings->mCommandHistory.value().pop_front();
        }
      }));

  // Set settings for the time Navigation
  mSettings->pMinDate.connectAndTouch([this](std::string const& minDate) {
    mCosmoScoutGui->callJavascript(
        "CosmoScout.timeline.setTimelineRange", minDate, mSettings->pMaxDate.get());
  });

  mSettings->pMaxDate.connect([this](std::string const& maxDate) {
    mCosmoScoutGui->callJavascript(
        "CosmoScout.timeline.setTimelineRange", mSettings->pMinDate.get(), maxDate);
  });

  // Hide the user interface when ESC is pressed.
  mInputManager->sOnEscapePressed.connect(
      [this]() { mSettings->pEnableUserInterface = !mSettings->pEnableUserInterface.get(); });

  mSettings->pEnableUserInterface.connectAndTouch([this](bool enable) {
    if (mGlobalGuiTransform) {
      mGlobalGuiTransform->SetIsEnabled(enable);
    }
    mLocalGuiTransform->SetIsEnabled(enable);
    mCosmoScoutGui->setIsInteractive(enable);
  });

  for (const auto& mEvent : mSettings->mEvents) {
    std::string planet;
    std::string place;
    if (mEvent.mLocation.has_value()) {
      planet = mEvent.mLocation.value().mPlanet;
      place  = mEvent.mLocation.value().mPlace;
    }
    addEventToTimenavigationBar(mEvent.mStart, mEvent.mEnd, mEvent.mId, mEvent.mContent,
        mEvent.mStyle, mEvent.mDescription, planet, place);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

GuiManager::~GuiManager() {
  try {
    // Tell the user what's going on.
    logger().debug("Deleting GuiManager.");
  } catch (...) {}

  mCosmoScoutGui->unregisterCallback("statusbar.addCommandToHistory");

  mInputManager->unregisterSelectable(mLocalGuiOpenGLnode);

  if (mGlobalGuiOpenGLnode) {
    mInputManager->unregisterSelectable(mGlobalGuiOpenGLnode);
  }

  // Free resources acquired by the Chromium Embedded Framework.
  gui::cleanUp();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::setCursor(gui::Cursor cursor) {
  auto* windowingToolkit = dynamic_cast<VistaGlutWindowingToolkit*>(
      GetVistaSystem()->GetDisplayManager()->GetWindowingToolkit());

  int glutCursor = GLUT_CURSOR_LEFT_ARROW;

  switch (cursor) {
  case gui::Cursor::ePointer:
    glutCursor = GLUT_CURSOR_LEFT_ARROW;
    break;
  case gui::Cursor::eCross:
    glutCursor = GLUT_CURSOR_CROSSHAIR;
    break;
  case gui::Cursor::eHelp:
    glutCursor = GLUT_CURSOR_HELP;
    break;
  case gui::Cursor::eWait:
    glutCursor = GLUT_CURSOR_WAIT;
    break;
  case gui::Cursor::eIbeam:
    glutCursor = GLUT_CURSOR_TEXT;
    break;
  case gui::Cursor::eHand:
    glutCursor = GLUT_CURSOR_INFO;
    break;
  default:
    break;
  }

  if (windowingToolkit) {
    for (auto const& window : GetVistaSystem()->GetDisplayManager()->GetWindows()) {
      windowingToolkit->SetCursor(window.second, glutCursor);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::showNotification(std::string const& sTitle, std::string const& sText,
    std::string const& sIcon, std::string const& sFlyToOnClick) const {
  mCosmoScoutGui->callJavascript(
      "CosmoScout.notifications.print", sTitle, sText, sIcon, sFlyToOnClick);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

gui::GuiItem* GuiManager::getGui() const {
  return mCosmoScoutGui.get();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

gui::GuiItem* GuiManager::getStatistics() const {
  return mStatistics.get();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::enableLoadingScreen(bool enable) {
  mCosmoScoutGui->callJavascript("CosmoScout.loadingScreen.setLoading", enable);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::setLoadingScreenStatus(std::string const& sStatus) const {
  mCosmoScoutGui->callJavascript("CosmoScout.loadingScreen.setStatus", sStatus);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::setLoadingScreenProgress(float percent, bool animate) const {
  mCosmoScoutGui->callJavascript("CosmoScout.loadingScreen.setProgress", percent, animate);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::update() {

  // If frame timings are enabled, collect the data and send it to the statistics GuiItem.
  mStatistics->setIsEnabled(mFrameTimings->pEnableMeasurements.get());
  if (mFrameTimings->pEnableMeasurements.get()) {
    std::string json("{");
    for (auto const& timings : mFrameTimings->getCalculatedQueryResults()) {
      uint64_t timeGPU(timings.second.mGPUTime);
      uint64_t timeCPU(timings.second.mCPUTime);

      uint64_t const waitNanos = 100000;
      if (timeGPU > waitNanos || timeCPU > waitNanos) {
        json += "\"" + timings.first + "\":[" + std::to_string(timeGPU) + "," +
                std::to_string(timeCPU) + "],";
      }
    }
    json.back() = '}';

    if (json.length() <= 1) {
      json = "{}";
    }

    mStatistics->callJavascript(
        "CosmoScout.statistics.setData", json, GetVistaSystem()->GetFrameLoop()->GetFrameRate());
  }

  // Update all entities of the Chromium Embedded Framework.
  gui::update();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::addPluginTabToSideBar(
    std::string const& name, std::string const& icon, std::string const& content) {
  mCosmoScoutGui->callJavascript("CosmoScout.sidebar.addPluginTab", name, icon, content);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::addPluginTabToSideBarFromHTML(
    std::string const& name, std::string const& icon, std::string const& htmlFile) {
  std::string content = utils::filesystem::loadToString(htmlFile);
  addPluginTabToSideBar(name, icon, content);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::addSettingsSectionToSideBar(
    std::string const& name, std::string const& icon, std::string const& content) {
  mCosmoScoutGui->callJavascript("CosmoScout.sidebar.addSettingsSection", name, icon, content);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::addSettingsSectionToSideBarFromHTML(
    std::string const& name, std::string const& icon, std::string const& htmlFile) {
  std::string content = utils::filesystem::loadToString(htmlFile);
  addSettingsSectionToSideBar(name, icon, content);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::removePluginTab(std::string const& name) {
  mCosmoScoutGui->callJavascript("CosmoScout.sidebar.removePluginTab", name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::removeSettingsSection(std::string const& name) {
  mCosmoScoutGui->callJavascript("CosmoScout.sidebar.removeSettingsSection", name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::addScriptToGui(std::string const& src) {
  mCosmoScoutGui->executeJavascript(src);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::addScriptToGuiFromJS(std::string const& jsFile) {
  std::string content = utils::filesystem::loadToString(jsFile);
  addScriptToGui(content);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::addHtmlToGui(std::string const& id, std::string const& src) {
  std::string content = utils::filesystem::loadToString(src);
  mCosmoScoutGui->callJavascript("CosmoScout.gui.registerHtml", id, content);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::addCssToGui(const std::string& fileName) {
  mCosmoScoutGui->callJavascript("CosmoScout.gui.registerCss", fileName);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::addEventToTimenavigationBar(std::string const& start,
    std::optional<std::string> const& end, std::string const& id, std::string const& content,
    std::optional<std::string> const& style, std::string const& description,
    std::string const& planet, std::string const& place) {
  mCosmoScoutGui->callJavascript("CosmoScout.timeline.addItem", start, end.value_or(""), id,
      content, style.value_or(""), description, planet, place);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::setCheckboxValue(std::string const& name, bool val, bool emitCallbacks) const {
  mCosmoScoutGui->callJavascript("CosmoScout.gui.setCheckboxValue", name, val, emitCallbacks);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::setRadioChecked(std::string const& name, bool emitCallbacks) const {
  mCosmoScoutGui->callJavascript("CosmoScout.gui.setRadioChecked", name, emitCallbacks);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::setSliderValue(std::string const& name, double val, bool emitCallbacks) const {
  mCosmoScoutGui->callJavascript("CosmoScout.gui.setSliderValue", name, emitCallbacks, val);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::setSliderValue(
    std::string const& name, glm::dvec2 const& val, bool emitCallbacks) const {
  mCosmoScoutGui->callJavascript(
      "CosmoScout.gui.setSliderValue", name, emitCallbacks, val.x, val.y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace cs::core
