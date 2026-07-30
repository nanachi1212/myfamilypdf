// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#ifndef DOCUMENTEDITPLUGIN_H
#define DOCUMENTEDITPLUGIN_H

#include "pdfplugin.h"

#include <QObject>

namespace pdf
{
struct PDFDocumentDecorationSettings;
struct PDFPageGeometrySettings;
}

namespace pdfplugin
{

class DocumentEditPlugin : public pdf::PDFPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "PDF4QT.DocumentEditPlugin"
                      FILE "DocumentEditPlugin.json")

private:
    using BaseClass = pdf::PDFPlugin;

public:
    DocumentEditPlugin();

    void setWidget(pdf::PDFWidget* widget) override;
    void setDocument(const pdf::PDFModifiedDocument& document) override;
    std::vector<QAction*> getActions() const override;
    QString getPluginMenuName() const override;

private:
    void addTextWatermark();
    void setPageBackground();
    void changePageGeometry();
    void rotatePages(int quarterTurns);
    void applyDecoration(
        const pdf::PDFDocumentDecorationSettings& settings);
    void applyGeometry(const pdf::PDFPageGeometrySettings& settings);
    void updateActions();

    QAction* m_addTextWatermark;
    QAction* m_setPageBackground;
    QAction* m_changePageGeometry;
    QAction* m_rotatePagesLeft;
    QAction* m_rotatePagesRight;
};

} // namespace pdfplugin

#endif // DOCUMENTEDITPLUGIN_H
