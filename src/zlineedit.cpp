/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "zlineedit.h"
#include "iDescriptor-ui.h"

ZLineEdit::ZLineEdit(QWidget *parent) : QLineEdit(parent) { setupStyles(); }

ZLineEdit::ZLineEdit(const QString &text, QWidget *parent)
    : QLineEdit(text, parent)
{
    setupStyles();
}

void ZLineEdit::setupStyles() { updateStyles(); }

void ZLineEdit::updateStyles()
{
    // FIMXE: seg faults if Qt decides to change pallets due to applied
    // stylesheets
    // setStyleSheet("QLineEdit { "
    //               "    border: 2px solid " +
    //               qApp->palette().color(QPalette::Midlight).name() +
    //               "; "
    //               "    border-radius: 6px; "
    //               "    padding: 8px 12px; "
    //               "    font-size: 14px; "
    //               "} "
    //               "QLineEdit:focus { "
    //               "    border: 2px solid " +
    //               COLOR_ACCENT_BLUE.name() +
    //               "; "
    //               "    outline: none; "
    //               "}");
}