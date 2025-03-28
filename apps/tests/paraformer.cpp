#include <funasr/paraformer/model.h>
#include <xz-cpp-server/common/logger.h>
#include <opus/opus.h>
#include <fstream>

int main() {
    init_logging("DEBUG");
    std::map<std::string, std::string> model_path = {
        {"model-dir", "models/speech_paraformer-large_asr_nat-zh-cn-16k-common-vocab8404-online"},
        {"quantize", "true"}
    };
    auto offline_model = funasr::CreateModel(model_path, 1);
    std::vector<int> chunk_size = {5, 10, 5};
    auto online_model = funasr::CreateModel(offline_model, chunk_size);
    int error;
    OpusDecoder* decoder = opus_decoder_create(16000, 1, &error);
    std::string full_result;
    std::vector<float> pcm_data(960*10);
    for(size_t index=0; index <= 92; index++) {
        std::ifstream file(std::format("tmp/example/opus_data_{}.opus", index), std::ifstream::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        int frame_size = opus_decode_float(decoder, data.data(), data.size(), pcm_data.data() + (index%10)*960, 960, 0);
        if(frame_size < 0) {
            BOOST_LOG_TRIVIAL(error) << "Opus decode error:" << opus_strerror(frame_size);
            return -1;
        }
        if(index % 10 == 9) {
            bool is_final = (index / 10 == 8); // 索引从 0 到 92，共 93 段
            std::string result = online_model->Forward(pcm_data.data(), pcm_data.size(), is_final);
            full_result += result;
            BOOST_LOG_TRIVIAL(info) << "Chunk " << index << " result: " << result;
        }
    }
    BOOST_LOG_TRIVIAL(info) << "Full result: " << full_result;

    delete offline_model;
    delete online_model;
}