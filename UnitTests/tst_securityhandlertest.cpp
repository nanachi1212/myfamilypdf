// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#include "pdfsecurityhandler.h"
#include "pdfparser.h"
#include "pdfexception.h"

#include <QtTest>

#include <memory>

using namespace pdf;

class SecurityHandlerTest : public QObject
{
    Q_OBJECT

private slots:
    void aesV3RoundTripAcrossBlockBoundaries();
    void aesV3UsesFreshInitializationVector();
    void aesV3RejectsTruncatedCiphertext();
    void aesV3RejectsInvalidPadding();
    void emptyOwnerPasswordDoesNotGrantAccess();
    void invalidEncryptionLengthsAreRejected();
    void cryptFilterLengthsAndUnits();
};

static PDFSecurityHandlerPointer createAes256Handler()
{
    PDFSecurityHandlerFactory::SecuritySettings settings;
    settings.algorithm = PDFSecurityHandlerFactory::AES_256;
    settings.encryptContents = PDFSecurityHandlerFactory::All;
    settings.userPassword = QStringLiteral("family-pdf-user");
    settings.ownerPassword = QStringLiteral("family-pdf-owner");
    settings.id = QByteArrayLiteral("family-pdf-security-test-id");
    return PDFSecurityHandlerFactory::createSecurityHandler(settings);
}

void SecurityHandlerTest::aesV3RoundTripAcrossBlockBoundaries()
{
    const PDFSecurityHandlerPointer handler = createAes256Handler();
    QVERIFY(!handler.isNull());
    const PDFObjectReference reference{ 7, 0 };

    for (const int size : { 0, 1, 15, 16, 17, 31, 32, 33 })
    {
        const QByteArray plaintext(size, 'A');
        const QByteArray ciphertext = handler->encrypt(plaintext, reference, PDFSecurityHandler::EncryptionScope::Stream);
        QVERIFY2(ciphertext.size() >= 32 && ciphertext.size() % 16 == 0,
                 "AES-V3 output must contain an IV and complete padded blocks.");
        QCOMPARE(handler->decrypt(ciphertext, reference, PDFSecurityHandler::EncryptionScope::Stream), plaintext);
    }
}

void SecurityHandlerTest::aesV3UsesFreshInitializationVector()
{
    const PDFSecurityHandlerPointer handler = createAes256Handler();
    QVERIFY(!handler.isNull());
    const PDFObjectReference reference{ 7, 0 };
    const QByteArray plaintext("same plaintext");

    const QByteArray first = handler->encrypt(plaintext, reference, PDFSecurityHandler::EncryptionScope::Stream);
    const QByteArray second = handler->encrypt(plaintext, reference, PDFSecurityHandler::EncryptionScope::Stream);
    QVERIFY(first != second);
    QVERIFY(first.left(16) != second.left(16));
}

void SecurityHandlerTest::aesV3RejectsTruncatedCiphertext()
{
    const PDFSecurityHandlerPointer handler = createAes256Handler();
    QVERIFY(!handler.isNull());
    const PDFObjectReference reference{ 7, 0 };
    QByteArray ciphertext = handler->encrypt(QByteArray("payload"), reference, PDFSecurityHandler::EncryptionScope::Stream);

    ciphertext.chop(1);
    QVERIFY(handler->decrypt(ciphertext, reference, PDFSecurityHandler::EncryptionScope::Stream).isEmpty());
}

void SecurityHandlerTest::aesV3RejectsInvalidPadding()
{
    const PDFSecurityHandlerPointer handler = createAes256Handler();
    QVERIFY(!handler.isNull());
    const PDFObjectReference reference{ 7, 0 };
    QByteArray ciphertext = handler->encrypt(QByteArray("payload"), reference, PDFSecurityHandler::EncryptionScope::Stream);

    ciphertext[ciphertext.size() - 1] = '\0';
    QVERIFY(handler->decrypt(ciphertext, reference, PDFSecurityHandler::EncryptionScope::Stream).isEmpty());
}

void SecurityHandlerTest::emptyOwnerPasswordDoesNotGrantAccess()
{
    for (const auto algorithm : { PDFSecurityHandlerFactory::RC4, PDFSecurityHandlerFactory::AES_128, PDFSecurityHandlerFactory::AES_256 })
    {
        PDFSecurityHandlerFactory::SecuritySettings settings;
        settings.algorithm = algorithm;
        settings.encryptContents = PDFSecurityHandlerFactory::All;
        settings.userPassword = QStringLiteral("family-pdf-user");
        settings.id = QByteArrayLiteral("family-pdf-security-test-id");
        const PDFSecurityHandlerPointer handler = PDFSecurityHandlerFactory::createSecurityHandler(settings);
        QVERIFY(!handler.isNull());

        // Without a password, the document must not open (neither as owner nor as user)
        std::unique_ptr<PDFSecurityHandler> reader(handler->clone());
        const auto noPassword = [](bool* ok) { *ok = false; return QString(); };
        QVERIFY(reader->authenticate(noPassword, false) == PDFSecurityHandler::AuthorizationResult::Cancelled);

        // Missing owner password is replaced by the user password
        const auto userPassword = [](bool* ok) { *ok = true; return QStringLiteral("family-pdf-user"); };
        QVERIFY(reader->authenticate(userPassword, true) == PDFSecurityHandler::AuthorizationResult::OwnerAuthorized);
    }
}

void SecurityHandlerTest::invalidEncryptionLengthsAreRejected()
{
    for (const qint64 length : {qint64(-8), qint64(0), qint64(7), qint64(39), qint64(41), qint64(136), qint64(4294967336LL)})
    {
        const QByteArray entries = " /O <" + QByteArray(64, '0') + "> /U <" + QByteArray(64, '0') + "> /P -4 >>";
        const QByteArray input = "<< /Filter /Standard /V 2 /R 3 /Length " + QByteArray::number(length) + entries;
        PDFParser parser(input, nullptr, PDFParser::None);
        const PDFObject dictionary = parser.getObject();
        QVERIFY_EXCEPTION_THROWN(PDFSecurityHandler::createSecurityHandler(dictionary, QByteArray("id")), PDFException);
    }
    for (const int length : {40, 128})
    {
        const QByteArray input = "<< /Filter /Standard /V 2 /R 3 /Length " + QByteArray::number(length) +
                                " /O <" + QByteArray(64, '0') + "> /U <" + QByteArray(64, '0') + "> /P -4 >>";
        PDFParser parser(input, nullptr, PDFParser::None);
        QVERIFY(!PDFSecurityHandler::createSecurityHandler(parser.getObject(), QByteArray("id")).isNull());
    }
}

class CryptFilterProbe : public PDFSecurityHandler
{
public:
    using PDFSecurityHandler::parseCryptFilter;
};

void SecurityHandlerTest::cryptFilterLengthsAndUnits()
{
    const auto parse = [](const QByteArray& name, qint64 length, bool publicKey)
    {
        const QByteArray input = "<< /CFM /" + name + " /Length " + QByteArray::number(length) + " >>";
        PDFParser parser(input, nullptr, PDFParser::None);
        return CryptFilterProbe::parseCryptFilter(256, parser.getObject(), publicKey);
    };
    for (const qint64 bad : {qint64(-5), qint64(0), qint64(4), qint64(17), qint64(2147483647), qint64(4294967312LL)})
        QVERIFY_EXCEPTION_THROWN(parse("V2", bad, false), PDFException);
    QCOMPARE(parse("V2", 5, false).keyLength, 5);
    QCOMPARE(parse("V2", 16, false).keyLength, 16);
    QCOMPARE(parse("V2", 40, true).keyLength, 5);
    QCOMPARE(parse("AESV2", 128, true).keyLength, 16);
    QCOMPARE(parse("AESV3", 256, true).keyLength, 32);
    QCOMPARE(parse("AESV3", 32, false).keyLength, 32);
    QVERIFY_EXCEPTION_THROWN(parse("AESV3", 16, false), PDFException);
    QVERIFY_EXCEPTION_THROWN(parse("AESV2", 32, false), PDFException);
    QVERIFY_EXCEPTION_THROWN(parse("V2", 41, true), PDFException);
}

QTEST_MAIN(SecurityHandlerTest)
#include "tst_securityhandlertest.moc"
