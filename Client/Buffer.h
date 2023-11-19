#pragma once

#include <vector>
#include <iostream>

class Buffer
{
public:
    Buffer(int size = 128)
    {
        m_BufferData.resize(size);
        m_Size = size;
        m_WriteIndex = 0;
        m_ReadIndex = 0;
    }

    ~Buffer() { }

    // Grow the buffer if necessary
    void EnsureCapacity(size_t size)
    {
        if (m_WriteIndex + size > m_Size)
        {
            m_Size = max(m_Size * 2, m_WriteIndex + size);
            m_BufferData.resize(m_Size);
        }
    }

    // Serialize and Deserialize unsigned int (32 bit)
    void WriteUInt32LE(uint32_t value)
    {
        EnsureCapacity(sizeof(uint32_t));
        m_BufferData[m_WriteIndex++] = value >> 24;
        m_BufferData[m_WriteIndex++] = value >> 16;
        m_BufferData[m_WriteIndex++] = value >> 8;
        m_BufferData[m_WriteIndex++] = value;
    }

    uint32_t ReadUInt32LE()
    {
        if (m_ReadIndex + sizeof(uint32_t) > m_Size)
        {
            throw std::runtime_error("Buffer underflow while reading uint32.");
        }

        uint32_t value = m_BufferData[m_ReadIndex++] << 24;
        value |= m_BufferData[m_ReadIndex++] << 16;
        value |= m_BufferData[m_ReadIndex++] << 8;
        value |= m_BufferData[m_ReadIndex++];

        return value;
    }

    // Serialize and Deserialize unsigned short (16 bit)
    void WriteUInt16LE(uint16_t value)
    {
        EnsureCapacity(sizeof(uint16_t));
        m_BufferData[m_WriteIndex++] = value >> 8;
        m_BufferData[m_WriteIndex++] = value;
    }

    uint16_t ReadUInt16LE()
    {
        if (m_ReadIndex + sizeof(uint16_t) > m_Size)
        {
            throw std::runtime_error("Buffer underflow while reading uint16.");
        }

        uint16_t value = m_BufferData[m_ReadIndex++] << 8;
        value |= m_BufferData[m_ReadIndex++];

        return value;
    }

    // Serialize and Deserialize string
    void WriteString(const std::string& str)
    {
        size_t strLength = str.length();
        EnsureCapacity(strLength);
        for (int i = 0; i < strLength; i++)
        {
            m_BufferData[m_WriteIndex++] = str[i];
        }
    }

    std::string ReadString(uint32_t length)
    {
        if (m_ReadIndex + length > m_Size)
        {
            throw std::runtime_error("Buffer underflow while reading string.");
        }

        std::string str;
        for (uint32_t i = 0; i < length; i++)
        {
            str.push_back(m_BufferData[m_ReadIndex++]);
        }
        return str;
    }

    std::vector<uint8_t> m_BufferData;

private:
    int m_Size;
    int m_WriteIndex;
    int m_ReadIndex;
};