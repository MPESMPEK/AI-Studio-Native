---
license: creativeml-openrail-m
base_model:
- stable-diffusion-v1-5/stable-diffusion-v1-5
base_model_relation: quantized
---

# stable-diffusion-v1-5-int8-ov

 * Model creator: [SD v1.5](https://huggingface.co/stable-diffusion-v1-5)
 * Original model: [stable-diffusion-v1-5/stable-diffusion-v1-5](https://huggingface.co/stable-diffusion-v1-5/stable-diffusion-v1-5)

## Description

This is [stable-diffusion-v1-5/stable-diffusion-v1-5](https://huggingface.co/stable-diffusion-v1-5/stable-diffusion-v1-5) model converted to the [OpenVINO™ IR](https://docs.openvino.ai/2024/documentation/openvino-ir-format.html) (Intermediate Representation) format with weights compressed to INT8 by [NNCF](https://github.com/openvinotoolkit/nncf).

## Quantization Parameters

Weight compression was performed using `nncf.compress_weights` with the following parameters:


* mode: **INT8_ASYM**
* ratio: **1.0**

For more information on quantization, check the [OpenVINO model optimization guide](https://docs.openvino.ai/2024/openvino-workflow/model-optimization-guide/weight-compression.html).


## Compatibility

The provided OpenVINO™ IR model is compatible with:

* OpenVINO version 2025.0.0 and higher
* Optimum Intel 1.22.0 and higher

## Running Model Inference with [Optimum Intel](https://huggingface.co/docs/optimum/intel/index)

1. Install packages required for using [Optimum Intel](https://huggingface.co/docs/optimum/intel/index) integration with the OpenVINO backend:

```
pip install optimum[openvino] diffusers --extra-index-url https://download.pytorch.org/whl/cpu
```

2. Run model inference:

```
from optimum.intel.openvino import OVDiffusionPipeline

model_id = "OpenVINO/stable-diffusion-v1-5-int8-ov"
pipeline = OVDiffusionPipeline.from_pretrained(model_id, safety_checker=None)

prompt = "sailing ship in storm by Rembrandt"
images = pipeline(prompt, num_inference_steps=20).images
```

## Running Model Inference with [OpenVINO GenAI](https://github.com/openvinotoolkit/openvino.genai)

1. Install packages required for using OpenVINO GenAI.
```
pip install huggingface_hub
pip install -U openvino openvino-tokenizers openvino-genai
```

2. Download model from HuggingFace Hub
   
```
import huggingface_hub as hf_hub

model_id = "OpenVINO/stable-diffusion-v1-5-int8-ov"
model_path = "stable-diffusion-v1-5-int8-ov"

hf_hub.snapshot_download(model_id, local_dir=model_path)

```

3. Run model inference:

```
import openvino_genai as ov_genai
from PIL import Image

device = "CPU"
pipe = ov_genai.Text2ImagePipeline(model_path, device)

prompt = "sailing ship in storm by Rembrandt"
image_tensor = pipe.generate(prompt, num_inference_steps=20)
image = Image.fromarray(image_tensor.data[0])

```

More GenAI usage examples can be found in OpenVINO GenAI library [docs](https://github.com/openvinotoolkit/openvino.genai/blob/master/src/README.md) and [samples](https://github.com/openvinotoolkit/openvino.genai?tab=readme-ov-file#openvino-genai-samples)


## Limitations

Check the original model card for [limitations](https://huggingface.co/stable-diffusion-v1-5/stable-diffusion-v1-5).

## Legal information

The original model is distributed under [creativeml-openrail-m](https://huggingface.co/spaces/CompVis/stable-diffusion-license) license. More details can be found in [stable-diffusion-v1-5/stable-diffusion-v1-5](https://huggingface.co/stable-diffusion-v1-5/stable-diffusion-v1-5).

## Disclaimer

Intel is committed to respecting human rights and avoiding causing or contributing to adverse impacts on human rights. See [Intel’s Global Human Rights Principles](https://www.intel.com/content/dam/www/central-libraries/us/en/documents/policy-human-rights.pdf). Intel’s products and software are intended only to be used in applications that do not cause or contribute to adverse impacts on human rights.