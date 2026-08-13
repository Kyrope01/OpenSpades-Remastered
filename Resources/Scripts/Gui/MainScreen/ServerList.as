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

#include "CountryFlags.as"

namespace spades {

    // Shared responsive column geometry keeps row contents and all five sort buttons aligned.
    // Widths match KyroSpades' proportions at normal resolutions while retaining readable minimums
    // when possible. Scaling those minimums together also guarantees that columns never overflow.
    float ServerListMinimumColumnScale(float width) {
        return Clamp(width / 285.f, 0.f, 1.f);
    }

    float ServerListPlayersColumnWidth(float width) {
        return Clamp(width * 0.12f, 70.f, 125.f) * ServerListMinimumColumnScale(width);
    }

    float ServerListMapColumnWidth(float width) {
        return Clamp(width * 0.22f, 70.f, 220.f) * ServerListMinimumColumnScale(width);
    }

    float ServerListModeColumnWidth(float width) {
        return Clamp(width * 0.117f, 55.f, 125.f) * ServerListMinimumColumnScale(width);
    }

    float ServerListPingColumnWidth(float width) {
        return Clamp(width * 0.125f, 50.f, 110.f) * ServerListMinimumColumnScale(width);
    }

    float ServerListNameColumnWidth(float width) {
        float scale = ServerListMinimumColumnScale(width);
        return Max(40.f * scale, width - ServerListPlayersColumnWidth(width) -
                                        ServerListMapColumnWidth(width) -
                                        ServerListModeColumnWidth(width) -
                                        ServerListPingColumnWidth(width));
    }

    class ServerListItem : spades::ui::ButtonBase {
        MainScreenServerItem @item;

        ServerListItem(spades::ui::UIManager @manager, MainScreenServerItem @item) {
            super(manager);
            @this.item = item;
        }

        private string FitText(string text, float width) {
            if (width <= 8.f)
                return "";
            if (Font.Measure(text).x <= width)
                return text;
            string ellipsis = "...";
            if (Font.Measure(ellipsis).x > width)
                return "";
            while (text.length > 0 && Font.Measure(text + ellipsis).x > width) {
                text = text.substr(0, text.length - 1);
            }
            return text + ellipsis;
        }

        void Render() {
            Renderer @renderer = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Image @white = renderer.RegisterImage("Gfx/White.tga");

            Vector4 background = Vector4(1.f, 1.f, 1.f, 0.015f);
            Vector4 foreground = Vector4(0.95f, 0.95f, 0.95f, 1.f);
            if (item.Favorite) {
                background = Vector4(0.36f, 0.36f, 0.36f, 0.22f);
                foreground = Vector4(1.f, 0.91f, 0.30f, 1.f);
            }
            if (Pressed && Hover) {
                background.w += 0.30f;
            } else if (Hover) {
                background.w += 0.15f;
            }
            renderer.ColorNP = background;
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, size.y));
            renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.045f);
            renderer.DrawImage(white, AABB2(pos.x, pos.y + size.y - 1.f, size.x, 1.f));

            // The reference order is Players, Name, Map, Mode, Ping. Use the exact same
            // responsive geometry as the clickable headers so every control remains aligned.
            float nameX = ServerListPlayersColumnWidth(size.x);
            float mapX = nameX + ServerListNameColumnWidth(size.x);
            float modeX = mapX + ServerListMapColumnWidth(size.x);
            float pingX = modeX + ServerListModeColumnWidth(size.x);
            float y = 2.f;
            float inset = 5.f;

            string players = ToString(item.NumPlayers) + "/" + ToString(item.MaxPlayers);
            Vector4 playerColor(1.f, 1.f, 1.f, 1.f);
            if (item.NumPlayers >= item.MaxPlayers)
                playerColor = Vector4(1.f, 0.64f, 0.64f, 1.f);
            else if (item.NumPlayers >= item.MaxPlayers * 3 / 4)
                playerColor = Vector4(1.f, 0.91f, 0.54f, 1.f);
            else if (item.NumPlayers == 0)
                playerColor = Vector4(0.72f, 0.72f, 0.72f, 1.f);
            Font.Draw(players, pos + Vector2((nameX - Font.Measure(players).x) * 0.5f, y), 1.f,
                      playerColor);

            Font.Draw(FitText(item.Name, mapX - nameX - inset * 2.f),
                      pos + Vector2(nameX + inset, y), 1.f, foreground);
            Font.Draw(FitText(item.MapName, modeX - mapX - inset * 2.f),
                      pos + Vector2(mapX + inset, y), 1.f,
                      Vector4(0.92f, 0.92f, 0.92f, 1.f));
            Font.Draw(FitText(item.GameMode, pingX - modeX - inset * 2.f),
                      pos + Vector2(modeX + inset, y), 1.f,
                      Vector4(0.92f, 0.92f, 0.92f, 1.f));

            string ping = ToString(item.Ping);
            Vector4 pingColor(0.56f, 0.95f, 0.62f, 1.f);
            if (item.Ping >= 180)
                pingColor = Vector4(1.f, 0.48f, 0.43f, 1.f);
            else if (item.Ping >= 90)
                pingColor = Vector4(1.f, 0.86f, 0.48f, 1.f);
            Font.Draw(ping,
                      pos + Vector2(pingX + (size.x - pingX - Font.Measure(ping).x) * 0.5f, y),
                      1.f, pingColor);
        }
    }

    funcdef void ServerListItemEventHandler(ServerListModel @sender, MainScreenServerItem @item);

    class ServerListModel : spades::ui::ListViewModel {
        spades::ui::UIManager @manager;
        MainScreenServerItem @[] @list;

        ServerListItemEventHandler @ItemActivated;
        ServerListItemEventHandler @ItemDoubleClicked;
        ServerListItemEventHandler @ItemRightClicked;

        ServerListModel(spades::ui::UIManager @manager, MainScreenServerItem @[] @list) {
            @this.manager = manager;
            @this.list = list;
        }
        int NumRows {
            get { return int(list.length); }
        }
        private void OnItemClicked(spades::ui::UIElement @sender) {
            ServerListItem @item = cast<ServerListItem>(sender);
            if (ItemActivated !is null)
                ItemActivated(this, item.item);
        }
        private void OnItemDoubleClicked(spades::ui::UIElement @sender) {
            ServerListItem @item = cast<ServerListItem>(sender);
            if (ItemDoubleClicked !is null)
                ItemDoubleClicked(this, item.item);
        }
        private void OnItemRightClicked(spades::ui::UIElement @sender) {
            ServerListItem @item = cast<ServerListItem>(sender);
            if (ItemRightClicked !is null)
                ItemRightClicked(this, item.item);
        }
        spades::ui::UIElement @CreateElement(int row) {
            ServerListItem i(manager, list[row]);
            @i.Activated = spades::ui::EventHandler(this.OnItemClicked);
            @i.DoubleClicked = spades::ui::EventHandler(this.OnItemDoubleClicked);
            @i.RightClicked = spades::ui::EventHandler(this.OnItemRightClicked);
            return i;
        }
        void RecycleElement(spades::ui::UIElement @elem) {}
    }

    class ServerListHeader : spades::ui::ButtonBase {
        string Text;
        ServerListHeader(spades::ui::UIManager @manager) { super(manager); }
        void OnActivated() { ButtonBase::OnActivated(); }
        void Render() {
            Renderer @renderer = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Image @white = renderer.RegisterImage("Gfx/White.tga");
            if (Pressed && Hover)
                renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.25f);
            else if (Hover)
                renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.14f);
            else
                renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.075f);
            renderer.DrawImage(white, AABB2(pos.x, pos.y, size.x, size.y));
            renderer.ColorNP = Vector4(1.f, 1.f, 1.f, 0.13f);
            renderer.DrawImage(white, AABB2(pos.x, pos.y + size.y - 1.f, size.x, 1.f));

            float scale = 1.f;
            Vector2 textSize = Font.Measure(Text);
            float availableWidth = Max(1.f, size.x - 8.f);
            if (textSize.x > availableWidth)
                scale = Max(0.65f, availableWidth / textSize.x);
            Vector2 scaledTextSize = textSize * scale;
            Font.Draw(Text, pos + (size - scaledTextSize) * 0.5f, scale,
                      Vector4(0.95f, 0.95f, 0.95f, 1.f));
        }
    }

    class MainScreenServerListLoadingView : spades::ui::UIElement {
        MainScreenServerListLoadingView(spades::ui::UIManager @manager) { super(manager); }
        void Render() {
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            string text = _Tr("MainScreen", "Loading...");
            Vector2 textSize = Font.Measure(text);
            Font.Draw(text, pos + (size - textSize) * 0.5f, 1.f,
                      Vector4(1.f, 1.f, 1.f, 0.8f));
        }
    }

    class MainScreenServerListErrorView : spades::ui::UIElement {
        MainScreenServerListErrorView(spades::ui::UIManager @manager) { super(manager); }
        void Render() {
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            string text = _Tr("MainScreen", "Failed to fetch the server list.");
            Vector2 textSize = Font.Measure(text);
            Font.Draw(text, pos + (size - textSize) * 0.5f, 1.f,
                      Vector4(1.f, 1.f, 1.f, 0.8f));
        }
    }
}
