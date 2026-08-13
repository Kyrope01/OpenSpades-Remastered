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
        private PreferenceTabButton @[] sectionButtons;
        private int[] sectionGroups;
        private int[] sectionRows;
        private bool[] groupExpanded;
        private int selectedSectionIndex = -1;
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

            // Expandable/dropdown subcategories. Selecting one also scrolls its existing
            // OpenSpades settings panel to the corresponding heading.
            AddSection(0, _Tr("Preferences", "Player Information"), 0);
            AddSection(0, _Tr("Preferences", "Remastered Features"), 2);
            AddSection(0, _Tr("Preferences", "Effects"), 8);
            AddSection(0, _Tr("Preferences", "Feedbacks"), 15);
            AddSection(0, _Tr("Preferences", "AoS Compatibility"), 19);
            AddSection(0, _Tr("Preferences", "Misc"), 22);

            AddSection(1, _Tr("Preferences", "Weapons/Tools"), 0);
            AddSection(1, _Tr("Preferences", "Movement"), 16);
            AddSection(1, _Tr("Preferences", "Misc"), 25);

            AddSection(2, _Tr("Preferences", "Startup Window"), 0);

            PreferenceTabButton close(Manager);
            close.Caption = _Tr("Preferences", "Back");
            close.IsBackButton = true;
            @close.Activated = spades::ui::EventHandler(this.OnClosePressed);
            AddChild(close);
            @backButton = close;

            SelectedTabIndex = Clamp(options.InitialTabIndex, 0, int(tabs.length) - 1);
            for (uint i = 0; i < groupExpanded.length; i++)
                groupExpanded[i] = int(i) == SelectedTabIndex;
            selectedSectionIndex = FirstSectionInGroup(SelectedTabIndex);

            LayoutContents();
            UpdateTabs();
        }

        private void AddTab(spades::ui::UIElement @view, string caption) {
            PreferenceTab tab(this, view);
            tab.Caption = caption;
            tab.TabButton.Caption = caption;
            tab.View.Visible = false;
            @tab.TabButton.Activated = spades::ui::EventHandler(this.OnGroupButtonActivated);
            AddChild(tab.View);
            AddChild(tab.TabButton);
            tabs.insertLast(tab);
            groupExpanded.insertLast(false);
        }

        private void AddSection(int group, string caption, int row) {
            PreferenceTabButton button(Manager);
            button.Caption = "    " + caption;
            button.IsSectionButton = true;
            button.Toggle = true;
            @button.Activated = spades::ui::EventHandler(this.OnSectionButtonActivated);
            AddChild(button);
            sectionButtons.insertLast(button);
            sectionGroups.insertLast(group);
            sectionRows.insertLast(row);
        }

        private int FirstSectionInGroup(int group) {
            for (uint i = 0; i < sectionButtons.length; i++) {
                if (sectionGroups[i] == group)
                    return int(i);
            }
            return -1;
        }

        private void LayoutContents() {
            ContentsWidth = Max(320.f, Size.x - 12.f);
            ContentsHeight = Max(348.f, Size.y - 12.f);
            float sidebarWidth = Clamp(ContentsWidth * 0.22f, 150.f, 205.f);
            float y = ContentsTop + 6.f;
            float rowHeight = 29.f;
            float rowGap = 3.f;

            for (uint group = 0; group < tabs.length; group++) {
                tabs[group].TabButton.Caption =
                    tabs[group].Caption + (groupExpanded[group] ? "  [-]" : "  [+]");
                tabs[group].TabButton.Bounds =
                    AABB2(ContentsLeft + 5.f, y, sidebarWidth - 10.f, rowHeight);
                y += rowHeight + rowGap;

                for (uint section = 0; section < sectionButtons.length; section++) {
                    if (sectionGroups[section] != int(group))
                        continue;
                    sectionButtons[section].Visible = groupExpanded[group];
                    if (groupExpanded[group]) {
                        sectionButtons[section].Bounds =
                            AABB2(ContentsLeft + 5.f, y, sidebarWidth - 10.f, rowHeight - 2.f);
                        y += rowHeight;
                    }
                }
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

        private void OnGroupButtonActivated(spades::ui::UIElement @sender) {
            for (uint i = 0; i < tabs.length; i++) {
                if (cast<spades::ui::UIElement>(tabs[i].TabButton) !is sender)
                    continue;

                if (SelectedTabIndex == int(i))
                    groupExpanded[i] = !groupExpanded[i];
                else {
                    for (uint group = 0; group < groupExpanded.length; group++)
                        groupExpanded[group] = group == i;
                    SelectedTabIndex = int(i);
                    selectedSectionIndex = FirstSectionInGroup(int(i));
                    ScrollSelectedPanelTo(0);
                }
                LayoutContents();
                UpdateTabs();
                return;
            }
        }

        private void OnSectionButtonActivated(spades::ui::UIElement @sender) {
            for (uint i = 0; i < sectionButtons.length; i++) {
                if (cast<spades::ui::UIElement>(sectionButtons[i]) !is sender)
                    continue;
                SelectedTabIndex = sectionGroups[i];
                selectedSectionIndex = int(i);
                groupExpanded[SelectedTabIndex] = true;
                ScrollSelectedPanelTo(sectionRows[i]);
                UpdateTabs();
                return;
            }
        }

        private void ScrollSelectedPanelTo(int row) {
            if (SelectedTabIndex == 0) {
                GameOptionsPanel @panel = cast<GameOptionsPanel>(tabs[0].View);
                panel.ScrollToSection(row);
            } else if (SelectedTabIndex == 1) {
                ControlOptionsPanel @panel = cast<ControlOptionsPanel>(tabs[1].View);
                panel.ScrollToSection(row);
            }
        }

        private void UpdateTabs() {
            for (uint i = 0; i < tabs.length; i++) {
                bool selected = SelectedTabIndex == int(i);
                tabs[i].TabButton.Toggled = selected;
                tabs[i].View.Visible = selected;
            }
            for (uint i = 0; i < sectionButtons.length; i++)
                sectionButtons[i].Toggled = int(i) == selectedSectionIndex;
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

            renderer.ColorNP = Vector4(0.012f, 0.015f, 0.019f, 0.94f);
            renderer.DrawImage(white,
                               AABB2(pos.x + ContentsLeft, pos.y + ContentsTop, ContentsWidth,
                                     ContentsHeight));
            renderer.ColorNP = Vector4(0.77f, 0.81f, 0.85f, 0.38f);
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
        bool IsSectionButton = false;
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

            Vector4 fill = IsSectionButton ? Vector4(0.12f, 0.13f, 0.15f, 0.72f)
                                           : Vector4(0.31f, 0.33f, 0.35f, 0.78f);
            Vector4 edge = Vector4(1.f, 1.f, 1.f, 0.16f);
            if (IsBackButton) {
                fill = Vector4(0.43f, 0.10f, 0.10f, 0.82f);
                edge = Vector4(1.f, 0.38f, 0.32f, 0.38f);
            } else if (Toggled || (Pressed && Hover)) {
                fill += Vector4(0.19f, 0.19f, 0.19f, 0.08f);
                edge = Vector4(0.70f, 0.84f, 1.f, 0.50f);
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

            float left = IsSectionButton ? 8.f : 10.f;
            Vector2 textSize = Font.Measure(Caption);
            Font.DrawShadow(Caption, pos + Vector2(left, (size.y - textSize.y) * 0.5f), 1.f,
                            Vector4(0.96f, 0.97f, 0.99f, 1.f),
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
        StandardPreferenceLayouterModel(spades::ui::UIElement @[] @items) { @this.items = items; }
        int NumRows {
            get { return int(items.length); }
        }
        spades::ui::UIElement @CreateElement(int row) { return items[row]; }
        void RecycleElement(spades::ui::UIElement @elem) {}
    }
    class StandardPreferenceLayouter {
        spades::ui::UIElement @Parent;
        private float FieldX = 250.f;
        private float FieldWidth = 310.f;
        private spades::ui::UIElement @[] items;
        private ConfigHotKeyField @[] hotkeyItems;
        private FontManager @fontManager;

        StandardPreferenceLayouter(spades::ui::UIElement @parent, FontManager @fontManager) {
            @Parent = parent;
            @this.fontManager = fontManager;

            float availableWidth = Max(340.f, Parent.Manager.Renderer.ScreenWidth - 235.f);
            FieldX = Clamp(availableWidth * 0.42f, 150.f, 330.f);
            FieldWidth = Max(160.f, availableWidth - FieldX - 28.f);
        }

        private spades::ui::UIElement @CreateItem() {
            spades::ui::UIElement elem(Parent.Manager);
            elem.Size = Vector2(FieldX + FieldWidth + 20.f, 32.f);
            items.insertLast(elem);
            return elem;
        }

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

        void AddHeading(string text) {
            spades::ui::UIElement @container = CreateItem();

            spades::ui::Label label(Parent.Manager);
            label.Text = text;
            label.Alignment = Vector2(0.f, 1.f);
            @label.Font = fontManager.HeadingFont;
            label.Bounds = AABB2(10.f, 0.f, FieldX - 20.f, 32.f);
            container.AddChild(label);
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

        spades::ui::ListView @FinishLayout() {
            spades::ui::ListView list(Parent.Manager);
            @list.Model = StandardPreferenceLayouterModel(items);
            list.RowHeight = 32.f;
            list.Bounds = AABB2(0.f, 0.f, Max(580.f, Parent.Size.x), Max(400.f, Parent.Size.y));
            Parent.AddChild(list);
            return list;
        }
    }

    class GameOptionsPanel : spades::ui::UIElement {
        private spades::ui::ListView @listView;

        GameOptionsPanel(spades::ui::UIManager @manager, PreferenceViewOptions @options,
                         FontManager @fontManager) {
            super(manager);

            StandardPreferenceLayouter layouter(this, fontManager);
            layouter.AddHeading(_Tr("Preferences", "Player Information"));
            ConfigField @nameField = layouter.AddInputField(
                _Tr("Preferences", "Player Name"), "cg_playerName", not options.GameActive);
            nameField.MaxLength = 15;
            nameField.DenyNonAscii = true;

            layouter.AddHeading(_Tr("Preferences", "Remastered Features"));
            layouter.AddSliderField(_Tr("Preferences", "Field of View"), "cg_fov", 45, 130, 1,
                                    ConfigNumberFormatter(0, " deg"));
            layouter.AddToggleField(_Tr("Preferences", "Glowing Tracers"), "cg_glowingTracers");
            layouter.AddChoiceField(_Tr("Preferences", "Damage Numbers"), "cg_damageIndicators",
                                    array<string> = {_Tr("Preferences", "OFF"),
                                                     _Tr("Preferences", "ON"),
                                                     _Tr("Preferences", "Grenades")},
                                    array<int> = {0, 1, 2});
            layouter.AddSliderField(_Tr("Preferences", "Tracer Light Intensity"),
                                    "cg_tracerLightIntensity", 0, 4, 0.1,
                                    ConfigNumberFormatter(1, "x"));
            layouter.AddToggleField(_Tr("Preferences", "Filmic Tonemapping"),
                                    "r_filmicToneMapping");

            layouter.AddHeading(_Tr("Preferences", "Effects"));
            layouter.AddToggleField(_Tr("Preferences", "Blood"), "cg_blood");
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

            layouter.AddHeading(_Tr("Preferences", "Feedbacks"));
            layouter.AddToggleField(_Tr("Preferences", "Chat Notify Sounds"), "cg_chatBeep");
            layouter.AddToggleField(_Tr("Preferences", "Hit Indicator"), "cg_hitIndicator");
            layouter.AddToggleField(_Tr("Preferences", "Show Alerts"), "cg_alerts");

            layouter.AddHeading(_Tr("Preferences", "AoS 0.75/0.76 Compatibility"));
            layouter.AddToggleField(_Tr("Preferences", "Allow Unicode"), "cg_unicode");
            layouter.AddToggleField(_Tr("Preferences", "Server Alert"), "cg_serverAlert");

            layouter.AddHeading(_Tr("Preferences", "Misc"));
            layouter.AddSliderField(_Tr("Preferences", "Minimap size"), "cg_minimapSize", 128, 256,
                                    8, ConfigNumberFormatter(0, " px"));
            layouter.AddToggleField(_Tr("Preferences", "Show Statistics"), "cg_stats");
            @listView = layouter.FinishLayout();
        }

        void OnResized() {
            if (listView !is null)
                listView.Bounds = AABB2(0.f, 0.f, Size.x, Size.y);
            UIElement::OnResized();
        }

        void ScrollToSection(int row) { listView.ScrollToRow(row); }
    }

    class ControlOptionsPanel : spades::ui::UIElement {
        private spades::ui::ListView @listView;

        ControlOptionsPanel(spades::ui::UIManager @manager, PreferenceViewOptions @options,
                            FontManager @fontManager) {
            super(manager);

            StandardPreferenceLayouter layouter(this, fontManager);
            layouter.AddHeading(_Tr("Preferences", "Weapons/Tools"));
            layouter.AddControl(_Tr("Preferences", "Attack"), "cg_keyAttack");
            layouter.AddControl(_Tr("Preferences", "Alt. Attack"), "cg_keyAltAttack");
            layouter.AddToggleField(_Tr("Preferences", "Hold Aim Down Sight"),
                                    "cg_holdAimDownSight");
            layouter.AddSliderField(_Tr("Preferences", "Mouse Sensitivity"), "cg_mouseSensitivity",
                                    0.1, 10, 0.1, ConfigNumberFormatter(1, "x"));
            layouter.AddSliderField(_Tr("Preferences", "ADS Mouse Sens. Scale"),
                                    "cg_zoomedMouseSensScale", 0.05, 3, 0.05,
                                    ConfigNumberFormatter(2, "x"));
            layouter.AddSliderField(_Tr("Preferences", "Exponential Power"), "cg_mouseExpPower",
                                    0.5, 1.5, 0.02, ConfigNumberFormatter(2, "", "^"));
            layouter.AddToggleField(_Tr("Preferences", "Invert Y-axis Mouse Input"),
                                    "cg_invertMouseY");
            layouter.AddControl(_Tr("Preferences", "Reload"), "cg_keyReloadWeapon");
            layouter.AddControl(_Tr("Preferences", "Capture Color"), "cg_keyCaptureColor");
            layouter.AddControl(_Tr("Preferences", "Equip Spade"), "cg_keyToolSpade");
            layouter.AddControl(_Tr("Preferences", "Equip Block"), "cg_keyToolBlock");
            layouter.AddControl(_Tr("Preferences", "Equip Weapon"), "cg_keyToolWeapon");
            layouter.AddControl(_Tr("Preferences", "Equip Grenade"), "cg_keyToolGrenade");
            layouter.AddControl(_Tr("Preferences", "Last Used Tool"), "cg_keyLastTool");
            layouter.AddPlusMinusField(_Tr("Preferences", "Switch Tools by Wheel"),
                                       "cg_switchToolByWheel");

            layouter.AddHeading(_Tr("Preferences", "Movement"));
            layouter.AddControl(_Tr("Preferences", "Walk Forward"), "cg_keyMoveForward");
            layouter.AddControl(_Tr("Preferences", "Backpedal"), "cg_keyMoveBackward");
            layouter.AddControl(_Tr("Preferences", "Move Left"), "cg_keyMoveLeft");
            layouter.AddControl(_Tr("Preferences", "Move Right"), "cg_keyMoveRight");
            layouter.AddControl(_Tr("Preferences", "Crouch"), "cg_keyCrouch");
            layouter.AddControl(_Tr("Preferences", "Sneak"), "cg_keySneak");
            layouter.AddControl(_Tr("Preferences", "Jump"), "cg_keyJump");
            layouter.AddControl(_Tr("Preferences", "Sprint"), "cg_keySprint");

            layouter.AddHeading(_Tr("Preferences", "Misc"));
            layouter.AddControl(_Tr("Preferences", "Minimap Scale"), "cg_keyChangeMapScale");
            layouter.AddControl(_Tr("Preferences", "Toggle Map"), "cg_keyToggleMapZoom");
            layouter.AddControl(_Tr("Preferences", "Flashlight"), "cg_keyFlashlight");
            layouter.AddControl(_Tr("Preferences", "Global Chat"), "cg_keyGlobalChat");
            layouter.AddControl(_Tr("Preferences", "Team Chat"), "cg_keyTeamChat");
            layouter.AddControl(_Tr("Preferences", "Limbo Menu"), "cg_keyLimbo");
            layouter.AddControl(_Tr("Preferences", "Save Map"), "cg_keySaveMap");
            layouter.AddControl(_Tr("Preferences", "Save Sceneshot"), "cg_keySceneshot");
            layouter.AddControl(_Tr("Preferences", "Save Screenshot"), "cg_keyScreenshot");

            @listView = layouter.FinishLayout();
        }

        void OnResized() {
            if (listView !is null)
                listView.Bounds = AABB2(0.f, 0.f, Size.x, Size.y);
            UIElement::OnResized();
        }

        void ScrollToSection(int row) { listView.ScrollToRow(row); }
    }

    class MiscOptionsPanel : spades::ui::UIElement {
        spades::ui::Label @msgLabel;
        spades::ui::Button @enableButton;

        private ConfigItem cl_showStartupWindow("cl_showStartupWindow");

        MiscOptionsPanel(spades::ui::UIManager @manager, PreferenceViewOptions @options,
                         FontManager @fontManager) {
            super(manager);

            {
                spades::ui::Button e(Manager);
                e.Bounds = AABB2(10.f, 10.f, 400.f, 30.f);
                e.Caption = _Tr("Preferences", "Enable Startup Window");
                @e.Activated = spades::ui::EventHandler(this.OnEnableClicked);
                AddChild(e);
                @enableButton = e;
            }

            {
                spades::ui::Label label(Manager);
                label.Bounds = AABB2(10.f, 50.f, 0.f, 0.f);
                label.Text = "Hoge";
                AddChild(label);
                @msgLabel = label;
            }

            UpdateState();
        }

        void OnResized() {
            if (enableButton !is null) {
                enableButton.Bounds = AABB2(10.f, 10.f, Min(430.f, Size.x - 20.f), 30.f);
                msgLabel.Bounds = AABB2(12.f, 52.f, Max(100.f, Size.x - 24.f), 32.f);
            }
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
