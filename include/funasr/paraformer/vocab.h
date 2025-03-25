
#ifndef VOCAB_H
#define VOCAB_H

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <boost/json.hpp>

namespace funasr {
class Vocab {
  private:
    std::vector<std::string> vocab;
    std::map<std::string, int> token_id;
    std::map<std::string, std::string> lex_map;
    bool IsEnglish(std::string ch);
    void LoadVocabFromYaml(const char* filename);
    void LoadVocabFromJson(const char* filename);
    void LoadLex(const char* filename);

  public:
    Vocab(const char *filename);
    Vocab(const char *filename, const char *lex_file);
    ~Vocab();
    int Size() const;
    bool IsChinese(std::string ch);
    void Vector2String(std::vector<int> in, std::vector<std::string> &preds);
    std::string Vector2String(std::vector<int> in);
    std::string Vector2StringV2(std::vector<int> in, std::string language="");
    std::string Id2String(int id) const;
    std::string WordFormat(std::string word);
    int GetIdByToken(const std::string &token) const;
    std::string Word2Lex(const std::string &word) const;
};

} // namespace funasr
#endif