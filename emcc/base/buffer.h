#ifndef _BASE_BUFFER_H_
#define _BASE_BUFFER_H_

#include "base/common.h"

template<class T>
class MyAlloc {
public:
    MyAlloc() {}
    virtual ~MyAlloc() {
        delete m_buffer;
        m_buffer = NULL;
    }
    T* get(size_t size) {
        if (size > m_size) {
            delete m_buffer;
            m_buffer = new T[size];
            m_size = size;
        }
        return m_buffer;
    }

private:
    T* m_buffer = NULL;
    size_t m_size = 0;
};

class MySamples {
public:
    MySamples() {}
    size_t size() {
        return m_data.size();
    }
    Int16Array &data() {
        return m_data;
    }
    int16_t *data(size_t offset) {
        if (offset >= 0 && offset < m_data.size()) {
            return &m_data[offset];
        } else {
            return NULL;
        }
    }
    void append(const int16_t* data, size_t size, uint32_t now) {
        update_time(now);
        m_data.insert(m_data.end(), data, data + size);
    }
    void set(const int16_t* data, size_t size, uint32_t first, uint32_t last) {
        m_data.assign(data, data + size);
        m_firstTime = first;
        m_lastTime = last;
    }
    void erase(size_t start, size_t end) {
        if (start < 0 || start >= m_data.size()) return;
        if (end == -1) end = m_data.size();
        if (end <= start) return;
        end = std::min(end, m_data.size());
        auto it = m_data.begin();
        m_data.erase(it+start, it+end);
    }
    void update_time(uint32_t now) {
        if (m_data.empty()) m_firstTime = now;
        m_lastTime = now;
    }
    uint32_t first_time() {
        return m_firstTime;
    }
    uint32_t last_time() {
        return m_lastTime;
    }
private:
    uint32_t m_firstTime = 0;
    uint32_t m_lastTime = 0;
    Int16Array m_data;
}; 

class MyPacket {
public:
    MyPacket() {}
    size_t size() {
        return m_data.size();
    }
    Uint8Array &data() {
        return m_data;
    }
    void set(const char *data, size_t size, uint32_t time) {
        m_data.assign(data, data+size);
        m_time = time;
    }
    uint32_t time() {
        return m_time;
    }

private:
    uint32_t m_time = 0;
    Uint8Array m_data;
};

#endif // _BASE_BUFFER_H_
