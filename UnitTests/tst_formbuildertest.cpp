// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfdocumentwriter.h"
#include "pdfform.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class FormBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    void textAndCheckBoxFieldsRoundTrip();
};

void FormBuilderTest::textAndCheckBoxFieldsRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page =
        builder.appendPage(QRectF(0, 0, 595, 842));

    const QString textName = QString::fromUtf16(u"\u59D3\u540D");
    const QString textValue = QString::fromUtf16(u"\u7E41\u9AD4\u6E2C\u8A66");
    const pdf::PDFFormField::FieldFlags textFlags{
        pdf::PDFFormField::Required,
        pdf::PDFFormField::Multiline
    };
    const pdf::PDFObjectReference textField =
        builder.createFormFieldText(textName, textValue, textFlags, 120);
    const QString textTooltip =
        QString::fromUtf16(u"\u8ACB\u8F38\u5165\u59D3\u540D");
    builder.setFormFieldTooltip(textField, textTooltip);
    builder.createFormFieldWidget(textField,
                                  page,
                                  QRectF(50, 700, 240, 60),
                                  QByteArrayLiteral("/Helv 12 Tf 0 g"));
    builder.appendAcroFormField(textField);

    const QString checkBoxName = QString::fromUtf16(u"\u540C\u610F");
    const pdf::PDFObjectReference checkBox =
        builder.createFormFieldCheckBox(checkBoxName,
                                        true,
                                        pdf::PDFFormField::Required);
    builder.createFormFieldWidget(checkBox,
                                  page,
                                  QRectF(50, 640, 24, 24),
                                  QByteArray());
    builder.appendAcroFormField(checkBox);

    pdf::PDFDocument document = builder.build();
    QString outputPath = qEnvironmentVariable("FAMILYPDF_FORM_FIXTURE");
    if (outputPath.isEmpty())
    {
        outputPath = directory.filePath(QStringLiteral("form-interop.pdf"));
    }
    else
    {
        QDir().mkpath(QFileInfo(outputPath).absolutePath());
    }

    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult writeResult =
        writer.write(outputPath, &document, true);
    QVERIFY2(static_cast<bool>(writeResult),
             qPrintable(writeResult.getErrorMessage()));

    pdf::PDFDocumentReader reader(nullptr, nullptr, false, false);
    pdf::PDFDocument restoredDocument = reader.readFromFile(outputPath);
    QVERIFY2(reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK,
             qPrintable(reader.getErrorMessage()));

    const pdf::PDFForm form =
        pdf::PDFForm::parse(&restoredDocument,
                            restoredDocument.getCatalog()->getFormObject());

    QVERIFY(form.isAcroForm());
    QCOMPARE(form.getFormFields().size(), std::size_t(2));

    const pdf::PDFFormField* restoredText = form.getFormFields().at(0).data();
    QCOMPARE(restoredText->getFieldType(), pdf::PDFFormField::FieldType::Text);
    QCOMPARE(restoredText->getName(pdf::PDFFormField::Partial), textName);
    QCOMPARE(restoredText->getName(pdf::PDFFormField::UserCaption),
             textTooltip);
    QVERIFY(restoredText->getFlags().testFlag(pdf::PDFFormField::Required));
    QVERIFY(restoredText->getFlags().testFlag(pdf::PDFFormField::Multiline));
    QCOMPARE(restoredText->getWidgets().size(), std::size_t(1));
    QCOMPARE(restoredText->getWidgets().front().getPage(), page);

    const auto* restoredTextDetails =
        dynamic_cast<const pdf::PDFFormFieldText*>(restoredText);
    QVERIFY(restoredTextDetails);
    QCOMPARE(restoredTextDetails->getTextMaximalLength(), pdf::PDFInteger(120));
    QCOMPARE(restoredTextDetails->getDefaultAppearance(),
             QByteArrayLiteral("/Helv 12 Tf 0 g"));

    const pdf::PDFDocumentDataLoaderDecorator loader(&restoredDocument);
    QCOMPARE(loader.readTextString(restoredText->getValue(), QString()),
             textValue);
    QCOMPARE(loader.readTextString(restoredText->getDefaultValue(), QString()),
             textValue);

    const pdf::PDFFormField* restoredCheckBox =
        form.getFormFields().at(1).data();
    QCOMPARE(restoredCheckBox->getFieldType(),
             pdf::PDFFormField::FieldType::Button);
    QCOMPARE(restoredCheckBox->getName(pdf::PDFFormField::Partial),
             checkBoxName);
    QVERIFY(restoredCheckBox->getFlags().testFlag(pdf::PDFFormField::Required));
    QCOMPARE(restoredCheckBox->getWidgets().size(), std::size_t(1));
    QCOMPARE(restoredCheckBox->getWidgets().front().getPage(), page);

    const auto* restoredButton =
        dynamic_cast<const pdf::PDFFormFieldButton*>(restoredCheckBox);
    QVERIFY(restoredButton);
    QCOMPARE(restoredButton->getButtonType(),
             pdf::PDFFormFieldButton::ButtonType::CheckBox);
    QCOMPARE(loader.readName(restoredCheckBox->getValue()),
             QByteArrayLiteral("Yes"));
    QCOMPARE(loader.readName(restoredCheckBox->getDefaultValue()),
             QByteArrayLiteral("Yes"));

    pdf::PDFFormManager formManager(nullptr);
    formManager.setDocument(pdf::PDFModifiedDocument(
        &restoredDocument,
        nullptr));
    const pdf::PDFFormField* managedCheckBox =
        formManager.getForm()->getFormFields().at(1).data();
    QCOMPARE(
        pdf::PDFFormFieldButton::getOnAppearanceState(
            &formManager,
            &managedCheckBox->getWidgets().front()),
        QByteArrayLiteral("Yes"));
}

QTEST_MAIN(FormBuilderTest)

#include "tst_formbuildertest.moc"
