import websockets
import asyncio
import opuslib_next
from pydub import AudioSegment
import os
import numpy as np
import json
import time

ws_url = "ws://127.0.0.1:8000"

def wav_to_opus_data(wav_file_path):
        # 使用pydub加载PCM文件
        # 获取文件后缀名
        file_type = os.path.splitext(wav_file_path)[1]
        if file_type:
            file_type = file_type.lstrip('.')
        audio = AudioSegment.from_file(wav_file_path, format=file_type)

        duration = len(audio) / 1000.0

        # 转换为单声道和16kHz采样率（确保与编码器匹配）
        audio = audio.set_channels(1).set_frame_rate(16000)

        # 获取原始PCM数据（16位小端）
        raw_data = audio.raw_data

        # 初始化Opus编码器
        encoder = opuslib_next.Encoder(16000, 1, opuslib_next.APPLICATION_AUDIO)

        # 编码参数
        frame_duration = 60  # 60ms per frame
        frame_size = int(16000 * frame_duration / 1000)  # 960 samples/frame

        opus_datas = []
        # 按帧处理所有音频数据（包括最后一帧可能补零）
        for i in range(0, len(raw_data), frame_size * 2):  # 16bit=2bytes/sample
            # 获取当前帧的二进制数据
            chunk = raw_data[i:i + frame_size * 2]

            # 如果最后一帧不足，补零
            if len(chunk) < frame_size * 2:
                chunk += b'\x00' * (frame_size * 2 - len(chunk))

            # 转换为numpy数组处理
            np_frame = np.frombuffer(chunk, dtype=np.int16)

            # 编码Opus数据
            opus_data = encoder.encode(np_frame.tobytes(), frame_size)
            opus_datas.append(opus_data)

        return opus_datas, duration


async def connect(i, opus_data):
    async with websockets.connect(ws_url, additional_headers={"Authorization": "Bearer test-token", "Device-Id": "test-device"}) as ws:
        await ws.send('{"type":"hello","version": 1,"transport":"websocket","audio_params":{"format":"opus", "sample_rate":16000, "channels":1, "frame_duration":60}}')
        ret = await ws.recv()
        print(i, ret)
        for data in opus_data[0]:
            await ws.send(data, text=False)
        # return
        start = time.time()
        while True:
            ret = await ws.recv()
            if isinstance(ret, bytes):
                # print(i, len(ret))
                pass
            else:
                print(i, time.time() - start, ret)
                rej = json.loads(ret)
                if rej["type"] == "tts" and rej["state"] == "stop":
                    break
                # if rej.get("state") == "sentence_start":
                #     break

async def main():
    opus_data = wav_to_opus_data("tmp/example.wav")
    loop = asyncio.get_event_loop()
    futs = []
    for i in range(0, 10):
        fut = loop.create_task(connect(i, opus_data))
        futs.append(fut)
    for fut in futs:
        await fut

if __name__ == "__main__":
    asyncio.run(main())