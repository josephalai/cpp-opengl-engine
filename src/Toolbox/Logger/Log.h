//
// Created by Joseph Alai on 3/9/22.
//

#ifndef ENGINE_LOG_H
#define ENGINE_LOG_H

#include <iostream>
#include "ANSI.hpp"
#include <ctime>

using namespace std;
using namespace colored_cout;

class Log {

    enum CoutColor {
        black,
        red,
        green,
        yellow,
        blue,
        magenta,
        cyan,
        white,
        brightblack,
        brightred,
        brightgreen,
        brightyellow,
        brightblue,
        brightmagenta,
        brightcyan,
        brightwhite,
        nothing
    };
public:
    static void out(const string &text, CoutColor fgColor = black, CoutColor bgColor = nothing) {
        switch (fgColor) {
            case black:
                cout << ansi::fg_black;
                break;
            case red:
                cout << ansi::fg_red;
                break;
            case green:
                cout << ansi::fg_green;
                break;
            case yellow:
                cout << ansi::fg_yellow;
                break;
            case blue:
                cout << ansi::fg_blue;
                break;
            case magenta:
                cout << ansi::fg_magenta;
                break;
            case cyan:
                cout << ansi::fg_cyan;
                break;
            case white:
                cout << ansi::fg_white;
                break;
            case brightblack:
                cout << ansi::fg_brightblack;
                break;
            case brightred:
                cout << ansi::fg_brightred;
                break;
            case brightgreen:
                cout << ansi::fg_brightgreen;
                break;
            case brightyellow:
                cout << ansi::fg_brightyellow;
                break;
            case brightblue:
                cout << ansi::fg_brightblue;
                break;
            case brightmagenta:
                cout << ansi::fg_brightmagenta;
                break;
            case brightcyan:
                cout << ansi::fg_brightcyan;
                break;
            case brightwhite:
                cout << ansi::fg_brightwhite;
                break;
        }

        switch (bgColor) {
            case black:
                cout << ansi::bg_black;
                break;
            case red:
                cout << ansi::bg_red;
                break;
            case green:
                cout << ansi::bg_green;
                break;
            case yellow:
                cout << ansi::bg_yellow;
                break;
            case blue:
                cout << ansi::bg_blue;
                break;
            case magenta:
                cout << ansi::bg_magenta;
                break;
            case cyan:
                cout << ansi::bg_cyan;
                break;
            case white:
                cout << ansi::bg_white;
                break;
            case brightblack:
                cout << ansi::bg_brightblack;
                break;
            case brightred:
                cout << ansi::bg_brightred;
                break;
            case brightgreen:
                cout << ansi::bg_brightgreen;
                break;
            case brightyellow:
                cout << ansi::bg_brightyellow;
                break;
            case brightblue:
                cout << ansi::bg_brightblue;
                break;
            case brightmagenta:
                cout << ansi::bg_brightmagenta;
                break;
            case brightcyan:
                cout << ansi::bg_brightcyan;
                break;
            case brightwhite:
                cout << ansi::bg_brightwhite;
                break;
            default:
                cout << ansi::bg_default;
                break;
        }
        cout << text << std::endl;
        cout << ansi::reset;
    }

    static void emergency(const string &text = "") {
        string header = "[EMERGENCY] ";
        out(header.append(text), red, black);
    }

    static void alert(const string &text = "") {
        string header = "[ALERT] ";
        out(header.append(text), red);
    }

    static void critical(const string &text = "") {
        string header = "[CRITICAL] ";
        out(header.append(text), red);
    }

    static void error(const string &text = "") {
        string header = "[ERROR] ";
        out(header.append(text), red);
    }

    static void warning(const string &text = "") {
        string header = "[WARNING] ";
        out(header.append(text), red);
    }

    static void notice(const string &text = "") {
        string header = "[NOTICE] ";
        out(header.append(text), black);
    }

    static void info(const string &text = "") {
        string header = "[INFO] ";
        out(header.append(text), brightblack, nothing);
    }

    static void debug(const string &text = "") {
        string header = "[DEBUG] ";
        out(header.append(text), green, nothing);
    }

    static void setEnabled(bool enable) {
        enable = true;
    }

};

#endif //ENGINE_LOG_H
