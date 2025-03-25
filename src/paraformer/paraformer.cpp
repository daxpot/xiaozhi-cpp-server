/**
 * Copyright FunASR (https://github.com/alibaba-damo-academy/FunASR). All Rights Reserved.
 * MIT License  (https://opensource.org/licenses/MIT)
*/

#include "funasr/paraformer/paraformer.h"
#include "funasr/paraformer/commonfunc.h"
#include <fstream>
#include <yaml-cpp/yaml.h>
#include "funasr/paraformer/utils.h"

namespace funasr {

Paraformer::Paraformer()
:use_hotword(false),
 env_(ORT_LOGGING_LEVEL_ERROR, "paraformer"),session_options_{},
 hw_env_(ORT_LOGGING_LEVEL_ERROR, "paraformer_hw"),hw_session_options{} {
}

// online
void Paraformer::InitAsr(const std::string &en_model, const std::string &de_model, const std::string &am_cmvn, const std::string &am_config, const std::string &token_file, int thread_num){
    
    LoadOnlineConfigFromYaml(am_config.c_str());
    // knf options
    fbank_opts_.frame_opts.dither = 0;
    fbank_opts_.mel_opts.num_bins = n_mels;
    fbank_opts_.frame_opts.samp_freq = asr_sample_rate;
    fbank_opts_.frame_opts.window_type = window_type;
    fbank_opts_.frame_opts.frame_shift_ms = frame_shift;
    fbank_opts_.frame_opts.frame_length_ms = frame_length;
    fbank_opts_.energy_floor = 0;
    fbank_opts_.mel_opts.debug_mel = false;

    // session_options_.SetInterOpNumThreads(1);
    session_options_.SetIntraOpNumThreads(thread_num);
    session_options_.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    // DisableCpuMemArena can improve performance
    session_options_.DisableCpuMemArena();

    try {
        encoder_session_ = std::make_unique<Ort::Session>(env_, ORTSTRING(en_model).c_str(), session_options_);
        BOOST_LOG_TRIVIAL(info) << "Successfully load model from " << en_model;
    } catch (std::exception const &e) {
        BOOST_LOG_TRIVIAL(error) << "Error when load am encoder model: " << e.what();
        exit(-1);
    }

    try {
        decoder_session_ = std::make_unique<Ort::Session>(env_, ORTSTRING(de_model).c_str(), session_options_);
        BOOST_LOG_TRIVIAL(info) << "Successfully load model from " << de_model;
    } catch (std::exception const &e) {
        BOOST_LOG_TRIVIAL(error) << "Error when load am decoder model: " << e.what();
        exit(-1);
    }

    // encoder
    std::string strName;
    GetInputName(encoder_session_.get(), strName);
    en_strInputNames.push_back(strName.c_str());
    GetInputName(encoder_session_.get(), strName,1);
    en_strInputNames.push_back(strName);
    
    GetOutputName(encoder_session_.get(), strName);
    en_strOutputNames.push_back(strName);
    GetOutputName(encoder_session_.get(), strName,1);
    en_strOutputNames.push_back(strName);
    GetOutputName(encoder_session_.get(), strName,2);
    en_strOutputNames.push_back(strName);

    for (auto& item : en_strInputNames)
        en_szInputNames_.push_back(item.c_str());
    for (auto& item : en_strOutputNames)
        en_szOutputNames_.push_back(item.c_str());

    // decoder
    int de_input_len = 4 + fsmn_layers;
    int de_out_len = 2 + fsmn_layers;
    for(int i=0;i<de_input_len; i++){
        GetInputName(decoder_session_.get(), strName, i);
        de_strInputNames.push_back(strName.c_str());
    }

    for(int i=0;i<de_out_len; i++){
        GetOutputName(decoder_session_.get(), strName,i);
        de_strOutputNames.push_back(strName);
    }

    for (auto& item : de_strInputNames)
        de_szInputNames_.push_back(item.c_str());
    for (auto& item : de_strOutputNames)
        de_szOutputNames_.push_back(item.c_str());

    vocab = new Vocab(token_file.c_str());
    // phone_set_ = new PhoneSet(token_file.c_str());
    LoadCmvn(am_cmvn.c_str());
}

void Paraformer::LoadOnlineConfigFromYaml(const char* filename){

    YAML::Node config;
    try{
        config = YAML::LoadFile(filename);
    }catch(std::exception const &e){
        BOOST_LOG_TRIVIAL(error) << "Error loading file, yaml file error or not exist.";
        exit(-1);
    }

    try{
        YAML::Node frontend_conf = config["frontend_conf"];
        YAML::Node encoder_conf = config["encoder_conf"];
        YAML::Node decoder_conf = config["decoder_conf"];
        YAML::Node predictor_conf = config["predictor_conf"];

        this->window_type = frontend_conf["window"].as<std::string>();
        this->n_mels = frontend_conf["n_mels"].as<int>();
        this->frame_length = frontend_conf["frame_length"].as<int>();
        this->frame_shift = frontend_conf["frame_shift"].as<int>();
        this->lfr_m = frontend_conf["lfr_m"].as<int>();
        this->lfr_n = frontend_conf["lfr_n"].as<int>();

        this->encoder_size = encoder_conf["output_size"].as<int>();
        this->fsmn_dims = encoder_conf["output_size"].as<int>();

        this->fsmn_layers = decoder_conf["num_blocks"].as<int>();
        this->fsmn_lorder = decoder_conf["kernel_size"].as<int>()-1;

        this->cif_threshold = predictor_conf["threshold"].as<double>();
        this->tail_alphas = predictor_conf["tail_threshold"].as<double>();

        this->asr_sample_rate = frontend_conf["fs"].as<int>();


    }catch(std::exception const &e){
        BOOST_LOG_TRIVIAL(error) << "Error when load argument from vad config YAML.";
        exit(-1);
    }
}


Paraformer::~Paraformer()
{
    if(vocab){
        delete vocab;
    }
    if(lm_vocab){
        delete lm_vocab;
    }
    if(seg_dict){
        delete seg_dict;
    }
    if(phone_set_){
        delete phone_set_;
    }
}

void Paraformer::StartUtterance()
{
}

void Paraformer::EndUtterance()
{
}

void Paraformer::Reset()
{
}


void Paraformer::LoadCmvn(const char *filename)
{
    std::ifstream cmvn_stream(filename);
    if (!cmvn_stream.is_open()) {
        BOOST_LOG_TRIVIAL(error) << "Failed to open file: " << filename;
        exit(-1);
    }
    std::string line;

    while (getline(cmvn_stream, line)) {
        std::istringstream iss(line);
        std::vector<std::string> line_item{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
        if (line_item[0] == "<AddShift>") {
            getline(cmvn_stream, line);
            std::istringstream means_lines_stream(line);
            std::vector<std::string> means_lines{std::istream_iterator<std::string>{means_lines_stream}, std::istream_iterator<std::string>{}};
            if (means_lines[0] == "<LearnRateCoef>") {
                for (int j = 3; j < means_lines.size() - 1; j++) {
                    means_list_.push_back(stof(means_lines[j]));
                }
                continue;
            }
        }
        else if (line_item[0] == "<Rescale>") {
            getline(cmvn_stream, line);
            std::istringstream vars_lines_stream(line);
            std::vector<std::string> vars_lines{std::istream_iterator<std::string>{vars_lines_stream}, std::istream_iterator<std::string>{}};
            if (vars_lines[0] == "<LearnRateCoef>") {
                for (int j = 3; j < vars_lines.size() - 1; j++) {
                    vars_list_.push_back(stof(vars_lines[j])*scale);
                }
                continue;
            }
        }
    }
}

std::string Paraformer::GreedySearch(float * in, int n_len,  int64_t token_nums, bool is_stamp, std::vector<float> us_alphas, std::vector<float> us_cif_peak)
{
    std::vector<int> hyps;
    int Tmax = n_len;
    for (int i = 0; i < Tmax; i++) {
        int max_idx;
        float max_val;
        FindMax(in + i * token_nums, token_nums, max_val, max_idx);
        hyps.push_back(max_idx);
    }
    return vocab->Vector2StringV2(hyps, language);
}


Vocab* Paraformer::GetVocab()
{
    return vocab;
}

Vocab* Paraformer::GetLmVocab()
{
    return lm_vocab;
}

PhoneSet* Paraformer::GetPhoneSet()
{
    return phone_set_;
}

std::string Paraformer::Rescoring()
{
    BOOST_LOG_TRIVIAL(error)<<"Not Imp!!!!!!";
    return "";
}
} // namespace funasr