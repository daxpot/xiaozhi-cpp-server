# 小智 ESP-32 C++后端服务 (xz-cpp-server)

本项目为开源智能硬件项目 [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)
提供后端服务。根据 [小智通信协议](https://ccnphfhqs21z.feishu.cn/wiki/M0XiwldO9iJwHikpXD5cEx71nKh) 使用 `C++` 实现。

项目根据Python版本的[xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server)改版

基于BOOST 1.83, C++20协程开发。

---

## 功能清单 ✨

### 已实现 ✅

- **通信协议**  
  基于 `xiaozhi-esp32` 协议，通过 WebSocket 实现数据交互。
- **对话交互**  
  支持唤醒对话、手动对话及实时打断。长时间无对话时自动休眠
- **语音识别**  
  支持国语（默认使用 FunASR的paraformer-zh-streaming模型）。
- **LLM 模块**  
  支持灵活切换 LLM 模块，默认使用 ChatGLMLLM，也可选用Coze, Didy或其他类似openai的接口。
- **TTS 模块**  
  支持 EdgeTTS（默认）、火山引擎豆包 TTS  TTS 接口，满足语音合成需求。

## 本项目支持的平台/组件列表 📋

### LLM 语言模型

| 类型  |        平台名称        |         使用方式          |    收费模式     |                                                           备注                                                            |
|:---:|:------------------:|:---------------------:|:-----------:|:-----------------------------------------------------------------------------------------------------------------------:|
| LLM |   智谱（ChatGLMLLM）   |      openai 接口调用      |     免费      |                            虽然免费，仍需[点击申请密钥](https://bigmodel.cn/usercenter/proj-mgmt/apikeys)                            |
| LLM |      DifyLLM       |       dify 接口调用       | 免费/消耗 token |                                               本地化部署，注意配置提示词需在 Dify 控制台设置 |
| LLM |      CozeLLM       |       coze 接口调用       |  消耗 token   |                                                需提供 bot_id、user_id 及个人令牌             |                                

实际上，任何支持 openai 接口调用的 LLM 均可接入使用。

### VAD 语音活动检测

| 类型  |   平台名称    | 使用方式 | 收费模式 | 备注 |
|:---:|:---------:|:----:|:----:|:--:|
| VAD | SileroVAD | 本地使用 |  免费  |    |

---

### ASR 语音识别

| 类型  |   平台名称    | 使用方式 | 收费模式 | 备注 |
|:---:|:---------:|:----:|:----:|:--:|
| ASR |  FunASR   | 本地使用 |  免费  |    |
| ASR | BytedanceASRV2 | 接口调用 |  收费  |    |

---


### TTS 语音合成

| 类型  |          平台名称          | 使用方式 |   收费模式   |                                    备注                                     |
|:---:|:----------------------:|:----:|:--------:|:-------------------------------------------------------------------------:|
| TTS |        EdgeTTS         | 接口调用 |    免费    |                             默认 TTS，基于微软语音合成技术                             |
| TTS | 火山引擎豆包 TTS (BytedanceTTSV3) | 接口调用 | 消耗 token | [点击创建密钥](https://console.volcengine.com/speech/service/10007) |

---

---

## 使用方式

参看[install_deps.sh](https://github.com/daxpot/xiaozhi-cpp-server/blob/master/scripts/install_deps.sh)安装依赖和构建

启动命令
```
# 修改config.example.yaml为config.yaml，并填写自己的配置信息

./build/apps/web_server
# 或者指定config path
./build/apps/web_server config.yaml
```
