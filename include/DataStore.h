#pragma once

#include <LittleFS.h>

enum class OverflowBehavior
{
    // Clear the data store when there is no space left
    Clear,
    // Overwrite old data when there is no space left
    Overwrite
};

class DataStore
{
private:
    File m_file;
    OverflowBehavior m_overflowBehavior;

public:
    DataStore() {}

    /// @brief Constructs a new DataStore object with the given file
    /// @param file DataStore file
    /// @param overflowBehavior Overflow behavior
    DataStore(File file, OverflowBehavior overflowBehavior = OverflowBehavior::Overwrite) : m_file(file), m_overflowBehavior(overflowBehavior) {}
    ~DataStore()
    {
        m_file.close();
    }

    /// @brief Opens a data store
    /// @param filename Data store filename
    /// @param overflowBehavior Overflow behavior
    /// @return A new instance of DataStore
    static DataStore open(const char *filename, OverflowBehavior overflowBehavior = OverflowBehavior::Overwrite)
    {
        return DataStore(LittleFS.open(filename, "a", true));
    }

    /// @brief Clears the data store
    void clear()
    {
        LittleFS.remove(m_file.path());
    }

    /// @brief Appends the given data to the data store
    /// @param data Data pointer
    /// @param len Pointer length
    /// @return Number of bytes written
    size_t store(uint8_t *data, size_t len)
    {
        if (len > LittleFS.totalBytes() - LittleFS.usedBytes())
        {
            if (m_overflowBehavior == OverflowBehavior::Clear)
            {
                clear();
                m_file = LittleFS.open(m_file.path(), "a", true);
            }
            else if (m_overflowBehavior == OverflowBehavior::Overwrite)
            {
                m_file.seek(0);
            }
        }
        return m_file.write(data, len);
    }

    /// @brief Size of the data store
    /// @return Size in bytes
    size_t size()
    {
        return m_file.size();
    }

    /// @brief The underlying DataStore file
    /// @return DataStore file
    File *getFilePtr()
    {
        return &m_file;
    }
};