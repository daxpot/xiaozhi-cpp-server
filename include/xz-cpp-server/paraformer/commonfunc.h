#pragma once 
#include <algorithm>
#include "model.h"

namespace funasr {
typedef struct
{
    std::string msg;
    std::string stamp;
    std::string stamp_sents;
    std::string tpass_msg;
    float snippet_time;
}FUNASR_RECOG_RESULT;

typedef struct
{
    std::vector<std::vector<int>>* segments;
    float  snippet_time;
}FUNASR_VAD_RESULT;

typedef struct
{
    std::string msg;
    std::vector<std::string> arr_cache;
}FUNASR_PUNC_RESULT;


#define ORTSTRING(str) str
#define ORTCHAR(str) str

inline void GetInputName(Ort::Session* session, std::string& inputName,int nIndex=0) {
    size_t numInputNodes = session->GetInputCount();
    if (numInputNodes > 0) {
        Ort::AllocatorWithDefaultOptions allocator;
        {
            auto t = session->GetInputNameAllocated(nIndex, allocator);
            inputName = t.get();
        }
    }
}

inline void GetInputNames(Ort::Session* session, std::vector<std::string> &m_strInputNames,
                   std::vector<const char *> &m_szInputNames) {
    Ort::AllocatorWithDefaultOptions allocator;
    size_t numNodes = session->GetInputCount();
    m_strInputNames.resize(numNodes);
    m_szInputNames.resize(numNodes);
    for (size_t i = 0; i != numNodes; ++i) {    
        auto t = session->GetInputNameAllocated(i, allocator);
        m_strInputNames[i] = t.get();
        m_szInputNames[i] = m_strInputNames[i].c_str();
    }
}

inline void GetOutputName(Ort::Session* session, std::string& outputName, int nIndex = 0) {
    size_t numOutputNodes = session->GetOutputCount();
    if (numOutputNodes > 0) {
        Ort::AllocatorWithDefaultOptions allocator;
        {
            auto t = session->GetOutputNameAllocated(nIndex, allocator);
            outputName = t.get();
        }
    }
}

inline void GetOutputNames(Ort::Session* session, std::vector<std::string> &m_strOutputNames,
                   std::vector<const char *> &m_szOutputNames) {
    Ort::AllocatorWithDefaultOptions allocator;
    size_t numNodes = session->GetOutputCount();
    m_strOutputNames.resize(numNodes);
    m_szOutputNames.resize(numNodes);
    for (size_t i = 0; i != numNodes; ++i) {    
        auto t = session->GetOutputNameAllocated(i, allocator);
        m_strOutputNames[i] = t.get();
        m_szOutputNames[i] = m_strOutputNames[i].c_str();
    }
}

template <class ForwardIterator>
inline static size_t Argmax(ForwardIterator first, ForwardIterator last) {
    return std::distance(first, std::max_element(first, last));
}
} // namespace funasr