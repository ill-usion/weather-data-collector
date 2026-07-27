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
    String m_filename;
    OverflowBehavior m_overflowBehavior;

public:
    DataStore() {}

    /// @brief Constructs a new DataStore object with the given file
    /// @param file DataStore file
    /// @param overflowBehavior Overflow behavior
    DataStore(String filename, OverflowBehavior overflowBehavior = OverflowBehavior::Overwrite) : m_filename(filename), m_overflowBehavior(overflowBehavior) {}
    ~DataStore()
    {
    }

    /// @brief Opens a data store
    /// @param filename Data store filename
    /// @param overflowBehavior Overflow behavior
    /// @return A new instance of DataStore
    static DataStore open(const char *filename, OverflowBehavior overflowBehavior = OverflowBehavior::Overwrite)
    {
        return DataStore(String(filename), overflowBehavior);
    }

    /// @brief Clears the data store
    void clear()
    {
        if (LittleFS.exists(m_filename))
            LittleFS.remove(m_filename);
        if (LittleFS.exists(m_filename + ".idx"))
            LittleFS.remove(m_filename + ".idx");
    }

    /// @brief Appends the given data to the data store
    /// @param data Data pointer
    /// @param len Pointer length
    /// @return Number of bytes written
    size_t store(uint8_t *data, size_t len)
    {
        int seekPos = -1;
        bool isOverflowing = len > LittleFS.totalBytes() - LittleFS.usedBytes();
        // Serial.printf("Total bytes: %d, Used bytes: %d, Free bytes: %d\n", LittleFS.totalBytes(), LittleFS.usedBytes(), LittleFS.totalBytes() - LittleFS.usedBytes());
        // Serial.printf("Data length: %d\n", len);
        if (isOverflowing)
        {
            if (m_overflowBehavior == OverflowBehavior::Clear)
            {
                clear();
            }
            else if (m_overflowBehavior == OverflowBehavior::Overwrite)
            {
                seekPos = getOverwriteIndex();
                // Serial.printf("Seek position: %d\n", seekPos);
            }
        }

        File writer = getWriter();
        if (!writer)
        {
            // Serial.println("Failed to write to data store. Writer is not valid.");
            return -1;
        }

        if (seekPos != -1)
            writer.seek(seekPos);

        size_t written = writer.write(data, len);
        writer.close();

        if (isOverflowing &&
            seekPos != -1 &&
            written > 0 &&
            m_overflowBehavior == OverflowBehavior::Overwrite)
        {
            size_t sz = size();
            setOverwriteIdx((seekPos + written) % sz);
        }

        return written;
    }

    /// @brief Size of the data store
    /// @return Size in bytes
    size_t size()
    {
        if (!LittleFS.exists(m_filename))
            return 0;

        File reader = getReader();
        size_t s = reader.size();
        reader.close();
        return s;
    }

    /// @brief Gets a file reader for the data store
    /// @return File reader
    File getReader()
    {
        return LittleFS.open(m_filename, FILE_READ);
    }

    /// @brief Gets a file writer for the data store
    /// @return File writer
    File getWriter()
    {
        return LittleFS.open(m_filename, FILE_WRITE, true);
    }

private:
    uint32_t getOverwriteIndex()
    {
        String fileMetaPath = m_filename + ".idx";
        if (!LittleFS.exists(fileMetaPath))
            return 0;

        File fileMeta = LittleFS.open(fileMetaPath, FILE_READ);
        String content = fileMeta.readString();
        fileMeta.close();
        uint32_t idx = content.toInt();
        return idx;
    }

    size_t setOverwriteIdx(uint32_t idx)
    {
        String fileMetaPath = m_filename + ".idx";
        File fileMeta = LittleFS.open(fileMetaPath, FILE_WRITE, true);
        size_t s = fileMeta.print(idx);
        fileMeta.close();
        return s;
    }
};