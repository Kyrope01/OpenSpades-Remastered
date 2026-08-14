/*
 Copyright (c) 2013 yvt

 This file is part of OpenSpades.

 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.

 */

#include "ServerList.as"
#include "../Preferences.as"
#include "../UIFramework/DropDownList.as"

namespace spades {

    // These helpers are shared by the startup configuration filter and console completion.
    // Keep their original global names so those separately included GUI scripts can resolve them.
    uint8 ToLower(uint8 c) {
        if (c >= uint8(0x41) and c <= uint8(0x5a))
            return uint8(c - 0x41 + 0x61);
        return c;
    }

    bool StringContainsCaseInsensitive(string text, string pattern) {
        for (int i = int(text.length) - 1; i >= 0; i--)
            text[i] = ToLower(text[i]);
        for (int i = int(pattern.length) - 1; i >= 0; i--)
            pattern[i] = ToLower(pattern[i]);
        return text.findFirst(pattern) >= 0;
    }

    /** Compact button backed by the UI framework's modal drop-down list. */
    class MainScreenDropDownButton : spades::ui::SimpleButton {
        private string[] items;
        private int index = 0;
        spades::ui::EventHandler @Changed;

        MainScreenDropDownButton(spades::ui::UIManager @manager) {
            super(manager);
            Alignment = Vector2(0.f, 0.5f);
        }

        void SetItems(string[] values) {
            items = values;
            Index = 0;
        }

        int Index {
            get { return index; }
            set {
                if (items.length == 0) {
                    index = 0;
                    Caption = "";
                    return;
                }
                index = Clamp(value, 0, int(items.length) - 1);
                Caption = items[index] + "  v";
            }
        }

        void OnActivated() {
            ButtonBase::OnActivated();
            spades::ui::ShowDropDownList(this, items,
                                         spades::ui::DropDownListHandler(this.ItemSelected));
        }

        private void ItemSelected(int newIndex) {
            if (newIndex < 0)
                return;
            bool changed = newIndex != index;
            Index = newIndex;
            if (changed && Changed !is null)
                Changed(this);
        }
    }

    /** Dark, thin-bordered panel used by the main menu layout. */
    class MainScreenPanel : spades::ui::UIElement {
        Vector4 FillColor = Vector4(0.018f, 0.018f, 0.018f, 0.82f);
        Vector4 BorderColor = Vector4(0.76f, 0.76f, 0.76f, 0.42f);

        MainScreenPanel(spades::ui::UIManager @manager) { super(manager); }

        void Render() {
            Renderer @renderer = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Image @white = renderer.RegisterImage("Gfx/White.tga");

            renderer.ColorNP = FillColor;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, size.y));
            renderer.ColorNP = BorderColor;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, 1.f));
            renderer.DrawImage(white, AABB2(pos.x, pos.y + size.y - 1.f, size.x, 1.f));
            renderer.DrawImage(white, AABB2(pos.x, pos.y, 1.f, size.y));
            renderer.DrawImage(white, AABB2(pos.x + size.x - 1.f, pos.y, 1.f, size.y));
        }
    }

    /** Flat menu button matching the narrow KyroSpades-style navigation rail. */
    class MainScreenNavigationButton : spades::ui::Button {
        bool IsExitButton = false;

        MainScreenNavigationButton(spades::ui::UIManager @manager) {
            super(manager);
            Alignment = Vector2(0.5f, 0.5f);
        }

        void Render() {
            Renderer @renderer = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Image @white = renderer.RegisterImage("Gfx/White.tga");

            Vector4 fill = Vector4(0.36f, 0.36f, 0.36f, 0.72f);
            Vector4 edge = Vector4(0.85f, 0.85f, 0.85f, 0.25f);
            Vector4 text = Vector4(1.f, 1.f, 1.f, 1.f);
            if (IsExitButton) {
                fill = Vector4(0.28f, 0.28f, 0.28f, 0.78f);
                edge = Vector4(0.72f, 0.72f, 0.72f, 0.38f);
            }
            if (!IsEnabled) {
                fill *= Vector4(0.55f, 0.55f, 0.55f, 0.62f);
                text = Vector4(0.68f, 0.68f, 0.68f, 0.72f);
            } else if (Toggled || (Pressed && Hover)) {
                fill += Vector4(0.18f, 0.18f, 0.18f, 0.08f);
                edge = Vector4(1.f, 1.f, 1.f, 0.48f);
            } else if (Hover) {
                fill += Vector4(0.11f, 0.11f, 0.11f, 0.07f);
            }

            renderer.ColorNP = fill;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, size.y));
            renderer.ColorNP = edge;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, 1.f));
            renderer.DrawImage(white, AABB2(pos.x, pos.y + size.y - 1.f, size.x, 1.f));
            renderer.DrawImage(white, AABB2(pos.x, pos.y, 1.f, size.y));
            renderer.DrawImage(white, AABB2(pos.x + size.x - 1.f, pos.y, 1.f, size.y));

            Vector2 textSize = Font.Measure(Caption);
            Font.DrawShadow(Caption, pos + (size - textSize) * 0.5f, 1.f, text,
                            Vector4(0.f, 0.f, 0.f, 0.55f));
        }
    }

    /** Standard button styling with a measured caption that always fits its current bounds. */
    class MainScreenFittedButton : spades::ui::Button {
        MainScreenFittedButton(spades::ui::UIManager @manager) { super(manager); }

        void Render() {
            Renderer @renderer = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Image @image = renderer.RegisterImage("Gfx/UI/Button.png");

            Vector4 color = Vector4(0.2f, 0.2f, 0.2f, 0.5f);
            if (Toggled || (Pressed && Hover))
                color = Vector4(0.7f, 0.7f, 0.7f, 0.9f);
            else if (Hover)
                color = Vector4(0.4f, 0.4f, 0.4f, 0.7f);
            if (!IsEnabled)
                color.w *= 0.5f;
            renderer.ColorNP = color;
            DrawSliceImage(renderer, image, pos.x, pos.y, size.x, size.y, 12.f);

            Vector2 textSize = Font.Measure(Caption);
            float scale = 1.f;
            float availableWidth = Max(1.f, size.x - 16.f);
            if (textSize.x > availableWidth)
                scale = availableWidth / textSize.x;
            Vector2 scaledTextSize = textSize * scale;
            Vector2 textPosition = pos + (size - scaledTextSize) * 0.5f;
            Vector4 textColor = Vector4(1.f, 1.f, 1.f, IsEnabled ? 1.f : 0.5f);
            Font.DrawShadow(Caption, textPosition, scale, textColor,
                            Vector4(0.f, 0.f, 0.f, IsEnabled ? 0.4f : 0.1f));
        }
    }

    class MainScreenMainMenu : spades::ui::UIElement {
        MainScreenUI @ui;

        private MainScreenPanel @navigationPanel;
        private MainScreenPanel @browserPanel;

        private MainScreenNavigationButton @serversButton;
        private MainScreenNavigationButton @settingsButton;
        private MainScreenNavigationButton @controlsButton;
        private MainScreenNavigationButton @creditsButton;
        private MainScreenNavigationButton @exitButton;

        private spades::ui::Field @quickConnectField;
        private spades::ui::Button @connectButton;
        private spades::ui::Button @localButton;
        private spades::ui::Button @refreshButton;
        private spades::ui::Button @protocol75Button;
        private spades::ui::Button @protocol76Button;

        private MainScreenFittedButton @serverListSourceButton;
        private spades::ui::Field @serverFilterField;
        private MainScreenDropDownButton @filterPlayersButton;
        private MainScreenDropDownButton @filterVersionButton;

        private ServerListHeader @serverListPlayersHeader;
        private ServerListHeader @serverListNameHeader;
        private ServerListHeader @serverListMapHeader;
        private ServerListHeader @serverListModeHeader;
        private ServerListHeader @serverListPingHeader;

        private spades::ui::ListView @serverListView;
        private MainScreenServerListLoadingView @serverListLoadingView;
        private MainScreenServerListErrorView @serverListErrorView;
        private MainScreenServerItem @selectedServer;

        private bool serverListLoaded = false;
        private bool serverListLoading = false;
        private bool serverListSuccess = false;
        private string lastServerFilter = "";

        private int sortIndex = 1;
        private bool sortDescending = true;
        private int serverListSource = 0;

        private ConfigItem cg_protocolVersion("cg_protocolVersion", "3");
        private ConfigItem cg_lastQuickConnectHost("cg_lastQuickConnectHost", "127.0.0.1");
        private ConfigItem cg_serverlistSort("cg_serverlistSort", "16385");
        private ConfigItem cg_serverListSource("cg_serverListSource", "0");
        private ConfigItem cl_serverListUrl(
            "cl_serverListUrl", "http://services.buildandshoot.com/serverlist.json");

        MainScreenMainMenu(MainScreenUI @ui) {
            super(ui.manager);
            @this.ui = ui;

            int savedSort = cg_serverlistSort.IntValue;
            sortIndex = savedSort & 0xfff;
            if (sortIndex < 0 || sortIndex > 4)
                sortIndex = 1;
            sortDescending = (savedSort & 0x4000) != 0;
            serverListSource = cg_serverListSource.IntValue == 1 ? 1 : 0;
            ApplyServerListSource();

            @navigationPanel = MainScreenPanel(Manager);
            AddChild(navigationPanel);
            @browserPanel = MainScreenPanel(Manager);
            AddChild(browserPanel);

            @serversButton = MakeNavigationButton(_Tr("MainScreen", "Servers"));
            serversButton.Toggle = true;
            serversButton.Toggled = true;
            @serversButton.Activated = spades::ui::EventHandler(this.ServersButtonPressed);

            @settingsButton = MakeNavigationButton(_Tr("MainScreen", "Settings"));
            @settingsButton.Activated = spades::ui::EventHandler(this.SettingsButtonPressed);
            @controlsButton = MakeNavigationButton(_Tr("MainScreen", "Controls"));
            @controlsButton.Activated = spades::ui::EventHandler(this.ControlsButtonPressed);

            @creditsButton = MakeNavigationButton(_Tr("MainScreen", "Credits"));
            @creditsButton.Activated = spades::ui::EventHandler(this.CreditsButtonPressed);
            @exitButton = MakeNavigationButton(_Tr("MainScreen", "Exit"));
            exitButton.IsExitButton = true;
            @exitButton.Activated = spades::ui::EventHandler(this.ExitButtonPressed);

            @quickConnectField = spades::ui::Field(Manager);
            quickConnectField.Placeholder = _Tr("MainScreen", "Server address (aos://...)");
            quickConnectField.Text = cg_lastQuickConnectHost.StringValue;
            @quickConnectField.Changed = spades::ui::EventHandler(this.QuickConnectChanged);
            AddChild(quickConnectField);

            @protocol75Button = spades::ui::Button(Manager);
            protocol75Button.Caption = "0.75";
            protocol75Button.Toggle = true;
            protocol75Button.Toggled = cg_protocolVersion.IntValue == 3;
            @protocol75Button.Activated = spades::ui::EventHandler(this.ProtocolButtonPressed);
            AddChild(protocol75Button);
            @protocol76Button = spades::ui::Button(Manager);
            protocol76Button.Caption = "0.76";
            protocol76Button.Toggle = true;
            protocol76Button.Toggled = cg_protocolVersion.IntValue != 3;
            @protocol76Button.Activated = spades::ui::EventHandler(this.ProtocolButtonPressed);
            AddChild(protocol76Button);

            @connectButton = spades::ui::Button(Manager);
            connectButton.Caption = _Tr("MainScreen", "Join");
            connectButton.Enable = quickConnectField.Text.length > 0;
            @connectButton.Activated = spades::ui::EventHandler(this.ConnectButtonPressed);
            AddChild(connectButton);
            @localButton = spades::ui::Button(Manager);
            localButton.Caption = _Tr("MainScreen", "Local");
            @localButton.Activated = spades::ui::EventHandler(this.LocalButtonPressed);
            AddChild(localButton);
            @refreshButton = spades::ui::Button(Manager);
            refreshButton.Caption = _Tr("MainScreen", "Refresh");
            @refreshButton.Activated = spades::ui::EventHandler(this.RefreshButtonPressed);
            AddChild(refreshButton);

            @serverListSourceButton = MainScreenFittedButton(Manager);
            serverListSourceButton.Caption = GetServerListSourceCaption();
            @serverListSourceButton.Activated =
                spades::ui::EventHandler(this.ServerListSourceButtonPressed);
            AddChild(serverListSourceButton);

            @serverFilterField = spades::ui::Field(Manager);
            serverFilterField.Placeholder = _Tr("MainScreen", "Search servers");
            @serverFilterField.Changed = spades::ui::EventHandler(this.FilterChanged);
            AddChild(serverFilterField);

            @filterPlayersButton = MainScreenDropDownButton(Manager);
            filterPlayersButton.SetItems(
                array<string> = {_Tr("MainScreen", "Players: Any"),
                                 _Tr("MainScreen", "Players: Not empty"),
                                 _Tr("MainScreen", "Players: Not full"),
                                 _Tr("MainScreen", "Players: Not empty/full")});
            @filterPlayersButton.Changed = spades::ui::EventHandler(this.FilterChanged);
            AddChild(filterPlayersButton);

            @filterVersionButton = MainScreenDropDownButton(Manager);
            filterVersionButton.SetItems(array<string> = {_Tr("MainScreen", "Version: Any"),
                                                           "0.75", "0.76"});
            @filterVersionButton.Changed = spades::ui::EventHandler(this.FilterChanged);
            AddChild(filterVersionButton);

            @serverListPlayersHeader = MakeHeader(_Tr("MainScreen", "Players"),
                                                  spades::ui::EventHandler(this.SortServerListByPlayers));
            @serverListNameHeader = MakeHeader(_Tr("MainScreen", "Name"),
                                               spades::ui::EventHandler(this.SortServerListByName));
            @serverListMapHeader = MakeHeader(_Tr("MainScreen", "Map"),
                                              spades::ui::EventHandler(this.SortServerListByMap));
            @serverListModeHeader = MakeHeader(_Tr("MainScreen", "Mode"),
                                               spades::ui::EventHandler(this.SortServerListByMode));
            @serverListPingHeader = MakeHeader(_Tr("MainScreen", "Ping"),
                                               spades::ui::EventHandler(this.SortServerListByPing));

            @serverListView = spades::ui::ListView(Manager);
            serverListView.RowHeight = 24.f;
            AddChild(serverListView);
            @serverListLoadingView = MainScreenServerListLoadingView(Manager);
            AddChild(serverListLoadingView);
            @serverListErrorView = MainScreenServerListErrorView(Manager);
            serverListErrorView.Visible = false;
            AddChild(serverListErrorView);

            // MainScreenUI starts the query after assigning this menu's initial bounds. Setting
            // the empty model while the list still has zero height would leave its scroll range
            // and page size at zero during ScrollBar.Layout().
        }

        private MainScreenNavigationButton @MakeNavigationButton(string caption) {
            MainScreenNavigationButton button(Manager);
            button.Caption = caption;
            AddChild(button);
            return button;
        }

        private ServerListHeader @MakeHeader(string text, spades::ui::EventHandler @handler) {
            ServerListHeader header(Manager);
            header.Text = text;
            @header.Activated = handler;
            AddChild(header);
            return header;
        }

        private string GetServerListSourceCaption() {
            if (serverListSource == 0)
                return _Tr("MainScreen", "Serverlist: Master");
            return _Tr("MainScreen", "Serverlist: checkpoint.aos.coffee");
        }

        private void ApplyServerListSource() {
            cg_serverListSource = serverListSource;
            if (serverListSource == 0)
                cl_serverListUrl = "http://services.buildandshoot.com/serverlist.json";
            else
                cl_serverListUrl = "http://checkpoint.aos.coffee/serverlist.json";
        }

        private void UpdateServerListSourceCaption(int count) {
            string caption = GetServerListSourceCaption();
            if (count >= 0)
                caption += " (" + ToString(count) + ")";
            serverListSourceButton.Caption = caption;
        }

        void OnResized() {
            float margin = 6.f;
            float gap = 8.f;
            float navigationWidth = Clamp(Size.x * 0.205f, 168.f, 194.f);
            float browserX = margin + navigationWidth + gap;
            float browserWidth = Max(360.f, Size.x - browserX - margin);
            float fullHeight = Max(360.f, Size.y - margin * 2.f);

            navigationPanel.Bounds = AABB2(margin, margin, navigationWidth, fullHeight);
            browserPanel.Bounds = AABB2(browserX, margin, browserWidth, fullHeight);

            float navInset = 5.f;
            float navY = margin + navInset;
            float navButtonHeight = Clamp(Size.y * 0.050f, 25.f, 30.f);
            float navGap = 4.f;
            float navButtonWidth = navigationWidth - navInset * 2.f;
            spades::ui::UIElement @[] navButtons = {
                serversButton, settingsButton, controlsButton, creditsButton, exitButton
            };
            for (uint i = 0; i < navButtons.length; i++) {
                navButtons[i].Bounds =
                    AABB2(margin + navInset, navY, navButtonWidth, navButtonHeight);
                navY += navButtonHeight + navGap;
            }

            float contentInset = 4.f;
            float contentX = browserX + contentInset;
            float contentWidth = browserWidth - contentInset * 2.f;

            // The server browser begins at the top of the content panel. The old showcase/news
            // strip was decorative and needlessly reduced the number of visible servers.
            float toolbarY = margin + contentInset;
            float toolbarHeight = 30.f;
            float toolbarGap = 4.f;
            // Scale every action before sacrificing the address field. At the minimum supported
            // panel width (352 px), these values still fit on one row without overlap.
            float protocolWidth = Clamp(contentWidth * 0.11f, 42.f, 54.f);
            float joinWidth = Clamp(contentWidth * 0.14f, 54.f, 76.f);
            float localWidth = Clamp(contentWidth * 0.14f, 54.f, 76.f);
            float refreshWidth = Clamp(contentWidth * 0.18f, 68.f, 90.f);
            float quickWidth = contentWidth - protocolWidth * 2.f - joinWidth - localWidth -
                               refreshWidth - toolbarGap * 5.f;
            float x = contentX;
            quickConnectField.Bounds = AABB2(x, toolbarY, quickWidth, toolbarHeight);
            x += quickWidth + toolbarGap;
            protocol75Button.Bounds = AABB2(x, toolbarY, protocolWidth, toolbarHeight);
            x += protocolWidth + toolbarGap;
            protocol76Button.Bounds = AABB2(x, toolbarY, protocolWidth, toolbarHeight);
            x += protocolWidth + toolbarGap;
            connectButton.Bounds = AABB2(x, toolbarY, joinWidth, toolbarHeight);
            x += joinWidth + toolbarGap;
            localButton.Bounds = AABB2(x, toolbarY, localWidth, toolbarHeight);
            refreshButton.Bounds =
                AABB2(contentX + contentWidth - refreshWidth, toolbarY, refreshWidth, toolbarHeight);

            float filterY = toolbarY + toolbarHeight + 5.f;
            float filterHeight = 27.f;
            float sourceWidth = Clamp(contentWidth * 0.30f, 125.f, 230.f);
            float versionWidth = Clamp(contentWidth * 0.20f, 75.f, 98.f);
            float playersWidth = Clamp(contentWidth * 0.27f, 105.f, 148.f);
            float searchWidth = contentWidth - sourceWidth - playersWidth - versionWidth - 9.f;
            serverListSourceButton.Bounds = AABB2(contentX, filterY, sourceWidth, filterHeight);
            serverFilterField.Bounds =
                AABB2(contentX + sourceWidth + 3.f, filterY, searchWidth, filterHeight);
            filterPlayersButton.Bounds = AABB2(contentX + contentWidth - playersWidth -
                                                   versionWidth - 3.f,
                                               filterY, playersWidth, filterHeight);
            filterVersionButton.Bounds = AABB2(contentX + contentWidth - versionWidth, filterY,
                                               versionWidth, filterHeight);

            float headerY = filterY + filterHeight + 4.f;
            float headerHeight = 25.f;
            float columnWidth = contentWidth - serverListView.ScrollBarWidth;
            float playersColumnWidth = ServerListPlayersColumnWidth(columnWidth);
            float nameColumnWidth = ServerListNameColumnWidth(columnWidth);
            float mapColumnWidth = ServerListMapColumnWidth(columnWidth);
            float modeColumnWidth = ServerListModeColumnWidth(columnWidth);
            float pingColumnWidth = ServerListPingColumnWidth(columnWidth);
            float nameX = playersColumnWidth;
            float mapX = nameX + nameColumnWidth;
            float modeX = mapX + mapColumnWidth;
            float pingX = modeX + modeColumnWidth;
            serverListPlayersHeader.Bounds =
                AABB2(contentX, headerY, playersColumnWidth, headerHeight);
            serverListNameHeader.Bounds =
                AABB2(contentX + nameX, headerY, nameColumnWidth, headerHeight);
            serverListMapHeader.Bounds =
                AABB2(contentX + mapX, headerY, mapColumnWidth, headerHeight);
            serverListModeHeader.Bounds =
                AABB2(contentX + modeX, headerY, modeColumnWidth, headerHeight);
            serverListPingHeader.Bounds =
                AABB2(contentX + pingX, headerY, pingColumnWidth, headerHeight);

            float listY = headerY + headerHeight;
            float listHeight = margin + fullHeight - contentInset - listY;
            serverListView.Bounds = AABB2(contentX, listY, contentWidth, listHeight);
            serverListLoadingView.Bounds = serverListView.Bounds;
            serverListErrorView.Bounds = serverListView.Bounds;

            spades::ui::UIElement::OnResized();
        }

        private int ConnectProtocol {
            get { return protocol75Button.Toggled ? 3 : 4; }
        }

        private void ProtocolButtonPressed(spades::ui::UIElement @sender) {
            protocol75Button.Toggled = sender is protocol75Button;
            protocol76Button.Toggled = sender is protocol76Button;
            cg_protocolVersion = ConnectProtocol;
        }

        private void QuickConnectChanged(spades::ui::UIElement @sender) {
            cg_lastQuickConnectHost = quickConnectField.Text;
            connectButton.Enable = quickConnectField.Text.length > 0;
        }

        private void ConnectButtonPressed(spades::ui::UIElement @sender) {
            if (quickConnectField.Text.length == 0)
                return;
            cg_lastQuickConnectHost = quickConnectField.Text;
            string result = ui.helper.ConnectServer(quickConnectField.Text, ConnectProtocol);
            if (result.length > 0) {
                AlertScreen al(this, _Tr("MainScreen", "Connection failed") + ":\n\n" + result);
                al.Run();
            }
        }

        private void LocalButtonPressed(spades::ui::UIElement @sender) {
            string result = ui.helper.ConnectServer("127.0.0.1", ConnectProtocol);
            if (result.length > 0) {
                AlertScreen al(this, _Tr("MainScreen", "Connection failed") + ":\n\n" + result);
                al.Run();
            }
        }

        private void ServersButtonPressed(spades::ui::UIElement @sender) {
            serversButton.Toggled = true;
            LoadServerList();
        }

        private void RefreshButtonPressed(spades::ui::UIElement @sender) { LoadServerList(); }

        private void ServerListSourceButtonPressed(spades::ui::UIElement @sender) {
            if (serverListLoading)
                return;
            serverListSource = serverListSource == 0 ? 1 : 0;
            ApplyServerListSource();
            UpdateServerListSourceCaption(-1);
            LoadServerList();
        }

        private void SettingsButtonPressed(spades::ui::UIElement @sender) {
            PreferenceViewOptions options;
            options.InitialTabIndex = 0;
            PreferenceView view(this, options, ui.fontManager);
            view.Run();
        }

        private void ControlsButtonPressed(spades::ui::UIElement @sender) {
            PreferenceViewOptions options;
            options.InitialTabIndex = 1;
            PreferenceView view(this, options, ui.fontManager);
            view.Run();
        }

        private void CreditsButtonPressed(spades::ui::UIElement @sender) {
            AlertScreen credits(this, ui.helper.Credits,
                                Min(500.f, Manager.Renderer.ScreenHeight - 100.f));
            credits.Run();
        }

        private void ExitButtonPressed(spades::ui::UIElement @sender) {
            // Preserve the original main-menu exit semantics: activate once to close the client.
            ui.shouldExit = true;
        }

        void ServerListItemActivated(ServerListModel @sender, MainScreenServerItem @item) {
            @selectedServer = item;
            quickConnectField.Text = item.Address;
            cg_lastQuickConnectHost = item.Address;
            connectButton.Enable = true;
            if (item.Protocol == "0.75") {
                protocol75Button.Toggled = true;
                protocol76Button.Toggled = false;
                cg_protocolVersion = 3;
            } else if (item.Protocol == "0.76") {
                protocol75Button.Toggled = false;
                protocol76Button.Toggled = true;
                cg_protocolVersion = 4;
            }
            quickConnectField.SelectAll();
        }

        void ServerListItemDoubleClicked(ServerListModel @sender, MainScreenServerItem @item) {
            ServerListItemActivated(sender, item);
            ConnectButtonPressed(connectButton);
        }

        void ServerListItemRightClicked(ServerListModel @sender, MainScreenServerItem @item) {
            // Favorite is exposed by the native server item as read-only. Persist the inverse
            // through MainScreenHelper; the next model refresh obtains the updated value.
            ui.helper.SetServerFavorite(item.Address, !item.Favorite);
            UpdateServerList();
        }

        private void SortServerListByPing(spades::ui::UIElement @sender) { SortServerList(0); }
        private void SortServerListByPlayers(spades::ui::UIElement @sender) { SortServerList(1); }
        private void SortServerListByName(spades::ui::UIElement @sender) { SortServerList(2); }
        private void SortServerListByMap(spades::ui::UIElement @sender) { SortServerList(3); }
        private void SortServerListByMode(spades::ui::UIElement @sender) { SortServerList(4); }

        private void SortServerList(int index) {
            if (sortIndex == index)
                sortDescending = !sortDescending;
            else {
                sortIndex = index;
                sortDescending = false;
            }
            cg_serverlistSort = sortIndex | (sortDescending ? 0x4000 : 0);
            UpdateServerList();
        }

        private void FilterChanged(spades::ui::UIElement @sender) { UpdateServerList(); }

        private void UpdateServerList() {
            if (!serverListLoaded || !serverListSuccess)
                return;

            string key;
            switch (sortIndex) {
                case 0: key = "Ping"; break;
                case 1: key = "NumPlayers"; break;
                case 2: key = "Name"; break;
                case 3: key = "MapName"; break;
                case 4: key = "GameMode"; break;
                case 5: key = "Protocol"; break;
                case 6: key = "Country"; break;
            }
            MainScreenServerItem @[] @list = ui.helper.GetServerList(key, sortDescending);
            if (list is null)
                @list = array<spades::MainScreenServerItem @>();

            string filter = serverFilterField.Text;
            int filterPlayers = filterPlayersButton.Index;
            int filterVersion = filterVersionButton.Index;

            MainScreenServerItem @[] @list2 = array<spades::MainScreenServerItem @>();
            for (uint i = 0; i < list.length; i++) {
                MainScreenServerItem @item = list[i];
                bool good = true;
                if (filterVersion == 1 && item.Protocol != "0.75")
                    good = false;
                if (filterVersion == 2 && item.Protocol != "0.76")
                    good = false;
                if (filterPlayers == 1 && item.NumPlayers == 0)
                    good = false;
                if (filterPlayers == 2 && item.NumPlayers >= item.MaxPlayers)
                    good = false;
                if (filterPlayers == 3 &&
                    (item.NumPlayers == 0 || item.NumPlayers >= item.MaxPlayers))
                    good = false;
                if (filter.length > 0 &&
                    !(StringContainsCaseInsensitive(item.Name, filter) ||
                      StringContainsCaseInsensitive(item.MapName, filter) ||
                      StringContainsCaseInsensitive(item.GameMode, filter)))
                    good = false;
                if (good)
                    list2.insertLast(item);
            }
            UpdateServerListSourceCaption(int(list2.length));

            ServerListModel model(Manager, list2);
            @model.ItemActivated = ServerListItemEventHandler(this.ServerListItemActivated);
            @model.ItemDoubleClicked =
                ServerListItemEventHandler(this.ServerListItemDoubleClicked);
            @model.ItemRightClicked = ServerListItemEventHandler(this.ServerListItemRightClicked);
            @serverListView.Model = model;
            lastServerFilter = serverFilterField.Text;
        }

        void LoadServerList() {
            if (serverListLoading)
                return;
            ui.helper.StartQuery();
            serverListLoaded = false;
            serverListLoading = true;
            serverListSuccess = false;
            @serverListView.Model = spades::ui::ListViewModel();
            serverListLoadingView.Visible = true;
            serverListErrorView.Visible = false;
            serverListView.Visible = false;
            serverListSourceButton.Enable = false;
            UpdateServerListSourceCaption(-1);
        }

        void PollServerListState() {
            if (!serverListLoading || serverListLoaded)
                return;
            if (!ui.helper.PollServerListState())
                return;

            @selectedServer = null;
            serverListLoaded = true;
            serverListLoading = false;
            serverListSourceButton.Enable = true;
            MainScreenServerItem @[] @list = ui.helper.GetServerList("", false);
            if (list is null || list.length == 0) {
                serverListSuccess = false;
                serverListView.Visible = false;
                serverListLoadingView.Visible = false;
                serverListErrorView.Visible = true;
            } else {
                serverListSuccess = true;
                serverListView.Visible = true;
                serverListLoadingView.Visible = false;
                serverListErrorView.Visible = false;
                UpdateServerList();
            }
        }

        void HotKey(string key) {
            if (IsEnabled && key == "Enter")
                ConnectButtonPressed(connectButton);
            else if (IsEnabled && key == "Escape")
                ExitButtonPressed(exitButton);
            else
                UIElement::HotKey(key);
        }

        void Render() {
            // UIElement has no per-frame update hook; the original menu also polled its async
            // query from Render(). Keep polling here so the list can actually leave Loading.
            PollServerListState();
            if (serverListLoaded && serverListSuccess &&
                lastServerFilter != serverFilterField.Text) {
                UpdateServerList();
            }

            UIElement::Render();

            if (!IsEnabled)
                return;
            string msg = ui.helper.GetPendingErrorMessage();
            if (msg.length == 0)
                return;

            if (msg.findFirst("Disconnected:") >= 0) {
                int start = msg.findFirst("Disconnected:");
                int finish = msg.findFirst("\n", start);
                if (finish < 0)
                    finish = int(msg.length);
                start += int("Disconnected:".length);
                msg = msg.substr(start, finish - start);
                msg = _Tr("MainScreen",
                          "You were disconnected from the server because of the following "
                          "reason:\n\n{0}",
                          msg);
            }

            AlertScreen al(this, msg);
            al.Run();
        }
    }
}
