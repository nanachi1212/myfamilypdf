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
    void radioAndChoiceFieldsRoundTrip();
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

void FormBuilderTest::radioAndChoiceFieldsRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page =
        builder.appendPage(QRectF(0, 0, 595, 842));

    const pdf::PDFObjectReference radioGroup =
        builder.createFormFieldRadioGroup(
            QString::fromUtf16(u"\u914D\u9001\u65B9\u5F0F"),
            QStringLiteral("home"),
            pdf::PDFFormField::FieldFlags{
                pdf::PDFFormField::Required,
                pdf::PDFFormField::NoToggleToOff
            });
    builder.createFormFieldRadioWidget(
        radioGroup, page, QRectF(50, 700, 20, 20),
        QStringLiteral("home"), true);
    builder.createFormFieldRadioWidget(
        radioGroup, page, QRectF(50, 670, 20, 20),
        QStringLiteral("store"), false);
    builder.appendAcroFormField(radioGroup);

    const pdf::PDFFormFieldChoice::Options options{
        { QStringLiteral("tw"), QString::fromUtf16(u"\u53F0\u7063") },
        { QStringLiteral("cn"), QString::fromUtf16(u"\u4E2D\u56FD") },
        { QStringLiteral("other"), QStringLiteral("Other") }
    };
    const pdf::PDFObjectReference combo =
        builder.createFormFieldChoice(
            QString::fromUtf16(u"\u5730\u5340"),
            options,
            { 0 },
            pdf::PDFFormField::FieldFlags{
                pdf::PDFFormField::Required,
                pdf::PDFFormField::Combo
            });
    builder.createFormFieldWidget(
        combo, page, QRectF(100, 700, 160, 24),
        QByteArrayLiteral("/Helv 10 Tf 0 g"));
    builder.appendAcroFormField(combo);

    const pdf::PDFObjectReference list =
        builder.createFormFieldChoice(
            QString::fromUtf16(u"\u8A9E\u8A00"),
            options,
            { 0, 1 },
            pdf::PDFFormField::MultiSelect);
    builder.createFormFieldWidget(
        list, page, QRectF(100, 600, 160, 80),
        QByteArrayLiteral("/Helv 10 Tf 0 g"));
    builder.appendAcroFormField(list);

    pdf::PDFDocument document = builder.build();
    const QString outputPath =
        directory.filePath(QStringLiteral("radio-choice-interop.pdf"));
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
    QCOMPARE(form.getFormFields().size(), std::size_t(3));

    const auto* radio = dynamic_cast<const pdf::PDFFormFieldButton*>(
        form.getFormFields().at(0).data());
    QVERIFY(radio);
    QCOMPARE(radio->getButtonType(),
             pdf::PDFFormFieldButton::ButtonType::RadioButton);
    QCOMPARE(radio->getChildFields().size(), std::size_t(2));
    QCOMPARE(radio->getChildFields().at(0)->getWidgets().size(),
             std::size_t(1));
    QCOMPARE(radio->getChildFields().at(1)->getWidgets().size(),
             std::size_t(1));
    QVERIFY(radio->getFlags().testFlag(pdf::PDFFormField::Required));
    QVERIFY(radio->getFlags().testFlag(pdf::PDFFormField::NoToggleToOff));

    pdf::PDFDocumentDataLoaderDecorator loader(&restoredDocument);
    QCOMPARE(loader.readName(radio->getValue()), QByteArrayLiteral("home"));
    pdf::PDFFormManager formManager(nullptr);
    formManager.setDocument(pdf::PDFModifiedDocument(&restoredDocument,
                                                     nullptr));
    const auto* managedRadio =
        dynamic_cast<const pdf::PDFFormFieldButton*>(
            formManager.getForm()->getFormFields().at(0).data());
    QVERIFY(managedRadio);
    const pdf::PDFFormField* firstRadio =
        managedRadio->getChildFields().at(0).data();
    const pdf::PDFFormField* secondRadio =
        managedRadio->getChildFields().at(1).data();
    QCOMPARE(pdf::PDFFormFieldButton::getOnAppearanceState(
                 &formManager, &firstRadio->getWidgets().at(0)),
             QByteArrayLiteral("home"));
    QCOMPARE(pdf::PDFFormFieldButton::getOnAppearanceState(
                 &formManager, &secondRadio->getWidgets().at(0)),
             QByteArrayLiteral("store"));

    const auto* restoredCombo =
        dynamic_cast<const pdf::PDFFormFieldChoice*>(
            form.getFormFields().at(1).data());
    QVERIFY(restoredCombo);
    QVERIFY(restoredCombo->isComboBox());
    QCOMPARE(restoredCombo->getOptions().size(), std::size_t(3));
    QCOMPARE(restoredCombo->getOptions().at(0).exportString,
             QStringLiteral("tw"));
    QCOMPARE(restoredCombo->getOptions().at(0).userString,
             QString::fromUtf16(u"\u53F0\u7063"));
    QCOMPARE(loader.readTextString(restoredCombo->getValue(), QString()),
             QStringLiteral("tw"));

    const auto* restoredList =
        dynamic_cast<const pdf::PDFFormFieldChoice*>(
            form.getFormFields().at(2).data());
    QVERIFY(restoredList);
    QVERIFY(restoredList->isListBox());
    QVERIFY(restoredList->getFlags().testFlag(
        pdf::PDFFormField::MultiSelect));
    QCOMPARE(loader.readIntegerArray(restoredList->getSelection()),
             (std::vector<pdf::PDFInteger>{ 0, 1 }));
    QCOMPARE(loader.readTextStringList(restoredList->getValue()),
             (QStringList{ QStringLiteral("tw"), QStringLiteral("cn") }));
}

QTEST_MAIN(FormBuilderTest)

#include "tst_formbuildertest.moc"
