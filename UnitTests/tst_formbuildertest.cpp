// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#include "pdfdocumentbuilder.h"
#include "pdfform.h"

#include <QtTest>

class FormBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    void textAndCheckBoxFieldsRoundTrip();
};

void FormBuilderTest::textAndCheckBoxFieldsRoundTrip()
{
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

    const pdf::PDFDocument document = builder.build();
    const pdf::PDFForm form =
        pdf::PDFForm::parse(&document, document.getCatalog()->getFormObject());

    QVERIFY(form.isAcroForm());
    QCOMPARE(form.getFormFields().size(), std::size_t(2));

    const pdf::PDFFormField* restoredText = form.getFormFields().at(0).data();
    QCOMPARE(restoredText->getFieldType(), pdf::PDFFormField::FieldType::Text);
    QCOMPARE(restoredText->getName(pdf::PDFFormField::Partial), textName);
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

    const pdf::PDFDocumentDataLoaderDecorator loader(&document);
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
}

QTEST_MAIN(FormBuilderTest)

#include "tst_formbuildertest.moc"
