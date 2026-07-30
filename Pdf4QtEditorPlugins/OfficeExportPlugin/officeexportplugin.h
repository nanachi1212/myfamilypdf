// MIT License
//
// Copyright (c) 2026 FamilyPDF Contributors

#ifndef OFFICEEXPORTPLUGIN_H
#define OFFICEEXPORTPLUGIN_H

#include "pdfplugin.h"

#include <QObject>

namespace pdfplugin
{

class OfficeExportPlugin : public pdf::PDFPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "PDF4QT.OfficeExportPlugin"
                      FILE "OfficeExportPlugin.json")

private:
    using BaseClass = pdf::PDFPlugin;

public:
    OfficeExportPlugin();

    void setWidget(pdf::PDFWidget* widget) override;
    void setDocument(const pdf::PDFModifiedDocument& document) override;
    std::vector<QAction*> getActions() const override;
    QString getPluginMenuName() const override;

private:
    void exportDocument(const QString& format);
    QString findHelper() const;
    void updateActions();

    QAction* m_exportWord;
    QAction* m_exportExcel;
};

} // namespace pdfplugin

#endif // OFFICEEXPORTPLUGIN_H
