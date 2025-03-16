import ssl
from aiohttp import web
import json

async def handle(request: web.Request):
    debug_info = {
        "method": request.method,
        "path": request.path,
        "query": dict(request.query),  # 查询参数
        "headers": dict(request.headers),  # 请求头部
    }
    print(request.method, request.path, dict(request.query))
    print(dict(request.headers))

    # 尝试读取请求体（如果是 POST/PUT 等）
    body = None
    if request.can_read_body:
        try:
            body = await request.json()  # 尝试解析为 JSON
        except json.JSONDecodeError:
            body = await request.text()  # 如果不是 JSON，则返回原始文本
        debug_info["body"] = body
    print(body)
    # 格式化调试信息
    response_text = json.dumps(debug_info, indent=2, ensure_ascii=False)
    
    # 返回调试信息
    return web.Response(
        text=response_text,
        content_type="application/json",
        charset="utf-8"
    )

# 创建应用并设置路由
app = web.Application()
app.router.add_route("*", "/{path:.*}", handle)

# 配置 SSL 上下文
ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ssl_context.load_cert_chain(certfile='tmp/server.crt', keyfile='tmp/server.key')

# 运行服务器
if __name__ == '__main__':
    web.run_app(app, host='localhost', port=8002, ssl_context=ssl_context)