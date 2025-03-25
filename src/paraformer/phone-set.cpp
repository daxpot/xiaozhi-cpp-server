#include <iterator>
#include <xz-cpp-server/paraformer/phone-set.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <string>


namespace funasr {
PhoneSet::PhoneSet(const char *filename) {
  ifstream in(filename);
  LoadPhoneSetFromJson(filename);
}
PhoneSet::~PhoneSet()
{
}

void PhoneSet::LoadPhoneSetFromYaml(const char* filename) {
  YAML::Node config;
  try{
    config = YAML::LoadFile(filename);
  }catch(exception const &e){
     BOOST_LOG_TRIVIAL(info) << "Error loading file, yaml file error or not exist.";
     exit(-1);
  }
  YAML::Node myList = config["token_list"];
  int id = 0;
  for (YAML::const_iterator it = myList.begin(); it != myList.end(); ++it, id++) {
    phone_.push_back(it->as<string>());
    phn2Id_.emplace(it->as<string>(), id);
  }
}

void PhoneSet::LoadPhoneSetFromJson(const char* filename) {
    boost::json::value json_array;
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        json_array = boost::json::parse(json_str);
        file.close();
    } else {
        BOOST_LOG_TRIVIAL(info) << "Error loading token file, token file error or not exist.";
        exit(-1);
    }

    int id = 0;
    for (const auto& element : json_array.as_array()) {
        const std::string value(element.as_string());
        phone_.push_back(value);
        phn2Id_.emplace(value, id);
        id++;
    }
}

int PhoneSet::Size() const {
  return phone_.size();
}

int PhoneSet::String2Id(string phn_str) const {
  if (phn2Id_.count(phn_str)) {
    return phn2Id_.at(phn_str);
  } else {
    //BOOST_LOG_TRIVIAL(info) << "Phone unit not exist.";
    return -1;
  }
}

string PhoneSet::Id2String(int id) const {
  if (id < 0 || id > Size()) {
    //BOOST_LOG_TRIVIAL(info) << "Phone id not exist.";
    return "";
  } else {
    return phone_[id];
  }
}

bool PhoneSet::Find(string phn_str) const {
  return phn2Id_.count(phn_str) > 0;
}

int PhoneSet::GetBegSilPhnId() const {
  return String2Id(UNIT_BEG_SIL_SYMBOL);
}

int PhoneSet::GetEndSilPhnId() const {
  return String2Id(UNIT_END_SIL_SYMBOL);
}

int PhoneSet::GetBlkPhnId() const {
  return String2Id(UNIT_BLK_SYMBOL);
}

}