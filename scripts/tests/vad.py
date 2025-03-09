import onnx

model = onnx.load("models/silero_vad.onnx")
for input in model.graph.input:
    print("input", input.name, input.type)

    
for output in model.graph.output:
    print("output", output.name, output.type)