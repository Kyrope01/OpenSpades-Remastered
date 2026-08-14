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

    class ModListItem : spades::ui::ButtonBase {
        string Name;
        bool Enabled;

        ModListItem(spades::ui::UIManager @manager, string name, bool enabled) {
            super(manager);
            Name = name;
            Enabled = enabled;
        }

        private string FitText(string text, float width) {
            if (width <= 8.f)
                return "";
            if (Font.Measure(text).x <= width)
                return text;
            string ellipsis = "...";
            if (Font.Measure(ellipsis).x > width)
                return "";
            while (text.length > 0 && Font.Measure(text + ellipsis).x > width)
                text = text.substr(0, text.length - 1);
            return text + ellipsis;
        }

        void Render() {
            Renderer @renderer = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Image @white = renderer.RegisterImage("Gfx/White.tga");

            Vector4 background = Vector4(1.f, 1.f, 1.f, 0.025f);
            Vector4 foreground = Vector4(0.94f, 0.94f, 0.94f, 1.f);
            if (Enabled) {
                background = Vector4(0.52f, 0.40f, 0.04f, 0.42f);
                foreground = Vector4(1.f, 0.88f, 0.18f, 1.f);
            }
            if (Pressed && Hover)
                background.w += 0.30f;
            else if (Hover)
                background.w += 0.15f;

            renderer.ColorNP = background;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, size.y));
            renderer.ColorNP = Enabled ? Vector4(1.f, 0.82f, 0.10f, 0.42f)
                                       : Vector4(1.f, 1.f, 1.f, 0.05f);
            renderer.DrawImage(white, AABB2(pos.x, pos.y + size.y - 1.f, size.x, 1.f));

            string state = Enabled ? _Tr("MainScreen", "Enabled") : "";
            float stateWidth = Enabled ? Font.Measure(state).x + 22.f : 8.f;
            string caption = FitText(Name, Max(1.f, size.x - stateWidth - 20.f));
            Font.Draw(caption, pos + Vector2(10.f, (size.y - Font.Measure(caption).y) * 0.5f),
                      1.f, foreground);
            if (Enabled) {
                Vector2 stateSize = Font.Measure(state);
                Font.Draw(state,
                          pos + Vector2(size.x - stateSize.x - 10.f,
                                        (size.y - stateSize.y) * 0.5f),
                          1.f, foreground);
            }
        }
    }

    funcdef void ModListItemEventHandler(ModListModel @sender, string name);

    class ModListModel : spades::ui::ListViewModel {
        private spades::ui::UIManager @manager;
        private string[] mods;
        private string activeMod;

        ModListItemEventHandler @ItemDoubleClicked;

        ModListModel(spades::ui::UIManager @manager, string[] @mods, string activeMod) {
            @this.manager = manager;
            this.mods = mods;
            this.activeMod = activeMod;
        }

        int NumRows {
            get { return int(mods.length); }
        }

        private void OnItemDoubleClicked(spades::ui::UIElement @sender) {
            ModListItem @item = cast<ModListItem>(sender);
            if (ItemDoubleClicked !is null)
                ItemDoubleClicked(this, item.Name);
        }

        spades::ui::UIElement @CreateElement(int row) {
            ModListItem item(manager, mods[row], mods[row] == activeMod);
            @item.DoubleClicked = spades::ui::EventHandler(this.OnItemDoubleClicked);
            return item;
        }

        void RecycleElement(spades::ui::UIElement @elem) {}
    }
}
