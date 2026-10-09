// MIT License
// Copyright (c) 2026 FamilyPDF contributors

#ifndef PDFCHINESEFOLD_H
#define PDFCHINESEFOLD_H

#include "pdfglobal.h"

#include <QString>

namespace pdf
{

/// Replaces Traditional Chinese characters by their Simplified form, so text in either
/// script compares equal. The length and the position of every character stay the same.
PDF4QTLIBCORESHARED_EXPORT QString foldChineseScript(const QString& text);

}   // namespace pdf

#endif // PDFCHINESEFOLD_H
