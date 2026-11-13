// MIT License
// Copyright (c) 2026 FamilyPDF contributors
#ifndef PDFDOCUMENTWORKSPACE_H
#define PDFDOCUMENTWORKSPACE_H

#include <QAction>
#include <QCoreApplication>
#include <QLabel>
#include <QStackedLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace pdfviewer
{
// One empty-state action, shared by Reader and Editor. Reuse the controller's
// QAction so shortcuts, busy state and the existing open/cancel flow stay intact.
class PDFDocumentWorkspace : public QWidget
{
public:
    PDFDocumentWorkspace(QWidget* document, QAction* openAction, QWidget* parent = nullptr) :
        QWidget(parent), m_document(document), m_stack(new QStackedLayout(this))
    {
        setObjectName(QStringLiteral("documentWorkspace"));
        m_stack->setContentsMargins(0, 0, 0, 0);
        auto* home = new QWidget(this);
        home->setObjectName(QStringLiteral("documentHome"));
        auto* layout = new QVBoxLayout(home);
        layout->setContentsMargins(32, 32, 32, 32);
        layout->setSpacing(24);
        layout->addStretch();
        auto* title = new QLabel(QStringLiteral("FamilyPDF"), home);
        auto font = title->font();
        font.setPointSize(26);
        font.setWeight(QFont::DemiBold);
        title->setFont(font);
        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);
        m_openButton = new QPushButton(home);
        m_openButton->setObjectName(QStringLiteral("openPdfButton"));
        m_openButton->setText(QCoreApplication::translate("PDFDocumentWorkspace", "Open PDF"));
        m_openButton->setAccessibleName(m_openButton->text());
        m_openButton->setIcon(openAction->icon());
        m_openButton->setEnabled(openAction->isEnabled());
        connect(m_openButton, &QPushButton::clicked, openAction, &QAction::trigger);
        connect(openAction, &QAction::changed, m_openButton, [this, openAction]()
        {
            m_openButton->setEnabled(openAction->isEnabled());
            m_openButton->setIcon(openAction->icon());
        });
        m_openButton->setMinimumSize(160, 48);
        layout->addWidget(m_openButton, 0, Qt::AlignHCenter);
        layout->addStretch();
        m_stack->addWidget(home);
        m_stack->addWidget(document);
        setFocusProxy(m_openButton);
    }

    void setDocumentAvailable(bool available)
    {
        const int index = available ? 1 : 0;
        if (m_stack->currentIndex() == index)
            return;
        m_stack->setCurrentIndex(index);
        QWidget* target = available ? m_document : m_openButton;
        setFocusProxy(target);
        target->setFocus(Qt::OtherFocusReason);
    }

private:
    QWidget* m_document;
    QStackedLayout* m_stack;
    QPushButton* m_openButton;
};
}
#endif
