# OEMExporter

A tool for generating GGX convoluted octahedron mapped textures from cylindrical hdr images.

## Input/Output Examples

![Preview](T_InputExample.jpg?raw=true "Source Preview")
![Preview](T_OutputExample.jpg?raw=true "Ouput Preview")

## Execution Order
- Load source ktx texture.
- Create a destination texture with user specified count of mip levels.
  - Lower mip levels will be used to store higher levels of convolution.
- Per mip level convolution compute pass.
  - Select mip level & apply level roughness value.
  - Decode uv coordinate from octahedron space to a direction.
  - Peform 200000 samples of monte carlo integration using Hammersley point distribution.
    - Get a GGX Importance sample direction using the distributed point & level roughness value.
    - Encode sampled direction into cylindrical uv coordinates.
    - Gather a sample from source texture & apply Lambert's cosine law.
  - Encode integrated color into rgbm.
- Read processed image back from gpu & save as ktx.
 
## How To Run
To run the program via terminal the following arguments need to be submitted.
- Source texture file name.
- Destination texture file name.
- Destination resolution (clampped to 2048).
- A set of roughness levels.
  - As this will double as the number of mip levels the destination resolution might increase if the supplied one cannot support the desired amount of levels.  

### An Example Command
```
./OEMExporter.exe Source.ktx Destination.ktx 512 "{0.1, 0.2, 0.5, 1.0}"
```

## How To Sample
The following code snippet can be used to sample the generated image.
```
uniform sampler2D _SceneOEM;

// As of now the hdr mutliplier is hardcoded to 8.0. This might be parameterized later.
#define HDRFactor 8.0

vec3 HDRDecode(vec4 hdr) { return vec3(hdr.rgb * hdr.a * HDRFactor); }

vec2 OctaWrap(vec2 v) { return (1.0 - abs(v.yx)) * vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0); }

vec2 OctaEncode(vec3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xz = n.y >= 0.0 ? n.xz : OctaWrap(n.xz);
    n.xz = n.xz * 0.5 + 0.5;
    return n.xz;
}

vec3 SampleEnvironment(vec3 direction)
{
    vec2 uv = OctaEncode(direction);
    
    // Note that if your roughness levels do not scale linearly you need to remap the lod bias value to correspond with your setup.
    return HDRDecode(textureLod(_SceneOEM, uv, roughness * 4));
}
```

## Dependencies
- OpenGL 4.5
- [KTX](https://github.com/KhronosGroup/KTX-Software)
- [Glad](https://glad.dav1d.de/)
- [GLFW](https://www.glfw.org/)

## Asset Sources
- [HDRI Haven](https://hdrihaven.com)

## References
- [GGX Importance Sampling](https://learnopengl.com/PBR/IBL/Specular-IBL)
- [Fetching From Cubes and OctaHedrons](https://gpuopen.com/learn/fetching-from-cubes-and-octahedrons/)
- [Octahedron Environment Maps](https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.681.1522&rep=rep1&type=pdf)
- [Faster Octahedron decoding](https://twitter.com/Stubbesaurus/status/937994790553227264)
- [RGBM Encoding](http://graphicrants.blogspot.com/2009/04/rgbm-color-encoding.html)
- [Hammersley Point Distribution](http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html)
