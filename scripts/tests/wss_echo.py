import websockets
import asyncio
import ssl
import pathlib

async def handle(conn: websockets.ServerConnection):
    print("handle header")
    print(conn.request.path)
    print(conn.request.headers)
    while True:
        data = await conn.recv()
        print("read", data.encode("utf-8"))
    # ret = await conn.send(data)
    # print("write", ret)

async def main():
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.load_cert_chain("tmp/server.crt", "tmp/server.key")
    async with websockets.serve(
        handle,
        "127.0.0.1",
        "8000",
        ssl=ssl_context
    ):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())