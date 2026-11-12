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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>

#include "CLI.h"
#include "Platform.h"

using melonDS::Platform::Log;
using melonDS::Platform::LogLevel;

namespace CLI
{

CommandLineOptions* ManageArgs(QApplication& melon)
{
    QCommandLineParser parser;
    parser.addHelpOption();

    parser.addPositionalArgument("nds", "Nintendo DS ROM (or an archive file which contains it) to load into Slot-1");
    parser.addPositionalArgument("gba", "GBA ROM (or an archive file which contains it) to load into Slot-2");

    parser.addOption(QCommandLineOption({"b", "boot"}, "Whether to boot firmware on startup. Defaults to \"auto\" (boot if NDS rom given)", "auto/always/never", "auto"));
    parser.addOption(QCommandLineOption({"f", "fullscreen"}, "Start melonDS in fullscreen mode"));

    // Nexus symbiotic CLI options
    parser.addOption(QCommandLineOption({"l", "layout"}, "Screen layout: natural, vertical (top-bottom), horizontal (side-by-side), hybrid, single-top, single-bottom", "mode"));
    parser.addOption(QCommandLineOption({"s", "sizing"}, "Screen sizing: even, emph-top, emph-bottom, auto, top-only, bottom-only", "mode"));
    parser.addOption(QCommandLineOption({"r", "rotation"}, "Screen rotation in degrees: 0, 90, 180, 270", "deg"));
    parser.addOption(QCommandLineOption({"g", "gap"}, "Gap between screens in pixels (0-500)", "px"));
    parser.addOption(QCommandLineOption("swap-screens", "Swap top and bottom screens (true/false/1/0)", "bool"));
    parser.addOption(QCommandLineOption("integer-scale", "Enable integer scaling (true/false/1/0)", "bool"));
    parser.addOption(QCommandLineOption("filter", "Screen filtering: linear/true/1 or nearest/false/0", "mode"));
    parser.addOption(QCommandLineOption("window-scale", "Window size multiplier (1-8)", "factor"));
    parser.addOption(QCommandLineOption("renderer", "3D renderer: software (0) or opengl (1)", "mode"));
    parser.addOption(QCommandLineOption("scale", "Internal 3D resolution scale factor (1-16, enables OpenGL if > 1)", "factor"));
    parser.addOption(QCommandLineOption("console-type", "Console type: ds (0) or dsi (1)", "type"));
    parser.addOption(QCommandLineOption("vsync", "Enable V-Sync (true/false/1/0)", "bool"));
    parser.addOption(QCommandLineOption("auto-gamepad", "Auto-configure connected SDL gamepad if unbound (true/false)", "bool", "true"));

    parser.addOption(QCommandLineOption("ra-user", "RetroAchievements username", "name"));
    parser.addOption(QCommandLineOption("ra-token", "RetroAchievements login token (not the password)", "token"));
    parser.addOption(QCommandLineOption("ra-hardcore", "RetroAchievements hardcore mode: disables savestates and cheats (true/false)", "bool", "true"));

#ifdef ARCHIVE_SUPPORT_ENABLED
    parser.addOption(QCommandLineOption({"a", "archive-file"}, "Specify file to load inside an archive given (NDS)", "rom"));
    parser.addOption(QCommandLineOption({"A", "archive-file-gba"}, "Specify file to load inside an archive given (GBA)", "rom"));
#endif

    parser.process(melon);

    CommandLineOptions* options = new CommandLineOptions;

    options->fullscreen = parser.isSet("fullscreen");

    QStringList posargs = parser.positionalArguments();
    switch (posargs.size())
    {
        default:
            Log(LogLevel::Warn, "Too many positional arguments; ignoring 3 onwards\n");
        case 2:
            options->gbaRomPath = posargs[1];
        case 1:
            options->dsRomPath = posargs[0];
        case 0:
            break;
    }

    QString bootMode = parser.value("boot");
    if (bootMode == "auto")
    {
        options->boot = !posargs.empty();
    }
    else if (bootMode == "always")
    {
        options->boot = true;
    }
    else if (bootMode == "never")
    {
        options->boot = false;
    }
    else
    {
        Log(LogLevel::Error, "ERROR: -b/--boot only accepts auto/always/never as arguments\n");
        exit(1);
    }

#ifdef ARCHIVE_SUPPORT_ENABLED
    if (parser.isSet("archive-file"))
    {
        if (options->dsRomPath.has_value())
        {
            options->dsRomArchivePath = parser.value("archive-file");
        }
        else
        {
            Log(LogLevel::Error, "Option -a/--archive-file given, but no archive specified!");
        }
    }

    if (parser.isSet("archive-file-gba"))
    {
        if (options->gbaRomPath.has_value())
        {
            options->gbaRomArchivePath = parser.value("archive-file-gba");
        }
        else
        {
            Log(LogLevel::Error, "Option -A/--archive-file-gba given, but no archive specified!");
        }
    }
#endif

    auto parseBool = [](const QString& v) -> std::optional<bool> {
        QString l = v.trimmed().toLower();
        if (l == "1" || l == "true" || l == "yes" || l == "on" || l == "linear") return true;
        if (l == "0" || l == "false" || l == "no" || l == "off" || l == "nearest") return false;
        return std::nullopt;
    };

    if (parser.isSet("layout"))
    {
        QString l = parser.value("layout").trimmed().toLower();
        if (l == "natural" || l == "0") {
            options->screenLayout = 0;
        } else if (l == "vertical" || l == "top-bottom" || l == "1") {
            options->screenLayout = 1;
            options->screenSizing = 0;
        } else if (l == "horizontal" || l == "side-by-side" || l == "2") {
            options->screenLayout = 2;
            options->screenSizing = 0;
        } else if (l == "hybrid" || l == "3") {
            options->screenLayout = 3;
            options->screenSizing = 1;
        } else if (l == "single-top" || l == "top-only") {
            options->screenLayout = 0;
            options->screenSizing = 4;
        } else if (l == "single-bottom" || l == "bottom-only" || l == "bot-only") {
            options->screenLayout = 0;
            options->screenSizing = 5;
        }
    }

    if (parser.isSet("sizing"))
    {
        QString s = parser.value("sizing").trimmed().toLower();
        if (s == "even" || s == "0") options->screenSizing = 0;
        else if (s == "emph-top" || s == "top" || s == "1") options->screenSizing = 1;
        else if (s == "emph-bottom" || s == "emph-bot" || s == "bottom" || s == "2") options->screenSizing = 2;
        else if (s == "auto" || s == "3") options->screenSizing = 3;
        else if (s == "top-only" || s == "single-top" || s == "4") options->screenSizing = 4;
        else if (s == "bottom-only" || s == "bot-only" || s == "single-bottom" || s == "5") options->screenSizing = 5;
    }

    if (parser.isSet("rotation"))
    {
        QString r = parser.value("rotation").trimmed().toLower();
        if (r == "0" || r == "0deg") options->screenRotation = 0;
        else if (r == "90" || r == "90deg" || r == "1") options->screenRotation = 1;
        else if (r == "180" || r == "180deg" || r == "2") options->screenRotation = 2;
        else if (r == "270" || r == "270deg" || r == "3") options->screenRotation = 3;
    }

    if (parser.isSet("gap"))
    {
        bool ok = false;
        int gap = parser.value("gap").toInt(&ok);
        if (ok && gap >= 0 && gap <= 500) options->screenGap = gap;
    }

    if (parser.isSet("swap-screens"))
        options->screenSwap = parseBool(parser.value("swap-screens"));

    if (parser.isSet("integer-scale"))
        options->integerScaling = parseBool(parser.value("integer-scale"));

    if (parser.isSet("filter"))
        options->screenFilter = parseBool(parser.value("filter"));

    if (parser.isSet("window-scale"))
    {
        bool ok = false;
        int ws = parser.value("window-scale").toInt(&ok);
        if (ok && ws >= 1 && ws <= 8) options->windowScale = ws;
    }

    if (parser.isSet("renderer"))
    {
        QString r = parser.value("renderer").trimmed().toLower();
        if (r == "software" || r == "soft" || r == "0") options->renderer3D = 0;
        else if (r == "opengl" || r == "gl" || r == "1") options->renderer3D = 1;
    }

    if (parser.isSet("scale"))
    {
        bool ok = false;
        int sc = parser.value("scale").toInt(&ok);
        if (ok && sc >= 1 && sc <= 16)
        {
            options->glScale = sc;
            if (sc > 1 && !options->renderer3D.has_value())
                options->renderer3D = 1;
        }
    }

    if (parser.isSet("console-type"))
    {
        QString c = parser.value("console-type").trimmed().toLower();
        if (c == "ds" || c == "0") options->consoleType = 0;
        else if (c == "dsi" || c == "1") options->consoleType = 1;
    }

    if (parser.isSet("vsync"))
        options->vsync = parseBool(parser.value("vsync"));

    if (parser.isSet("auto-gamepad"))
    {
        if (auto b = parseBool(parser.value("auto-gamepad")))
            options->autoGamepad = *b;
    }

    if (parser.isSet("ra-user"))
        options->raUser = parser.value("ra-user");
    if (parser.isSet("ra-token"))
        options->raToken = parser.value("ra-token");
    if (parser.isSet("ra-hardcore"))
    {
        if (auto b = parseBool(parser.value("ra-hardcore")))
            options->raHardcore = *b;
    }

    return options;
}

}
