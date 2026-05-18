#include "cryptohelper.h"

#include <QSysInfo>
#include <QCryptographicHash>
#include <QRandomGenerator>

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <CommonCrypto/CommonCryptor.h>
#  define HAS_COMMON_CRYPTO 1
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
    for (int i = 0; i < 16; ++i)
        iv[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));

    // 输出缓冲区（最多多出一个 block）
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
    return QString::fromUtf8(plainData);
}
