#ifndef CRYPTOHELPER_H
#define CRYPTOHELPER_H

#include <QString>

// ============================================================
// 本地密码加密存储
//
// AES-256-CBC，密钥由机器唯一 ID (QSysInfo::machineUniqueId)
// 通过 SHA-256 派生。密文格式：base64(随机IV + 密文)。
//
// macOS 使用内置 CommonCrypto，无需额外依赖。
// ============================================================
class CryptoHelper
{
public:
    static QString encrypt(const QString &plainText);
    static QString decrypt(const QString &cipherText);

private:
    static QByteArray deriveKey();
};

#endif // CRYPTOHELPER_H
