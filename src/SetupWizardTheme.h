#pragma once

#include <QString>

// ── SetupWizard Theme ──────────────────────────────────────────────────────
//
//  Single point of truth for every colour, size, font, and spacing value
//  used across the setup wizard pages.  Nothing in SetupWizard.cpp should
//  contain a raw colour hex, pixel size, or font-family string.
//
//  Usage:
//      #include "SetupWizardTheme.h"
//      using namespace WizardTheme;
//      label->setStyleSheet(statusStyle(Color::success));
//

namespace WizardTheme {

// ── Colours ────────────────────────────────────────────────────────────────

namespace Color {
    constexpr const char *success   = "#4CAF50";   // green  – git found, connected, done
    constexpr const char *error     = "#F44336";   // red    – git missing, connection failed
    constexpr const char *warning   = "#FF9800";   // amber  – credential probe failed / missing
    constexpr const char *accent    = "#FF5500";   // orange – privacy box border
    constexpr const char *muted     = "#888888";   // grey   – footnotes, help text
    constexpr const char *logText   = "#CCCCCC";   // light  – log output foreground
    constexpr const char *boxBg     = "rgba(128, 128, 128, 0.1)";  // privacy box fill
}

// ── Typography ─────────────────────────────────────────────────────────────

namespace Font {
    constexpr int statusSize        = 14;          // px – status labels
    constexpr int helpSize          = 12;          // px – footnotes, privacy box
    constexpr int logSize           = 11;          // px – log output
    constexpr const char *mono      = "'Fira Code', monospace";
}

// ── Spacing ────────────────────────────────────────────────────────────────

namespace Spacing {
    constexpr int statusPad         = 8;           // px – status label padding
    constexpr int statusPadLarge    = 10;          // px – git-check status padding
    constexpr int boxMargin         = 24;          // px – privacy box outer margin
    constexpr int boxPad            = 24;          // px – privacy box inner padding
    constexpr int boxRadius         = 6;           // px – privacy box corner radius
    constexpr int logPad            = 8;           // px – log output padding
    constexpr int logRadius         = 4;           // px – log output corner radius
}

// ── Composite style-sheet builders ─────────────────────────────────────────
//
//  These return complete QSS strings ready for setStyleSheet().
//  Keeping them inline so the compiler can fold them at the call site.
//

// Status label – neutral (no state colour)
inline QString statusStyle(int pad = Spacing::statusPad) {
    return QString("font-size: %1px; padding: %2px;")
        .arg(Font::statusSize).arg(pad);
}

// Status label – with a state colour
inline QString statusStyle(const char *color, int pad = Spacing::statusPad) {
    return QString("font-size: %1px; padding: %2px; color: %3;")
        .arg(Font::statusSize).arg(pad).arg(color);
}

// Privacy / callout box
inline QString privacyBoxStyle() {
    return QString(
        "QLabel {"
        "  background-color: %1;"
        "  margin: %2px;"
        "  padding: %3px;"
        "  border-radius: %4px;"
        "  border: 1px solid %5;"
        "  font-size: %6px;"
        "}")
        .arg(Color::boxBg)
        .arg(Spacing::boxMargin)
        .arg(Spacing::boxPad)
        .arg(Spacing::boxRadius)
        .arg(Color::accent)
        .arg(Font::helpSize);
}

// Log / terminal output pane
inline QString logOutputStyle() {
    return QString(
        "font-family: %1; font-size: %2px; "
        "color: %3; padding: %4px; border-radius: %5px;")
        .arg(Font::mono)
        .arg(Font::logSize)
        .arg(Color::logText)
        .arg(Spacing::logPad)
        .arg(Spacing::logRadius);
}

// Inline HTML: muted help/footnote paragraph  (for use inside rich-text strings)
inline QString mutedHtml(const QString &body) {
    return QString("<p style='color: %1; font-size: %2px;'>%3</p>")
        .arg(Color::muted)
        .arg(Font::helpSize)
        .arg(body);
}

} // namespace WizardTheme
