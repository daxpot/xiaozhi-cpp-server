import websockets
import asyncio

ws_url = "ws://127.0.0.1:8000"

async def connect(i):
    async with websockets.connect(ws_url, additional_headers={"Authorization": "Bearer test-token", "Device-Id": "test-device"}) as ws:
        await ws.send("test")
        ret = await ws.recv()
        print(i, ret)

async def main():
    loop = asyncio.get_event_loop()
    futs = []
    for i in range(0, 1):
        fut = loop.create_task(connect(i))
        futs.append(fut)
    for fut in futs:
        await fut

if __name__ == "__main__":
    asyncio.run(main())