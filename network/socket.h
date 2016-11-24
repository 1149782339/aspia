/*
* PROJECT:         Aspia Remote Desktop
* FILE:            network/socket.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_NETWORK__SOCKET_H
#define _ASPIA_NETWORK__SOCKET_H

#include "aspia_config.h"

#include <string>
#include <stdint.h>
#include <memory>

#include "base/exception.h"
#include "base/macros.h"
#include "base/mutex.h"

#include "crypto/encryptor_aes.h"

class Socket
{
public:
    Socket();
    virtual ~Socket();

    virtual void Connect(const char *hostname, int port) = 0;
    virtual int Write(const uint8_t *buf, int len) = 0;
    virtual int Read(uint8_t *buf, int len) = 0;
    virtual void Bind(const char *hostname, int port) = 0;
    virtual void Listen() = 0;
    virtual std::unique_ptr<Socket> Accept() = 0;
    virtual void Disconnect() = 0;
    virtual std::string GetIpAddress() = 0;

    // Disable or enable Nagle�s algorithm.
    virtual void SetNoDelay(bool enable) = 0;

    static bool IsValidHostName(const char *hostname);
    static bool IsValidPort(int port);

    template<class T>
    void WriteMessage(const T *message)
    {
        uint32_t size = message->ByteSize();

        if (!size)
        {
            Disconnect();
            throw Exception("Wrong packet size.");
        }

        LockGuard<Mutex> guard(&write_lock_);

        if (write_buffer_size_ < size)
        {
            write_buffer_size_ = size;
            write_buffer_.reset(new uint8_t[size]);
        }

        if (!message->SerializeToArray(write_buffer_.get(), size))
        {
            Disconnect();
            throw Exception("Unable to serialize the message.");
        }

        uint8_t *data = nullptr;

        if (!crypto_->Encrypt(write_buffer_.get(), size, &data, &size))
        {
            Disconnect();
            throw Exception("Unable to encrypt the message.");
        }

        //
        // ���������� ������������ ������ TCP_CORK �� Linux.
        // TCP �� ����������� ����������� �������� ����� ��������� �� ���� �����
        // send() � ��� �������� ����� ��������� ������ ����� ���������� ������
        // ��������� ���������� ���. �� ��������� ��� TCP ���������� �������
        // �������� ������, ������� ����������� ������ � ������� �� �������� ���
        // ����, ����� �������� ������ �������� TCP-���������� (������� ������ �
        // ����������� IP ����� ������ 40 ����).
        // ��� ����������� �������� �������� ������ ������ ���� ��������.
        // �� ����� �������� ������ ����������� ��������� �� �������� ��������
        // ������, ����� ��������� ���� ���������� ���������� ����������� TCP-
        // �������.
        // ����� �������� ����������� ��������� �� �������� �������� ������ �
        // �������� send(), ����� ��������� ������ � ������� ���������.
        //

        // �������� �������� ������
        SetNoDelay(true);

        // ���������� ������ ������ � ���� ������.
        Writer(reinterpret_cast<const uint8_t*>(&size), sizeof(size));
        Writer(data, size);

        // ��������� �������� ������.
        SetNoDelay(false);

        //
        // �������� �������� ������ � ������� �������� ������ ���
        // �������� ������� � �������.
        //
        Write(reinterpret_cast<const uint8_t*>(""), 0);
    }

    template<class T>
    void ReadMessage(std::unique_ptr<T> *message)
    {
        uint32_t size = 0;

        Reader(reinterpret_cast<uint8_t*>(&size), sizeof(size));

        if (!size)
        {
            Disconnect();
            throw Exception("Serialized message size is equal to zero.");
        }

        if (read_buffer_size_ < size)
        {
            read_buffer_size_ = size;
            read_buffer_.reset(new uint8_t[size]);
        }

        Reader(read_buffer_.get(), size);

        uint8_t *data = nullptr;

        if (!crypto_->Decrypt(read_buffer_.get(), size, &data, &size))
        {
            Disconnect();
            throw Exception("Unable to decrypt the message.");
        }

        if (!message->get()->ParseFromArray(data, size))
        {
            Disconnect();
            throw Exception("Unable to parse the message.");
        }
    }

private:
    void Reader(uint8_t *buf, int len);
    void Writer(const uint8_t *buf, int len);
    static bool IsHostnameChar(char c);

private:
    uint32_t write_buffer_size_;
    uint32_t read_buffer_size_;

    std::unique_ptr<uint8_t[]> write_buffer_;
    std::unique_ptr<uint8_t[]> read_buffer_;

    Mutex write_lock_;

    std::unique_ptr<Encryptor> crypto_;

    DISALLOW_COPY_AND_ASSIGN(Socket);
};

#endif // _ASPIA_NETWORK__SOCKET_H
