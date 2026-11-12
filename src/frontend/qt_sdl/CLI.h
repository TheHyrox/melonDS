/*
    Copyright 2021-2023 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef CLI_H
#define CLI_H

#include <QApplication>
#include <QStringList>

#include <optional>

namespace CLI {

struct CommandLineOptions
{
    std::optional<QString> dsRomPath;
    std::optional<QString> dsRomArchivePath;
    std::optional<QString> gbaRomPath;
    std::optional<QString> gbaRomArchivePath;
    bool fullscreen;
    bool boot;

    // Nexus symbiotic CLI extensions
    std::optional<int> screenLayout;
    std::optional<int> screenSizing;
    std::optional<int> screenRotation;
    std::optional<int> screenGap;
    std::optional<bool> screenSwap;
    std::optional<bool> integerScaling;
    std::optional<bool> screenFilter;
    std::optional<int> windowScale;
    std::optional<int> renderer3D;
    std::optional<int> glScale;
    std::optional<int> consoleType;
    std::optional<bool> vsync;
    bool autoGamepad = true;

    // RetroAchievements
    std::optional<QString> raUser;
    std::optional<QString> raToken;
    bool raHardcore = true;
};

extern CommandLineOptions* ManageArgs(QApplication& melon);

}

#endif
