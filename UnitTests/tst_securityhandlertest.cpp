// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors

#include "pdfsecurityhandler.h"

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

QTEST_MAIN(SecurityHandlerTest)
#include "tst_securityhandlertest.moc"
