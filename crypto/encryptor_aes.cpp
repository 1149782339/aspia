/*
* PROJECT:         Aspia Remote Desktop
* FILE:            crypto/encryptor_aes.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "crypto/encryptor_aes.h"

// ������ ����� ���������� � �����.
static const DWORD kKeySize = 256;

// ������ ������� ������������� ����� � ������.
static const DWORD kIvSize = 16;

EncryptorAES::EncryptorAES(const std::string &password) :
    prov_(NULL),
    enc_key_(NULL),
    dec_key_(NULL),
    enc_buffer_size_(0),
    dec_buffer_size_(0),
    has_enc_session_key_(false),
    has_dec_session_key_(false)
{
    // ������� �������� ����������.
    if (!CryptAcquireContextW(&prov_,
                              NULL,
                              MS_ENH_RSA_AES_PROV_W,
                              PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
    {
        LOG(ERROR) << "CryptAcquireContextW() failed: " << GetLastError();
        Cleanup();
        return;
    }

    // ������� ��������� ����� ��� ���������� � �������������� ������.
    enc_key_ = CreateKeyFromPassword(prov_, password);
    if (!enc_key_)
    {
        Cleanup();
        return;
    }

    //
    // �� �� ����� ������������ ���� � ��� �� ��������� ����� ��� ���������� � ������������
    // � ������� �������� �����.
    // CBC ����� ���������� ������������ ����������� ����������� ������������� ������ ��
    // ���������� � ������ � ����� �������� ���������� � ���������� ���������.
    // ������������� ������ � ���� �� ���������� ����� �������� � ����������� ������ � ������
    // NTE_BAD_DATA ��� ����������.
    //
    if (!CryptDuplicateKey(enc_key_, NULL, 0, &dec_key_))
    {
        LOG(ERROR) << "CryptDuplicateKey() failed: " << GetLastError();
        Cleanup();
        return;
    }

    DWORD block_size_len = sizeof(block_size_);

    // �������� ������ ����� ���������� � �����.
    if (!CryptGetKeyParam(enc_key_,
                          KP_BLOCKLEN,
                          reinterpret_cast<BYTE*>(&block_size_),
                          &block_size_len,
                          0))
    {
        LOG(ERROR) << "CryptGetKeyParam(KP_BLOCKLEN) failed: " << GetLastError();
        Cleanup();
        return;
    }

    // ����������� ���� � �����.
    block_size_ /= 8;
}

EncryptorAES::~EncryptorAES()
{
    Cleanup();
}

void EncryptorAES::Cleanup()
{
    //
    // ���������� ����� ����������. ����� ������ ���� ���������� �� �����������
    // ��������� ����������.
    //
    if (enc_key_)
    {
        // ���������� ���� ��� ����������.
        if (!CryptDestroyKey(enc_key_))
        {
            DLOG(ERROR) << "CryptDestroyKey() failed: " << GetLastError();
        }

        enc_key_ = NULL;
    }

    if (dec_key_)
    {
        // ���������� ���� ��� ������������.
        if (!CryptDestroyKey(dec_key_))
        {
            DLOG(ERROR) << "CryptDestroyKey() failed: " << GetLastError();
        }

        dec_key_ = NULL;
    }

    if (prov_)
    {
        // ���������� �������� ����������.
        if (!CryptReleaseContext(prov_, 0))
        {
            DLOG(ERROR) << "CryptReleaseContext() failed: " << GetLastError();
        }

        prov_ = NULL;
    }
}

// static
HCRYPTKEY EncryptorAES::CreateKeyFromPassword(HCRYPTPROV provider,
                                              const std::string &password)
{
    // � ������ AES ���������� ������ ����� ������ ���� 128, 192 ��� 256 ���.
    static_assert(kKeySize == 128 || kKeySize == 192 || kKeySize == 256,
                  "Not supported key size");

    HCRYPTHASH hash;

    // ������� SHA512 ���.
    if (!CryptCreateHash(provider, CALG_SHA_512, 0, 0, &hash))
    {
        LOG(ERROR) << "CryptCreateHash() failed: " << GetLastError();
        return NULL;
    }

    // �������� ������.
    if (!CryptHashData(hash,
                       reinterpret_cast<const BYTE*>(password.c_str()),
                       password.size(),
                       0))
    {
        LOG(ERROR) << "CryptHashData() failed: " << GetLastError();
        CryptDestroyHash(hash);
        return NULL;
    }

    ALG_ID algorithm = 0;
    DWORD flags = 0;

    switch (kKeySize)
    {
        case 128:
            algorithm = CALG_AES_128;
            flags |= (128 << 16);
            break;

        case 192:
            algorithm = CALG_AES_192;
            flags |= (192 << 16);
            break;

        case 256:
            algorithm = CALG_AES_256;
            flags |= (256 << 16);
            break;
    }

    HCRYPTKEY key = NULL;

    // ������� ��������� ����� � �������������� SHA512 ����.
    if (!CryptDeriveKey(provider, algorithm, hash, flags, &key))
    {
        LOG(ERROR) << "CryptDeriveKey() failed: " << GetLastError();
        CryptDestroyHash(hash);
        return NULL;
    }

    CryptDestroyHash(hash);

    DWORD mode = CRYPT_MODE_CBC;

    // ������������� ����� ���������� ��� �����.
    if (!CryptSetKeyParam(key, KP_MODE, reinterpret_cast<BYTE*>(&mode), 0))
    {
        LOG(ERROR) << "CryptSetKeyParam(KP_MODE) failed: " << GetLastError();

        // ���������� ���� ����������.
        if (!CryptDestroyKey(key))
        {
            DLOG(ERROR) << "CryptDestroyKey() failed: " << GetLastError();
        }

        return NULL;
    }

    return key;
}

bool EncryptorAES::Encrypt(const uint8_t *in, uint32_t in_len,
                           uint8_t **out, uint32_t *out_len)
{
    //
    // ���� ���������� �� ����� ������� ������������� �����, �� ��������� �
    // ������� ��������������� ��������� ������ ������� �������������.
    //
    DWORD header_size = ((!has_enc_session_key_) ? kIvSize : 0);

    // ���������� ������, ������� ��������� ��� ������������� ������.
    DWORD enc_size = ((in_len / block_size_ + 1) * block_size_) + header_size;

    // ���� ��� ���������� ������ ������, ��� ���������.
    if (enc_buffer_size_ < enc_size)
    {
        enc_buffer_size_ = enc_size;

        // ������������������ ����� ��� ��������.
        enc_buffer_.reset(new uint8_t[enc_size]);

        // �������� ������.
        SecureZeroMemory(enc_buffer_.get(), enc_size);
    }

    //
    // ��� ���������� ������� ��������� ������ �� ������ ���������� ���������� ����
    // (������ ������������� �����) � ��������� ��� � ������ ������� ���������.
    // ������ ���� ���������� ��� ������ �� ������ ������.
    //

    // ���� �������� �� ����� ����������� �����.
    if (!has_enc_session_key_)
    {
        // �������� ������ ��� ������� �������������.
        std::unique_ptr<uint8_t[]> iv(new uint8_t[kIvSize]);

        // ��������� �� ������ �� ���������� ���������� ����������.
        SecureZeroMemory(iv.get(), kIvSize);

        // ��������� ������ ������������� ���������� ����������.
        if (!CryptGenRandom(prov_, kIvSize, iv.get()))
        {
            LOG(ERROR) << "CryptGenRandom() failed: " << GetLastError();
            return false;
        }

        // �������� ������ ������������� � ����� ��������������� ���������.
        memcpy(enc_buffer_.get(), iv.get(), kIvSize);

        // ������������� ������ ������������� ��� �����.
        if (!CryptSetKeyParam(enc_key_, KP_IV, iv.get(), 0))
        {
            LOG(ERROR) << "CryptSetKeyParam() failed: " << GetLastError();
            return false;
        }

        // ����� ��������� �� ������ ����� �������������.
        SecureZeroMemory(iv.get(), kIvSize);

        has_enc_session_key_ = true;
    }

    // �������� �������� ����� � ����� ��� ����������.
    memcpy(enc_buffer_.get() + header_size, in, in_len);

    DWORD size = in_len;

    // ��������� ���������� ������.
    if (!CryptEncrypt(enc_key_,
                      NULL,
                      TRUE,
                      0,
                      enc_buffer_.get() + header_size,
                      &size,
                      enc_size - header_size))
    {
        LOG(ERROR) << "CryptEncrypt() failed: " << GetLastError();
        return false;
    }

    // �������������� �������� ���������.
    *out = enc_buffer_.get();
    *out_len = size + header_size;

    return true;
}

bool EncryptorAES::Decrypt(const uint8_t *in, uint32_t in_len,
                           uint8_t **out, uint32_t *out_len)
{
    //
    // ���� ������ ��� ������������������� ������ ������, ��� ������
    // �������������� ���������.
    //
    if (dec_buffer_size_ < in_len)
    {
        dec_buffer_size_ = in_len;

        // ������������������ ����� ��� ������������.
        dec_buffer_.reset(new uint8_t[in_len]);

        // �������� ������ ������.
        SecureZeroMemory(dec_buffer_.get(), in_len);
    }

    DWORD header_size = 0;

    // ���� ���������� �� ����� ����������� �����.
    if (!has_dec_session_key_)
    {
        // �������� ������ ��� ������� �������������.
        std::unique_ptr<uint8_t[]> iv(new uint8_t[kIvSize]);

        // �������� ������ ������������� �� ���������.
        memcpy(iv.get(), in, kIvSize);

        // ������������� ������ ������������� ��� �����.
        if (!CryptSetKeyParam(dec_key_, KP_IV, iv.get(), 0))
        {
            LOG(ERROR) << "CryptSetKeyParam() failed: " << GetLastError();
            return false;
        }

        // ��������� ������ ������ ����� �������������.
        SecureZeroMemory(iv.get(), kIvSize);

        header_size = kIvSize;

        has_dec_session_key_ = true;
    }

    DWORD length = in_len - header_size;

    // �������� ������������� ��������� � ����� ��� �����������.
    memcpy(dec_buffer_.get(), in + header_size, length);

    // �������������� ���������.
    if (!CryptDecrypt(dec_key_, NULL, TRUE, 0, dec_buffer_.get(), &length))
    {
        LOG(ERROR) << "CryptDecrypt() failed: " << GetLastError();
        return false;
    }

    // �������������� �������� ���������.
    *out = dec_buffer_.get();
    *out_len = length;

    return true;
}
