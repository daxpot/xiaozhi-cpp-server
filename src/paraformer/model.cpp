#include <xz-cpp-server/paraformer/model.h>
#include <xz-cpp-server/paraformer/paraformer-online.h>
#include <xz-cpp-server/paraformer/paraformer.h>
#include <xz-cpp-server/paraformer/utils.h>

namespace funasr {
Model *CreateModel(std::map<std::string, std::string>& model_path, int thread_num)
{
    std::string en_model_path;
    std::string de_model_path;
    std::string am_cmvn_path;
    std::string am_config_path;
    std::string token_path;

    en_model_path = PathAppend(model_path.at(MODEL_DIR), ENCODER_NAME);
    de_model_path = PathAppend(model_path.at(MODEL_DIR), DECODER_NAME);
    if(model_path.find(QUANTIZE) != model_path.end() && model_path.at(QUANTIZE) == "true"){
        en_model_path = PathAppend(model_path.at(MODEL_DIR), QUANT_ENCODER_NAME);
        de_model_path = PathAppend(model_path.at(MODEL_DIR), QUANT_DECODER_NAME);
    }
    am_cmvn_path = PathAppend(model_path.at(MODEL_DIR), AM_CMVN_NAME);
    am_config_path = PathAppend(model_path.at(MODEL_DIR), AM_CONFIG_NAME);
    token_path = PathAppend(model_path.at(MODEL_DIR), TOKEN_PATH);

    Model *mm;
    mm = new Paraformer();
    mm->InitAsr(en_model_path, de_model_path, am_cmvn_path, am_config_path, token_path, thread_num);
    return mm;
}

Model *CreateModel(void* asr_handle, std::vector<int> chunk_size)
{
    Model* mm;
    mm = new ParaformerOnline((Paraformer*)asr_handle, chunk_size);
    return mm;
}

} // namespace funasr