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

namespace spades {

    class PreferenceViewOptions {
        bool GameActive = false;
        /** 0 = Settings, 1 = Controls, 2 = System. */
        int InitialTabIndex = 0;
    }

    class PreferenceView : spades::ui::UIElement {
        private spades::ui::UIElement @owner;

        private PreferenceTab @[] tabs;
        private PreferenceTabButton @backButton;
        private spades::ui::Label @shade;

        float ContentsLeft, ContentsWidth;
        float ContentsTop, ContentsHeight;
        int SelectedTabIndex = 0;

        spades::ui::EventHandler @Closed;

        PreferenceView(spades::ui::UIElement @owner, PreferenceViewOptions @options,
                       FontManager @fontManager) {
            super(owner.Manager);
            @this.owner = owner;
            this.Bounds = owner.Bounds;

            // Use the same near-full-screen panel hierarchy as the server browser: a narrow
            // navigation rail on the left and one large content panel on the right.
            ContentsLeft = 6.f;
            ContentsTop = 6.f;
            ContentsWidth = Max(320.f, Size.x - 12.f);
            ContentsHeight = Max(348.f, Size.y - 12.f);

            spades::ui::Label backgroundShade(Manager);
            backgroundShade.BackgroundColor = Vector4(0.f, 0.f, 0.f, 0.72f);
            backgroundShade.Bounds = AABB2(0.f, 0.f, Size.x, Size.y);
            AddChild(backgroundShade);
            @shade = backgroundShade;

            AddTab(GameOptionsPanel(Manager, options, fontManager),
                   _Tr("Preferences", "Settings"));
            AddTab(ControlOptionsPanel(Manager, options, fontManager),
                   _Tr("Preferences", "Controls"));
            AddTab(MiscOptionsPanel(Manager, options, fontManager),
                   _Tr("Preferences", "System"));

            PreferenceTabButton close(Manager);
            close.Caption = _Tr("Preferences", "Back");
            close.IsBackButton = true;
            @close.Activated = spades::ui::EventHandler(this.OnClosePressed);
            AddChild(close);
            @backButton = close;

            SelectedTabIndex = Clamp(options.InitialTabIndex, 0, int(tabs.length) - 1);

            LayoutContents();
            UpdateTabs();
        }

        private void AddTab(spades::ui::UIElement @view, string caption) {
            PreferenceTab tab(this, view);
            tab.Caption = caption;
            tab.TabButton.Caption = caption;
            tab.View.Visible = false;
            @tab.TabButton.Activated = spades::ui::EventHandler(this.OnTabButtonActivated);
            AddChild(tab.View);
            AddChild(tab.TabButton);
            tabs.insertLast(tab);
        }

        private void LayoutContents() {
            ContentsWidth = Max(320.f, Size.x - 12.f);
            ContentsHeight = Max(348.f, Size.y - 12.f);
            float sidebarWidth = Clamp(ContentsWidth * 0.22f, 150.f, 205.f);
            float y = ContentsTop + 6.f;
            float rowHeight = 29.f;
            float rowGap = 3.f;

            for (uint i = 0; i < tabs.length; i++) {
                tabs[i].TabButton.Caption = tabs[i].Caption;
                tabs[i].TabButton.Bounds =
                    AABB2(ContentsLeft + 5.f, y, sidebarWidth - 10.f, rowHeight);
                y += rowHeight + rowGap;
            }

            backButton.Bounds = AABB2(ContentsLeft + 5.f,
                                      ContentsTop + ContentsHeight - rowHeight - 6.f,
                                      sidebarWidth - 10.f, rowHeight);

            float viewX = ContentsLeft + sidebarWidth + 6.f;
            float viewWidth = ContentsWidth - sidebarWidth - 11.f;
            for (uint i = 0; i < tabs.length; i++) {
                tabs[i].View.Bounds = AABB2(viewX, ContentsTop + 6.f, viewWidth,
                                            ContentsHeight - 12.f);
            }
        }

        void OnResized() {
            if (shade !is null)
                shade.Bounds = AABB2(0.f, 0.f, Size.x, Size.y);
            if (tabs.length > 0)
                LayoutContents();
            UIElement::OnResized();
        }

        private void OnTabButtonActivated(spades::ui::UIElement @sender) {
            for (uint i = 0; i < tabs.length; i++) {
                if (cast<spades::ui::UIElement>(tabs[i].TabButton) is sender) {
                    SelectedTabIndex = int(i);
                    UpdateTabs();
                    return;
                }
            }
        }

        private void UpdateTabs() {
            for (uint i = 0; i < tabs.length; i++) {
                bool selected = SelectedTabIndex == int(i);
                tabs[i].TabButton.Toggled = selected;
                tabs[i].View.Visible = selected;
            }
        }

        private void OnClosePressed(spades::ui::UIElement @sender) { Close(); }

        private void OnClosed() {
            if (Closed !is null)
                Closed(this);
        }

        void HotKey(string key) {
            if (key == "Escape")
                Close();
            else
                UIElement::HotKey(key);
        }

        void Render() {
            Vector2 pos = ScreenPosition;
            Renderer @renderer = Manager.Renderer;
            Image @white = renderer.RegisterImage("Gfx/White.tga");
            float sidebarWidth = Clamp(ContentsWidth * 0.22f, 150.f, 205.f);

            renderer.ColorNP = Vector4(0.015f, 0.015f, 0.015f, 0.94f);
            renderer.DrawImage(white,
                               AABB2(pos.x + ContentsLeft, pos.y + ContentsTop, ContentsWidth,
                                     ContentsHeight));
            renderer.ColorNP = Vector4(0.81f, 0.81f, 0.81f, 0.38f);
            renderer.DrawImage(white,
                               AABB2(pos.x + ContentsLeft, pos.y + ContentsTop, ContentsWidth, 1.f));
            renderer.DrawImage(white, AABB2(pos.x + ContentsLeft,
                                           pos.y + ContentsTop + ContentsHeight - 1.f,
                                           ContentsWidth, 1.f));
            renderer.DrawImage(white,
                               AABB2(pos.x + ContentsLeft, pos.y + ContentsTop, 1.f,
                                     ContentsHeight));
            renderer.DrawImage(white, AABB2(pos.x + ContentsLeft + ContentsWidth - 1.f,
                                           pos.y + ContentsTop, 1.f, ContentsHeight));
            renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.18f);
            renderer.DrawImage(white, AABB2(pos.x + ContentsLeft + sidebarWidth,
                                           pos.y + ContentsTop + 1.f, 1.f,
                                           ContentsHeight - 2.f));

            UIElement::Render();
        }

        void Close() {
            owner.Enable = true;
            @this.Parent = null;
            OnClosed();
        }

        void Run() {
            owner.Enable = false;
            owner.Parent.AddChild(this);
        }
    }

    class PreferenceTabButton : spades::ui::Button {
        bool IsBackButton = false;

        PreferenceTabButton(spades::ui::UIManager @manager) {
            super(manager);
            Alignment = Vector2(0.f, 0.5f);
        }

        void Render() {
            Renderer @renderer = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Image @white = renderer.RegisterImage("Gfx/White.tga");

            Vector4 fill = Vector4(0.33f, 0.33f, 0.33f, 0.78f);
            Vector4 edge = Vector4(1.f, 1.f, 1.f, 0.16f);
            if (IsBackButton) {
                fill = Vector4(0.27f, 0.27f, 0.27f, 0.82f);
                edge = Vector4(0.57f, 0.57f, 0.57f, 0.38f);
            } else if (Toggled || (Pressed && Hover)) {
                fill += Vector4(0.19f, 0.19f, 0.19f, 0.08f);
                edge = Vector4(0.85f, 0.85f, 0.85f, 0.50f);
            } else if (Hover) {
                fill += Vector4(0.10f, 0.10f, 0.10f, 0.06f);
            }

            renderer.ColorNP = fill;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, size.y));
            renderer.ColorNP = edge;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, 1.f));
            renderer.DrawImage(white, AABB2(pos.x, pos.y + size.y - 1.f, size.x, 1.f));
            renderer.DrawImage(white, AABB2(pos.x, pos.y, 1.f, size.y));
            renderer.DrawImage(white, AABB2(pos.x + size.x - 1.f, pos.y, 1.f, size.y));

            Vector2 textSize = Font.Measure(Caption);
            Font.DrawShadow(Caption, pos + Vector2(10.f, (size.y - textSize.y) * 0.5f), 1.f,
                            Vector4(0.97f, 0.97f, 0.97f, 1.f),
                            Vector4(0.f, 0.f, 0.f, 0.55f));
        }
    }

    class PreferenceTab {
        spades::ui::UIElement @View;
        PreferenceTabButton @TabButton;
        string Caption;

        PreferenceTab(PreferenceView @parent, spades::ui::UIElement @view) {
            @View = view;
            @TabButton = PreferenceTabButton(parent.Manager);
            TabButton.Toggle = true;
        }
    }

    class ConfigField : spades::ui::Field {
        ConfigItem @config;
        ConfigField(spades::ui::UIManager manager, string configName) {
            super(manager);
            @config = ConfigItem(configName);
            this.Text = config.StringValue;
        }

        void OnChanged() {
            Field::OnChanged();
            config = this.Text;
        }
    }

    class ConfigNumberFormatter {
        int digits;
        string suffix;
        string prefix;
        ConfigNumberFormatter(int digits, string suffix) {
            this.digits = digits;
            this.suffix = suffix;
            this.prefix = "";
        }
        ConfigNumberFormatter(int digits, string suffix, string prefix) {
            this.digits = digits;
            this.suffix = suffix;
            this.prefix = prefix;
        }
        private string FormatInternal(float value) {
            if (value < 0.f) {
                return "-" + Format(-value);
            }

            // do rounding
            float rounding = 0.5f;
            for (int i = digits; i > 0; i--)
                rounding *= 0.1f;
            value += rounding;

            int intPart = int(value);
            string s = ToString(intPart);
            if (digits > 0) {
                s += ".";
                for (int i = digits; i > 0; i--) {
                    value -= float(intPart);
                    value *= 10.f;
                    intPart = int(value);
                    if (intPart > 9)
                        intPart = 9;
                    s += ToString(intPart);
                }
            }
            s += suffix;
            return s;
        }
        string Format(float value) { return prefix + FormatInternal(value); }
    }

    class ConfigSlider : spades::ui::Slider {
        ConfigItem @config;
        float stepSize;
        spades::ui::Label @label;
        ConfigNumberFormatter @formatter;

        ConfigSlider(spades::ui::UIManager manager, string configName, float minValue,
                     float maxValue, float stepValue, ConfigNumberFormatter @formatter) {
            super(manager);
            @config = ConfigItem(configName);
            this.MinValue = minValue;
            this.MaxValue = maxValue;
            this.Value = Clamp(config.FloatValue, minValue, maxValue);
            this.stepSize = stepValue;
            @this.formatter = formatter;

            // compute large change
            int steps = int((maxValue - minValue) / stepValue);
            steps = (steps + 9) / 10;
            this.LargeChange = float(steps) * stepValue;

            @label = spades::ui::Label(Manager);
            label.Alignment = Vector2(1.f, 0.5f);
            AddChild(label);
            UpdateLabel();
        }

        void OnResized() {
            Slider::OnResized();
            label.Bounds = AABB2(Size.x, 0.f, 80.f, Size.y);
        }

        void UpdateLabel() { label.Text = formatter.Format(config.FloatValue); }

        void DoRounding() {
            float v = float(this.Value - this.MinValue);
            v = floor((v / stepSize) + 0.5) * stepSize;
            v += float(this.MinValue);
            this.Value = v;
        }

        void OnChanged() {
            Slider::OnChanged();
            DoRounding();
            config = this.Value;
            UpdateLabel();
        }
    }

    uint8 ToUpper(uint8 c) {
        if (c >= uint8(0x61) and c <= uint8(0x7a)) {
            return uint8(c - 0x61 + 0x41);
        } else {
            return c;
        }
    }
    class ConfigHotKeyField : spades::ui::UIElement {
        ConfigItem @config;
        private bool hover;
        spades::ui::EventHandler @KeyBound;

        ConfigHotKeyField(spades::ui::UIManager manager, string configName) {
            super(manager);
            @config = ConfigItem(configName);
            IsMouseInteractive = true;
        }

        string BoundKey {
            get { return config.StringValue; }
            set { config = value; }
        }

        void KeyDown(string key) {
            if (IsFocused) {
                if (key != "Escape") {
                    if (key == " ") {
                        key = "Space";
                    } else if (key == "BackSpace" or key == "Delete") {
                        key = ""; // unbind
                    }
                    config = key;
                    KeyBound(this);
                }
                @Manager.ActiveElement = null;
                AcceptsFocus = false;
            } else {
                UIElement::KeyDown(key);
            }
        }

        void MouseDown(spades::ui::MouseButton button, Vector2 clientPosition) {
            if (not AcceptsFocus) {
                AcceptsFocus = true;
                @Manager.ActiveElement = this;
                return;
            }
            if (IsFocused) {
                if (button == spades::ui::MouseButton::LeftMouseButton) {
                    config = "LeftMouseButton";
                } else if (button == spades::ui::MouseButton::RightMouseButton) {
                    config = "RightMouseButton";
                } else if (button == spades::ui::MouseButton::MiddleMouseButton) {
                    config = "MiddleMouseButton";
                } else if (button == spades::ui::MouseButton::MouseButton4) {
                    config = "MouseButton4";
                } else if (button == spades::ui::MouseButton::MouseButton5) {
                    config = "MouseButton5";
                }
                KeyBound(this);
                @Manager.ActiveElement = null;
                AcceptsFocus = false;
            }
        }

        void MouseEnter() { hover = true; }
        void MouseLeave() { hover = false; }

        void Render() {
            // render background
            Renderer @renderer = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Image @img = renderer.RegisterImage("Gfx/White.tga");
            renderer.ColorNP = Vector4(0.f, 0.f, 0.f, IsFocused ? 0.3f : 0.1f);
            renderer.DrawImage(img, AABB2(pos.x, pos.y, size.x, size.y));

            if (IsFocused) {
                renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.2f);
            } else if (hover) {
                renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.1f);
            } else {
                renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.06f);
            }
            renderer.DrawImage(img, AABB2(pos.x, pos.y, size.x, 1.f));
            renderer.DrawImage(img, AABB2(pos.x, pos.y + size.y - 1.f, size.x, 1.f));
            renderer.DrawImage(img, AABB2(pos.x, pos.y + 1.f, 1.f, size.y - 2.f));
            renderer.DrawImage(img, AABB2(pos.x + size.x - 1.f, pos.y + 1.f, 1.f, size.y - 2.f));

            Font @font = this.Font;
            string text = IsFocused
                              ? _Tr("Preferences", "Press Key to Bind or [Escape] to Cancel...")
                              : config.StringValue;

            Vector4 color(1, 1, 1, 1);

            if (IsFocused) {
                color.w = abs(sin(Manager.Time * 2.f));
            } else {
                AcceptsFocus = false;
            }

            if (text == " " or text == "Space") {
                text = _Tr("Preferences", "Space");
            } else if (text.length == 0) {
                text = _Tr("Preferences", "Unbound");
                color.w *= 0.3f;
            } else if (text == "Shift") {
                text = _Tr("Preferences", "Shift");
            } else if (text == "Control") {
                text = _Tr("Preferences", "Control");
            } else if (text == "Meta") {
                text = _Tr("Preferences", "Meta");
            } else if (text == "Alt") {
                text = _Tr("Preferences", "Alt");
            } else if (text == "LeftMouseButton") {
                text = _Tr("Preferences", "Left Mouse Button");
            } else if (text == "RightMouseButton") {
                text = _Tr("Preferences", "Right Mouse Button");
            } else if (text == "MiddleMouseButton") {
                text = _Tr("Preferences", "Middle Mouse Button");
            } else if (text == "MouseButton4") {
                text = _Tr("Preferences", "Mouse Button 4");
            } else if (text == "MouseButton5") {
                text = _Tr("Preferences", "Mouse Button 5");
            } else {
                for (uint i = 0, len = text.length; i < len; i++)
                    text[i] = ToUpper(text[i]);
            }

            Vector2 txtSize = font.Measure(text);
            Vector2 txtPos;
            txtPos = pos + (size - txtSize) * 0.5f;

            font.Draw(text, txtPos, 1.f, color);
        }
    }

    class ConfigSimpleToggleButton : spades::ui::RadioButton {
        ConfigItem @config;
        int value;
        ConfigSimpleToggleButton(spades::ui::UIManager manager, string caption, string configName,
                                 int value) {
            super(manager);
            @config = ConfigItem(configName);
            this.Caption = caption;
            this.value = value;
            this.Toggle = true;
            this.Toggled = config.IntValue == value;
        }

        void OnActivated() {
            RadioButton::OnActivated();
            this.Toggled = true;
            config = value;
        }

        void Render() {
            this.Toggled = config.IntValue == value;
            RadioButton::Render();
        }
    }

    class StandardPreferenceLayouterModel : spades::ui::ListViewModel {
        private spades::ui::UIElement @[] @items;
        private int[] @categories;
        private int[] @mainCategories;
        private bool[] @headingItems;
        private bool[] @expandedCategories;
        private spades::ui::UIElement @[] visibleItems;
        private spades::ui::ListView @listView;
        private int selectedMainCategory = 0;

        StandardPreferenceLayouterModel(spades::ui::UIElement @[] @items, int[] @categories,
                                        int[] @mainCategories, bool[] @headingItems,
                                        bool[] @expandedCategories) {
            @this.items = items;
            @this.categories = categories;
            @this.mainCategories = mainCategories;
            @this.headingItems = headingItems;
            @this.expandedCategories = expandedCategories;
            Rebuild();
        }

        private void Rebuild() {
            visibleItems.resize(0);
            for (uint i = 0; i < items.length; i++) {
                if (mainCategories[i] != selectedMainCategory)
                    continue;
                int category = categories[i];
                if (headingItems[i] or expandedCategories[category])
                    visibleItems.insertLast(items[i]);
            }
        }

        void AttachList(spades::ui::ListView @list) { @listView = list; }

        bool IsMainCategorySelected(int category) const {
            return selectedMainCategory == category;
        }

        void SelectMainCategory(int category) {
            if (selectedMainCategory == category)
                return;
            selectedMainCategory = category;
            Rebuild();
            if (listView !is null) {
                listView.Reload();
                listView.ScrollToTop();
            }
        }

        bool IsCategoryExpanded(int category) const {
            return expandedCategories[category];
        }

        void ToggleCategory(int category) {
            expandedCategories[category] = not expandedCategories[category];
            Rebuild();
            if (listView !is null)
                listView.Reload();
        }

        int NumRows {
            get { return int(visibleItems.length); }
        }
        spades::ui::UIElement @CreateElement(int row) { return visibleItems[row]; }
        void RecycleElement(spades::ui::UIElement @elem) {}
    }

    /** A KyroSpades-style treenode row which expands or collapses its settings in-place. */
    class PreferenceCategoryButton : spades::ui::Button {
        private StandardPreferenceLayouterModel @model;
        private int category;
        private string text;

        PreferenceCategoryButton(spades::ui::UIManager @manager, string text, int category,
                                 FontManager @fontManager) {
            super(manager);
            this.text = text;
            this.category = category;
            Alignment = Vector2(0.f, 0.5f);
            @Font = fontManager.HeadingFont;
        }

        void SetModel(StandardPreferenceLayouterModel @model) { @this.model = model; }

        void OnActivated() {
            spades::ui::Button::OnActivated();
            if (model !is null)
                model.ToggleCategory(category);
        }

        void Render() {
            Renderer @renderer = Manager.Renderer;
            Image @white = renderer.RegisterImage("Gfx/White.tga");
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            bool expanded = model is null or model.IsCategoryExpanded(category);

            Vector4 fill = Vector4(0.12f, 0.12f, 0.12f, 0.88f);
            if ((Pressed and Hover) or Toggled)
                fill = Vector4(0.24f, 0.24f, 0.24f, 0.92f);
            else if (Hover)
                fill = Vector4(0.19f, 0.19f, 0.19f, 0.91f);
            renderer.ColorNP = fill;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, size.y));

            renderer.ColorNP = Vector4(0.72f, 0.72f, 0.72f, Hover ? 0.90f : 0.62f);
            renderer.DrawImage(white, AABB2(pos.x, pos.y, 3.f, size.y));
            renderer.DrawImage(white, AABB2(pos.x + 3.f, pos.y + size.y - 1.f,
                                           size.x - 3.f, 1.f));

            string caption = (expanded ? "[-]  " : "[+]  ") + text;
            Vector2 textSize = Font.Measure(caption);
            Font.DrawShadow(caption, pos + Vector2(12.f, (size.y - textSize.y) * 0.5f), 1.f,
                            Vector4(0.97f, 0.97f, 0.97f, 1.f),
                            Vector4(0.f, 0.f, 0.f, 0.65f));
        }
    }

    /** One entry in the fixed main-category column beside the settings list. */
    class PreferenceMainCategoryButton : spades::ui::Button {
        private StandardPreferenceLayouterModel @model;
        private int category;

        PreferenceMainCategoryButton(spades::ui::UIManager @manager, string caption, int category) {
            super(manager);
            Caption = caption;
            this.category = category;
            Alignment = Vector2(0.f, 0.5f);
        }

        void SetModel(StandardPreferenceLayouterModel @model) { @this.model = model; }

        void OnActivated() {
            spades::ui::Button::OnActivated();
            if (model !is null)
                model.SelectMainCategory(category);
        }

        void Render() {
            Renderer @renderer = Manager.Renderer;
            Image @white = renderer.RegisterImage("Gfx/White.tga");
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            bool selected = model !is null and model.IsMainCategorySelected(category);

            Vector4 fill = Vector4(0.12f, 0.12f, 0.12f, 0.88f);
            Vector4 edge = Vector4(0.65f, 0.65f, 0.65f, 0.28f);
            if (selected) {
                fill = Vector4(0.38f, 0.38f, 0.38f, 0.92f);
                edge = Vector4(0.90f, 0.90f, 0.90f, 0.62f);
            } else if (Pressed and Hover) {
                fill = Vector4(0.29f, 0.29f, 0.29f, 0.92f);
            } else if (Hover) {
                fill = Vector4(0.21f, 0.21f, 0.21f, 0.91f);
            }

            renderer.ColorNP = fill;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, size.y));
            renderer.ColorNP = edge;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, selected ? 3.f : 1.f, size.y));
            renderer.DrawImage(white, AABB2(pos.x, pos.y + size.y - 1.f, size.x, 1.f));

            Vector2 textSize = Font.Measure(Caption);
            Font.DrawShadow(Caption, pos + Vector2(10.f, (size.y - textSize.y) * 0.5f), 1.f,
                            Vector4(0.97f, 0.97f, 0.97f, 1.f),
                            Vector4(0.f, 0.f, 0.f, 0.60f));
        }
    }

    /** The KyroSpades-style category column and right-hand dropdown settings panel. */
    class PreferenceSettingsLayout : spades::ui::UIElement {
        private spades::ui::ListView @listView;
        private PreferenceMainCategoryButton @[] categoryButtons;

        PreferenceSettingsLayout(spades::ui::UIManager @manager, spades::ui::ListView @list,
                                 StandardPreferenceLayouterModel @model, string[] categoryNames) {
            super(manager);
            @listView = list;

            for (uint i = 0; i < categoryNames.length; i++) {
                PreferenceMainCategoryButton button(Manager, categoryNames[i], int(i));
                button.SetModel(model);
                AddChild(button);
                categoryButtons.insertLast(button);
            }
            AddChild(listView);
        }

        void OnResized() {
            float categoryWidth = Clamp(Size.x * 0.20f, 120.f, 170.f);
            float rowHeight = 29.f;
            float rowGap = 3.f;
            float inset = 4.f;
            float y = inset;

            for (uint i = 0; i < categoryButtons.length; i++) {
                categoryButtons[i].Bounds =
                    AABB2(inset, y, categoryWidth - inset * 2.f, rowHeight);
                y += rowHeight + rowGap;
            }

            float listX = categoryWidth + 6.f;
            listView.Bounds = AABB2(listX, 0.f, Max(1.f, Size.x - listX), Size.y);
            spades::ui::UIElement::OnResized();
        }

        void Render() {
            Renderer @renderer = Manager.Renderer;
            Image @white = renderer.RegisterImage("Gfx/White.tga");
            Vector2 pos = ScreenPosition;
            float categoryWidth = Clamp(Size.x * 0.20f, 120.f, 170.f);

            renderer.ColorNP = Vector4(0.04f, 0.04f, 0.04f, 0.88f);
            renderer.DrawImage(white, AABB2(pos.x, pos.y, categoryWidth, Size.y));
            renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.16f);
            renderer.DrawImage(white,
                               AABB2(pos.x + categoryWidth - 1.f, pos.y, 1.f, Size.y));

            spades::ui::UIElement::Render();
        }

        void ScrollToRow(int row) { listView.ScrollToRow(row); }
    }

    class StandardPreferenceLayouter {
        spades::ui::UIElement @Parent;
        private float FieldX = 250.f;
        private float FieldWidth = 310.f;
        private spades::ui::UIElement @[] items;
        private int[] itemCategories;
        private int[] itemMainCategories;
        private bool[] headingItems;
        private bool[] expandedCategories;
        private PreferenceCategoryButton @[] categoryButtons;
        private string[] mainCategoryNames;
        private int currentCategory = -1;
        private int currentMainCategory = -1;
        private ConfigHotKeyField @[] hotkeyItems;
        private FontManager @fontManager;

        StandardPreferenceLayouter(spades::ui::UIElement @parent, FontManager @fontManager) {
            @Parent = parent;
            @this.fontManager = fontManager;

            // Leave room for both the outer navigation rail and the inner main-category column.
            float availableWidth = Max(320.f, Parent.Manager.Renderer.ScreenWidth - 400.f);
            FieldX = Clamp(availableWidth * 0.42f, 135.f, 330.f);
            FieldWidth = Max(155.f, availableWidth - FieldX - 28.f);
        }

        private spades::ui::UIElement @CreateItem() {
            spades::ui::UIElement elem(Parent.Manager);
            elem.Size = Vector2(FieldX + FieldWidth + 20.f, 32.f);
            items.insertLast(elem);
            itemCategories.insertLast(currentCategory);
            itemMainCategories.insertLast(currentMainCategory);
            headingItems.insertLast(false);
            return elem;
        }

        spades::ui::UIElement @AddCustomItem() { return CreateItem(); }

        private void OnKeyBound(spades::ui::UIElement @sender) {
            // unbind already bound key
            ConfigHotKeyField @bindField = cast<ConfigHotKeyField>(sender);
            string key = bindField.BoundKey;
            for (uint i = 0; i < hotkeyItems.length; i++) {
                ConfigHotKeyField @f = hotkeyItems[i];
                if (f !is bindField) {
                    if (f.BoundKey == key) {
                        f.BoundKey = "";
                    }
                }
            }
        }

        void AddMainCategory(string text) {
            mainCategoryNames.insertLast(text);
            currentMainCategory = int(mainCategoryNames.length) - 1;
        }

        void AddHeading(string text) {
            currentCategory++;
            expandedCategories.insertLast(true);

            PreferenceCategoryButton button(Parent.Manager, text, currentCategory, fontManager);
            button.Size = Vector2(FieldX + FieldWidth + 20.f, 32.f);
            items.insertLast(button);
            itemCategories.insertLast(currentCategory);
            itemMainCategories.insertLast(currentMainCategory);
            headingItems.insertLast(true);
            categoryButtons.insertLast(button);
        }

        ConfigField @AddInputField(string caption, string configName, bool enabled = true) {
            spades::ui::UIElement @container = CreateItem();

            spades::ui::Label label(Parent.Manager);
            label.Text = caption;
            label.Alignment = Vector2(0.f, 0.5f);
            label.Bounds = AABB2(10.f, 0.f, FieldX - 20.f, 32.f);
            container.AddChild(label);

            ConfigField field(Parent.Manager, configName);
            field.Bounds = AABB2(FieldX, 1.f, FieldWidth, 30.f);
            field.Enable = enabled;
            container.AddChild(field);

            return field;
        }

        ConfigSlider
            @AddSliderField(string caption, string configName, float minRange, float maxRange,
                            float step, ConfigNumberFormatter @formatter, bool enabled = true) {
            spades::ui::UIElement @container = CreateItem();

            spades::ui::Label label(Parent.Manager);
            label.Text = caption;
            label.Alignment = Vector2(0.f, 0.5f);
            label.Bounds = AABB2(10.f, 0.f, FieldX - 20.f, 32.f);
            container.AddChild(label);

            ConfigSlider slider(Parent.Manager, configName, minRange, maxRange, step, formatter);
            slider.Bounds = AABB2(FieldX, 8.f, FieldWidth - 80.f, 16.f);
            slider.Enable = enabled;
            container.AddChild(slider);

            return slider;
        }

        void AddControl(string caption, string configName, bool enabled = true) {
            spades::ui::UIElement @container = CreateItem();

            spades::ui::Label label(Parent.Manager);
            label.Text = caption;
            label.Alignment = Vector2(0.f, 0.5f);
            label.Bounds = AABB2(10.f, 0.f, FieldX - 20.f, 32.f);
            container.AddChild(label);

            ConfigHotKeyField field(Parent.Manager, configName);
            field.Bounds = AABB2(FieldX, 1.f, FieldWidth, 30.f);
            field.Enable = enabled;
            container.AddChild(field);

            @field.KeyBound = spades::ui::EventHandler(OnKeyBound);
            hotkeyItems.insertLast(field);
        }

        void AddChoiceField(string caption, string configName, array<string> labels,
                            array<int> values, bool enabled = true) {
            spades::ui::UIElement @container = CreateItem();

            spades::ui::Label label(Parent.Manager);
            label.Text = caption;
            label.Alignment = Vector2(0.f, 0.5f);
            label.Bounds = AABB2(10.f, 0.f, FieldX - 20.f, 32.f);
            container.AddChild(label);

            for (uint i = 0; i < labels.length; ++i) {
                ConfigSimpleToggleButton field(Parent.Manager, labels[i], configName, values[i]);
                field.Bounds = AABB2(FieldX + FieldWidth / labels.length * i, 1.f,
                                     FieldWidth / labels.length, 30.f);
                field.Enable = enabled;
                container.AddChild(field);
            }
        }

        void AddToggleField(string caption, string configName, bool enabled = true) {
            AddChoiceField(caption, configName,
                           array<string> = {_Tr("Preferences", "ON"), _Tr("Preferences", "OFF")},
                           array<int> = {1, 0}, enabled);
        }

        void AddPlusMinusField(string caption, string configName, bool enabled = true) {
            AddChoiceField(caption, configName,
                           array<string> = {_Tr("Preferences", "ON"),
                                            _Tr("Preferences", "REVERSED"),
                                            _Tr("Preferences", "OFF")},
                           array<int> = {1, -1, 0}, enabled);
        }

        PreferenceSettingsLayout @FinishLayout() {
            spades::ui::ListView list(Parent.Manager);
            StandardPreferenceLayouterModel model(items, itemCategories, itemMainCategories,
                                                  headingItems, expandedCategories);
            for (uint i = 0; i < categoryButtons.length; i++)
                categoryButtons[i].SetModel(model);
            @list.Model = model;
            model.AttachList(list);
            list.RowHeight = 32.f;

            PreferenceSettingsLayout layout(Parent.Manager, list, model, mainCategoryNames);
            layout.Bounds = AABB2(0.f, 0.f, Parent.Size.x, Parent.Size.y);
            Parent.AddChild(layout);
            return layout;
        }
    }

    class GameOptionsPanel : spades::ui::UIElement {
        private PreferenceSettingsLayout @settingsLayout;
        private ConfigItem cg_damageIndicators("cg_damageIndicators");

        GameOptionsPanel(spades::ui::UIManager @manager, PreferenceViewOptions @options,
                         FontManager @fontManager) {
            super(manager);

            StandardPreferenceLayouter layouter(this, fontManager);

            layouter.AddMainCategory(_Tr("Preferences", "General"));
            layouter.AddHeading(_Tr("Preferences", "Player Information"));
            ConfigField @nameField = layouter.AddInputField(
                _Tr("Preferences", "Player Name"), "cg_playerName", not options.GameActive);
            nameField.MaxLength = 15;
            nameField.DenyNonAscii = true;

            layouter.AddHeading(_Tr("Preferences", "AoS 0.75/0.76 Compatibility"));
            layouter.AddToggleField(_Tr("Preferences", "Allow Unicode"), "cg_unicode");
            layouter.AddToggleField(_Tr("Preferences", "Server Alert"), "cg_serverAlert");

            layouter.AddMainCategory(_Tr("Preferences", "Graphics"));
            layouter.AddHeading(_Tr("Preferences", "View"));
            layouter.AddSliderField(_Tr("Preferences", "Field of View"), "cg_fov", 45, 130, 1,
                                    ConfigNumberFormatter(0, " deg"));
            layouter.AddToggleField(_Tr("Preferences", "Filmic Tonemapping"),
                                    "r_filmicToneMapping");
            layouter.AddSliderField(_Tr("Preferences", "Sharpening"), "r_sharpen", 0, 1, 0.1,
                                    ConfigNumberFormatter(1, ""));

            layouter.AddHeading(_Tr("Preferences", "Visual Effects"));
            layouter.AddToggleField(_Tr("Preferences", "Blood"), "cg_blood");
            layouter.AddToggleField(_Tr("Preferences", "Terrain Blood Marks"), "cg_bloodMarks");
            layouter.AddToggleField(_Tr("Preferences", "Ejecting Brass"), "cg_ejectBrass");
            layouter.AddToggleField(_Tr("Preferences", "Ragdoll"), "cg_ragdoll");
            layouter.AddToggleField(_Tr("Preferences", "Animations"), "cg_animations");
            layouter.AddChoiceField(_Tr("Preferences", "Camera Shake"), "cg_shake",
                                    array<string> = {_Tr("Preferences", "MORE"),
                                                     _Tr("Preferences", "NORMAL"),
                                                     _Tr("Preferences", "OFF")},
                                    array<int> = {2, 1, 0});
            layouter.AddChoiceField(_Tr("Preferences", "Particles"), "cg_particles",
                                    array<string> = {_Tr("Preferences", "NORMAL"),
                                                     _Tr("Preferences", "LESS"),
                                                     _Tr("Preferences", "OFF")},
                                    array<int> = {2, 1, 0});

            layouter.AddHeading(_Tr("Preferences", "Dynamic Lighting"));
            layouter.AddToggleField(_Tr("Preferences", "Glowing Tracers"), "cg_glowingTracers");
            layouter.AddSliderField(_Tr("Preferences", "Tracer Light Intensity"),
                                    "cg_tracerLightIntensity", 0, 4, 0.1,
                                    ConfigNumberFormatter(1, "x"));

            layouter.AddMainCategory(_Tr("Preferences", "HUD/UI"));
            layouter.AddHeading(_Tr("Preferences", "Combat Feedback"));
            layouter.AddToggleField(_Tr("Preferences", "Hit Indicator"), "cg_hitIndicator");
            // Damage Numbers is a normal on/off feature in this client. Migrate the old
            // grenade-inclusive value so the retained two-button setting always has a selected
            // state.
            if (cg_damageIndicators.IntValue > 1)
                cg_damageIndicators = 1;
            layouter.AddToggleField(_Tr("Preferences", "Damage Numbers"), "cg_damageIndicators");

            layouter.AddHeading(_Tr("Preferences", "Notifications"));
            layouter.AddToggleField(_Tr("Preferences", "Chat Notify Sounds"), "cg_chatBeep");
            layouter.AddToggleField(_Tr("Preferences", "Show Alerts"), "cg_alerts");

            layouter.AddHeading(_Tr("Preferences", "HUD"));
            layouter.AddSliderField(_Tr("Preferences", "Minimap size"), "cg_minimapSize", 128, 256,
                                    8, ConfigNumberFormatter(0, " px"));
            layouter.AddToggleField(_Tr("Preferences", "Show Statistics"), "cg_stats");

            @settingsLayout = layouter.FinishLayout();
        }

        void OnResized() {
            if (settingsLayout !is null)
                settingsLayout.Bounds = AABB2(0.f, 0.f, Size.x, Size.y);
            UIElement::OnResized();
        }

        void ScrollToSection(int row) { settingsLayout.ScrollToRow(row); }
    }

    class ControlOptionsPanel : spades::ui::UIElement {
        private PreferenceSettingsLayout @settingsLayout;

        ControlOptionsPanel(spades::ui::UIManager @manager, PreferenceViewOptions @options,
                            FontManager @fontManager) {
            super(manager);

            StandardPreferenceLayouter layouter(this, fontManager);

            layouter.AddMainCategory(_Tr("Preferences", "Weapons/Tools"));
            layouter.AddHeading(_Tr("Preferences", "Combat"));
            layouter.AddControl(_Tr("Preferences", "Attack"), "cg_keyAttack");
            layouter.AddControl(_Tr("Preferences", "Alt. Attack"), "cg_keyAltAttack");
            layouter.AddToggleField(_Tr("Preferences", "Hold Aim Down Sight"),
                                    "cg_holdAimDownSight");
            layouter.AddControl(_Tr("Preferences", "Reload"), "cg_keyReloadWeapon");

            layouter.AddHeading(_Tr("Preferences", "Equipment"));
            layouter.AddControl(_Tr("Preferences", "Capture Color"), "cg_keyCaptureColor");
            layouter.AddControl(_Tr("Preferences", "Equip Spade"), "cg_keyToolSpade");
            layouter.AddControl(_Tr("Preferences", "Equip Block"), "cg_keyToolBlock");
            layouter.AddControl(_Tr("Preferences", "Equip Weapon"), "cg_keyToolWeapon");
            layouter.AddControl(_Tr("Preferences", "Equip Grenade"), "cg_keyToolGrenade");
            layouter.AddControl(_Tr("Preferences", "Last Used Tool"), "cg_keyLastTool");
            layouter.AddPlusMinusField(_Tr("Preferences", "Switch Tools by Wheel"),
                                       "cg_switchToolByWheel");

            layouter.AddMainCategory(_Tr("Preferences", "Mouse"));
            layouter.AddHeading(_Tr("Preferences", "Aiming"));
            layouter.AddSliderField(_Tr("Preferences", "Mouse Sensitivity"), "cg_mouseSensitivity",
                                    0.1, 10, 0.1, ConfigNumberFormatter(1, "x"));
            layouter.AddSliderField(_Tr("Preferences", "ADS Mouse Sens. Scale"),
                                    "cg_zoomedMouseSensScale", 0.05, 3, 0.05,
                                    ConfigNumberFormatter(2, "x"));
            layouter.AddSliderField(_Tr("Preferences", "Exponential Power"), "cg_mouseExpPower",
                                    0.5, 1.5, 0.02, ConfigNumberFormatter(2, "", "^"));
            layouter.AddToggleField(_Tr("Preferences", "Invert Y-axis Mouse Input"),
                                    "cg_invertMouseY");

            layouter.AddMainCategory(_Tr("Preferences", "Movement"));
            layouter.AddHeading(_Tr("Preferences", "Movement Keys"));
            layouter.AddControl(_Tr("Preferences", "Walk Forward"), "cg_keyMoveForward");
            layouter.AddControl(_Tr("Preferences", "Backpedal"), "cg_keyMoveBackward");
            layouter.AddControl(_Tr("Preferences", "Move Left"), "cg_keyMoveLeft");
            layouter.AddControl(_Tr("Preferences", "Move Right"), "cg_keyMoveRight");
            layouter.AddControl(_Tr("Preferences", "Crouch"), "cg_keyCrouch");
            layouter.AddControl(_Tr("Preferences", "Sneak"), "cg_keySneak");
            layouter.AddControl(_Tr("Preferences", "Jump"), "cg_keyJump");
            layouter.AddControl(_Tr("Preferences", "Sprint"), "cg_keySprint");

            layouter.AddMainCategory(_Tr("Preferences", "Interface"));
            layouter.AddHeading(_Tr("Preferences", "Map and Menus"));
            layouter.AddControl(_Tr("Preferences", "Minimap Scale"), "cg_keyChangeMapScale");
            layouter.AddControl(_Tr("Preferences", "Toggle Map"), "cg_keyToggleMapZoom");
            layouter.AddControl(_Tr("Preferences", "Limbo Menu"), "cg_keyLimbo");

            layouter.AddHeading(_Tr("Preferences", "Communication"));
            layouter.AddControl(_Tr("Preferences", "Global Chat"), "cg_keyGlobalChat");
            layouter.AddControl(_Tr("Preferences", "Team Chat"), "cg_keyTeamChat");

            layouter.AddHeading(_Tr("Preferences", "Utilities"));
            layouter.AddControl(_Tr("Preferences", "Flashlight"), "cg_keyFlashlight");
            layouter.AddControl(_Tr("Preferences", "Save Map"), "cg_keySaveMap");
            layouter.AddControl(_Tr("Preferences", "Save Sceneshot"), "cg_keySceneshot");
            layouter.AddControl(_Tr("Preferences", "Save Screenshot"), "cg_keyScreenshot");

            @settingsLayout = layouter.FinishLayout();
        }

        void OnResized() {
            if (settingsLayout !is null)
                settingsLayout.Bounds = AABB2(0.f, 0.f, Size.x, Size.y);
            UIElement::OnResized();
        }

        void ScrollToSection(int row) { settingsLayout.ScrollToRow(row); }
    }

    class MiscOptionsPanel : spades::ui::UIElement {
        private PreferenceSettingsLayout @settingsLayout;
        private spades::ui::Label @msgLabel;
        private spades::ui::Button @enableButton;

        private ConfigItem cl_showStartupWindow("cl_showStartupWindow");

        MiscOptionsPanel(spades::ui::UIManager @manager, PreferenceViewOptions @options,
                         FontManager @fontManager) {
            super(manager);

            StandardPreferenceLayouter layouter(this, fontManager);
            layouter.AddMainCategory(_Tr("Preferences", "General"));
            layouter.AddHeading(_Tr("Preferences", "Startup Window"));

            {
                spades::ui::UIElement @container = layouter.AddCustomItem();
                spades::ui::Button e(Manager);
                e.Bounds = AABB2(10.f, 1.f, Max(200.f, container.Size.x - 24.f), 30.f);
                e.Caption = _Tr("Preferences", "Enable Startup Window");
                @e.Activated = spades::ui::EventHandler(this.OnEnableClicked);
                container.AddChild(e);
                @enableButton = e;
            }

            {
                spades::ui::UIElement @container = layouter.AddCustomItem();
                spades::ui::Label label(Manager);
                label.Bounds = AABB2(12.f, 0.f, Max(200.f, container.Size.x - 24.f), 32.f);
                label.Text = "";
                container.AddChild(label);
                @msgLabel = label;
            }

            @settingsLayout = layouter.FinishLayout();
            UpdateState();
        }

        void OnResized() {
            if (settingsLayout !is null)
                settingsLayout.Bounds = AABB2(0.f, 0.f, Size.x, Size.y);
            UIElement::OnResized();
        }

        void UpdateState() {
            bool enabled = cl_showStartupWindow.IntValue != 0;

            msgLabel.Text = enabled
                                ? _Tr("Preferences",
                                      "Quit and restart OpenSpades to access the startup window.")
                                : _Tr("Preferences",
                                      "Some settings only can be changed in the startup window.");
            enableButton.Enable = not enabled;
        }

        private void OnEnableClicked(spades::ui::UIElement @) {
            cl_showStartupWindow.IntValue = 1;
            UpdateState();
        }
    }
}
