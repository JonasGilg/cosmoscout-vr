////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CS_CORE_GUI_MANAGER_HPP
#define CS_CORE_GUI_MANAGER_HPP

#include "cs_core_export.hpp"

#include "../cs-gui/GuiItem.hpp"
#include "../cs-gui/ScreenSpaceGuiArea.hpp"
#include "../cs-gui/WorldSpaceGuiArea.hpp"
#include "../cs-gui/gui.hpp"
#include "../cs-gui/types.hpp"

#include "../cs-utils/FrameTimings.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>

class VistaOpenGLNode;
class VistaViewportResizeToProjectionAdapter;
class VistaTransformNode;

namespace cs::utils {
class FrameTimings;
} // namespace cs::utils

namespace cs::core {
class Settings;
class InputManager;

/// The GuiManager is the central access point to the application's user interface.
/// The user interface of CosmoScout VR consists of several webpages (GuiItems) which are rendered
/// with the Chromium Embedded Framework.
///
/// The GuiItems are either drawn in screen-space or - if the settings key "gui": {...} is specified
/// - in world-space. The key differences are:
/// Screen-Space:
///  * The UI automatically resizes when the window is resized
///  * When running in a clustered setup, each display will show an individual copy of the same
///    item. This is for example useful for the statistics GuiItem which is in all cases shown in
///    screen-space.
/// World-Space:
///  * The UI is drawn in a fixed resolution which is specified in the "guiPosition": {...} settings
///    key.
///  * When running in a clustered setup, the UI will be displayed across multiple displays.
///
/// There are several GuiItems involved: e.g. the timeline, the status-bar, the side-bar and the
/// notifications area. There are methods for getting access to these GuiItems - for example, these
/// can be used to register callbacks which will be executed when a button is pressed in the UI.
/// Plugins can add content to the sidebar. This is done with the methods addPluginTabToSideBar(),
/// addSettingsSectionToSideBar() and addScriptToGui().
///
/// This class should only be instantiated once - this is done by the Application class and this
/// instance is then passed to all plugins.
class CS_CORE_EXPORT GuiManager {
 public:
  GuiManager(std::shared_ptr<Settings> settings, std::shared_ptr<InputManager> pInputManager,
      std::shared_ptr<utils::FrameTimings> pFrameTimings);

  GuiManager(GuiManager const& other) = delete;
  GuiManager(GuiManager&& other)      = delete;

  GuiManager& operator=(GuiManager const& other) = delete;
  GuiManager& operator=(GuiManager&& other) = delete;

  virtual ~GuiManager();

  /// Set the cursor icon. This is usually used in the following way:
  /// guiItem->setCursorChangeCallback([](cs::gui::Cursor c) { GuiManager::setCursor(c); });
  static void setCursor(gui::Cursor cursor);

  /// Shows a notification in the top right corner.
  ///
  /// @param sTitle        The first line of the notification.
  /// @param sText         The second line of the notification.
  /// @param sIcon         The name of the material theme icon the notification should display.
  /// @param sFlyToOnClick The name of a location to fly to when clicked.
  void showNotification(std::string const& sTitle, std::string const& sText,
      std::string const& sIcon, std::string const& sFlyToOnClick = "") const;

  /// Adds a new tab to the side bar.
  ///
  /// @param name     The nam/title of the tab.
  /// @param icon     The name of the Material icon.
  /// @param content  The HTML that describes the tabs contents.
  void addPluginTabToSideBar(
      std::string const& name, std::string const& icon, std::string const& content);

  /// Adds a new tab to the side bar.
  ///
  /// @param name      The nam/title of the tab.
  /// @param icon      The name of the Material icon.
  /// @param htmlFile  The HTML file that describes the tabs contents.
  void addPluginTabToSideBarFromHTML(
      std::string const& name, std::string const& icon, std::string const& htmlFile);

  /// Adds a new section to the settings tab.
  ///
  /// @param name     The name/title of the section.
  /// @param content  The HTML that describes the sections contents.
  void addSettingsSectionToSideBar(
      std::string const& name, std::string const& icon, std::string const& content);

  /// Adds a new section to the settings tab.
  ///
  /// @param name      The name/title of the section.
  /// @param htmlFile  The HTML file that describes the sections contents.
  void addSettingsSectionToSideBarFromHTML(
      std::string const& name, std::string const& icon, std::string const& htmlFile);

  void removePluginTab(std::string const& name);
  void removeSettingsSection(std::string const& name);

  /// This can be used to initialize the DOM elements added to the sidebar with the methods above.
  /// This is identical to getGui()->executeJavascript(src);
  ///
  /// @param src The javascript source code.
  void addScriptToGui(std::string const& src);

  /// This can be used to initialize the DOM elements added to the sidebar with the methods above.
  ///
  /// @param jsFile The javascript file that contains the source code.
  void addScriptToGuiFromJS(std::string const& jsFile);

  /// Append HTML to the body.
  /// The src content will be wrapped in a template element.
  ///
  /// @param src The html source code
  void addHtmlToGui(std::string const& id, std::string const& src);

  /// Adds a link element to the head with a local file href.
  ///
  /// @param fileName The filename in the css folder
  void addCssToGui(std::string const& fileName);

  /// Adds an event item to the timenavigation
  ///
  /// @param start The start date of the event.
  /// @param end The optional end date of the event.
  /// @param id The id of the event.
  /// @param content The name or content of the event.
  /// @param style The optional css of the event.
  /// @param description The description of the event.
  /// @param planet Planet the event is happening on.
  /// @parama place The location on the planet.
  void addEventToTimenavigationBar(std::string const& start, std::optional<std::string> const& end,
      std::string const& id, std::string const& content, std::optional<std::string> const& style,
      std::string const& description, std::string const& planet, std::string const& place);

  /// Sets a checkbox to the given value. This is only a thin wrapper for
  /// "CosmoScout.gui.setCheckboxValue" but provides compile time type safety.
  void setCheckboxValue(std::string const& name, bool val, bool emitCallbacks = false) const;

  /// Checks a radio button. This is only a thin wrapper for "CosmoScout.gui.setRadioChecked" but
  /// provides compile time type safety.
  void setRadioChecked(std::string const& name, bool emitCallbacks = false) const;

  /// Sets a slider (with one or two handles) to the given value(s). These are only a thin wrappers
  /// for "CosmoScout.gui.setSliderValue" but provide compile time type safety.
  void setSliderValue(std::string const& name, double val, bool emitCallbacks = false) const;
  void setSliderValue(
      std::string const& name, glm::dvec2 const& val, bool emitCallbacks = false) const;

  /// Returns the CosmoScout Gui.
  gui::GuiItem* getGui() const;

  /// Returns the CosmoScout Statistics Gui.
  gui::GuiItem* getStatistics() const;

  /// Shows or hides the loading screen.
  void enableLoadingScreen(bool enable);

  /// Sets the status text on the loading screen. This is only useful during application start-up,
  /// as the loading screen will be hidden thereafter.
  void setLoadingScreenStatus(std::string const& sStatus) const;

  /// Sets the progress bar state.
  void setLoadingScreenProgress(float percent, bool animate) const;

  /// This is called once a frame from the Application.
  void update();

 private:
  std::shared_ptr<InputManager>        mInputManager;
  std::shared_ptr<Settings>            mSettings;
  std::shared_ptr<utils::FrameTimings> mFrameTimings;

  std::unique_ptr<VistaViewportResizeToProjectionAdapter> mViewportUpdater;
  std::unique_ptr<gui::WorldSpaceGuiArea>                 mGlobalGuiArea;
  std::unique_ptr<gui::ScreenSpaceGuiArea>                mLocalGuiArea;

  std::unique_ptr<gui::GuiItem> mCosmoScoutGui;
  std::unique_ptr<gui::GuiItem> mStatistics;

  // The global GUI is drawn in world-space.
  VistaTransformNode* mGlobalGuiTransform  = nullptr;
  VistaOpenGLNode*    mGlobalGuiOpenGLnode = nullptr;

  // The local GUI is drawn in screen-space.
  VistaTransformNode* mLocalGuiTransform  = nullptr;
  VistaOpenGLNode*    mLocalGuiOpenGLnode = nullptr;
};

} // namespace cs::core

#endif // CS_CORE_GUI_MANAGER_HPP
