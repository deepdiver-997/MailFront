#include "cryptohelper.h"

#include <QSysInfo>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDebug>

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <CommonCrypto/CommonCryptor.h>
#  define HAS_COMMON_CRYPTO 1
#else
#  include <openssl/err.h>
#  include <openssl/evp.h>
#  include <openssl/rand.h>
#  include <memory>

static QString lastOpenSslError()
{
    const unsigned long error = ERR_get_error();
    if (error == 0)
        return QStringLiteral("unknown OpenSSL error");

    char buffer[256] = {};
    ERR_error_string_n(error, buffer, sizeof(buffer));
    return QString::fromLatin1(buffer);
}
#endif

QByteArray CryptoHelper::deriveKey()
{
    // 机器唯一 ID + 固定盐 → SHA-256 → 256-bit key
    QByteArray material = QSysInfo::machineUniqueId();
    material.append("mail_front_salt_2024");
    return QCryptographicHash::hash(material, QCryptographicHash::Sha256);
}

QString CryptoHelper::encrypt(const QString &plainText)
{
    if (plainText.isEmpty())
        return {};

    QByteArray key = deriveKey();
    QByteArray plainData = plainText.toUtf8();

    // 随机 16 字节 IV
    QByteArray iv(16, '\0');
#if defined(HAS_COMMON_CRYPTO)
    for (int i = 0; i < 16; ++i)
        iv[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
#else
    if (RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()), iv.size()) != 1)
    {
        qWarning() << "RAND_bytes failed:" << lastOpenSslError();
        return {};
    }
#endif

    // 输出缓冲区（最多多出一个 block）
#if defined(HAS_COMMON_CRYPTO)
    size_t bufSize = plainData.size() + kCCBlockSizeAES128;
    QByteArray cipherData(static_cast<int>(bufSize), '\0');

    size_t outLen = 0;
    CCCrypt(kCCEncrypt, kCCAlgorithmAES,
            kCCOptionPKCS7Padding,
            key.data(), kCCKeySizeAES256,
            iv.data(),
            plainData.data(), plainData.size(),
            cipherData.data(), bufSize, &outLen);

    cipherData.resize(static_cast<int>(outLen));
#else
    QByteArray cipherData(plainData.size() + EVP_MAX_BLOCK_LENGTH, '\0');
    int outLen1 = 0;
    int outLen2 = 0;

    auto ctx = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(
        EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!ctx)
        return {};

    bool ok = EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr,
                                 reinterpret_cast<const unsigned char *>(key.constData()),
                                 reinterpret_cast<const unsigned char *>(iv.constData())) == 1
           && EVP_EncryptUpdate(ctx.get(),
                                reinterpret_cast<unsigned char *>(cipherData.data()), &outLen1,
                                reinterpret_cast<const unsigned char *>(plainData.constData()), plainData.size()) == 1
           && EVP_EncryptFinal_ex(ctx.get(),
                                  reinterpret_cast<unsigned char *>(cipherData.data()) + outLen1, &outLen2) == 1;
    if (!ok)
    {
        qWarning() << "EVP encryption failed:" << lastOpenSslError();
        return {};
    }

    cipherData.resize(outLen1 + outLen2);
#endif

    // 存储格式：IV + 密文 → Base64
    QByteArray result = iv + cipherData;
    return QString::fromLatin1(result.toBase64());
}

QString CryptoHelper::decrypt(const QString &cipherText)
{
    if (cipherText.isEmpty())
        return {};

    QByteArray key = deriveKey();
    QByteArray raw = QByteArray::fromBase64(cipherText.toLatin1());

    if (raw.size() < 16 + 1)
        return {};   // 格式不对，可能是旧版明文

    QByteArray iv = raw.left(16);
    QByteArray cipherData = raw.mid(16);

#if defined(HAS_COMMON_CRYPTO)
    size_t bufSize = cipherData.size() + kCCBlockSizeAES128;
    QByteArray plainData(static_cast<int>(bufSize), '\0');

    size_t outLen = 0;
    CCCryptorStatus status = CCCrypt(
        kCCDecrypt, kCCAlgorithmAES,
        kCCOptionPKCS7Padding,
        key.data(), kCCKeySizeAES256,
        iv.data(),
        cipherData.data(), cipherData.size(),
        plainData.data(), bufSize, &outLen);

    if (status != kCCSuccess)
        return {};

    plainData.resize(static_cast<int>(outLen));
#else
    QByteArray plainData(cipherData.size() + EVP_MAX_BLOCK_LENGTH, '\0');
    int outLen1 = 0;
    int outLen2 = 0;

    auto ctx = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(
        EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!ctx)
        return {};

    bool ok = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr,
                                 reinterpret_cast<const unsigned char *>(key.constData()),
                                 reinterpret_cast<const unsigned char *>(iv.constData())) == 1
           && EVP_DecryptUpdate(ctx.get(),
                                reinterpret_cast<unsigned char *>(plainData.data()), &outLen1,
                                reinterpret_cast<const unsigned char *>(cipherData.constData()), cipherData.size()) == 1
           && EVP_DecryptFinal_ex(ctx.get(),
                                  reinterpret_cast<unsigned char *>(plainData.data()) + outLen1, &outLen2) == 1;
    if (!ok)
    {
        qWarning() << "EVP decryption failed:" << lastOpenSslError();
        return {};
    }

    plainData.resize(outLen1 + outLen2);
#endif
    return QString::fromUtf8(plainData);
}
