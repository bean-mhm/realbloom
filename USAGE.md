


# Quick Start

This tutorial intends to get your hands on RealBloom as quickly as possible. Below, we'll go through the process of applying convolutional bloom - A.K.A. the physically accurate RTX bloom sauce - on a 3D render. So buckle up, run the [latest release](https://github.com/bean-mhm/realbloom/releases), and let's get started.

## Wait

First off, I highly suggest you to watch [this video](https://www.youtube.com/watch?v=QWqb5Gewbx8) by AngeTheGreat, if you want to have a general idea of what we're gonna do in this tutorial, and how RealBloom works in general.

## Getting Started

RealBloom provides 3 main functionalities, in order to achieve the so-called physically accurate bloom effect.

 1. Simulating the **Diffraction Pattern** of an aperture
 2. Applying **Dispersion** on the pattern
 3. Performing **Convolution** to achieve bloom

If any or all of these terms sound alien to you, fear not, as they'll be explained below.

## Interface

RealBloom provides a GUI (Graphical User Interface) and a CLI (Command Line Interface). We'll start by observing the GUI, and briefly talk about the CLI later. The layout is rather simple, as there's a single main window that provides everything we need.

![RealBloom Screenshot](images/tutorial/0-interface.png)

As of now, there are 6 panels, each for a specific purpose. A panel can be docked or floating, and you can resize it to your liking.


 - **Image List**: A static list of image slots that serve different purposes. You can switch to another slot by clicking on it. We'll go through what each slot is used for later.

 - **Color Management**: We'll use this panel to change how we import, view, and export images, as well as options that'll come handy when simulating dispersion.

 - **Misc**: Miscellaneous information, along with a slider for the UI scale.

 - **Image Viewer**:  Displays the image contained in the current slot.

- **Diffraction**: We'll use this to generate the light diffraction pattern of an aperture, and to apply dispersion on it. Again, don't worry if this seems confusing to you.

- **Convolution**: This is where we can apply the bloom effect. We can do lots of other things using convolution, so it's not necessarily limited to bloom.

## Aperture

An [aperture](https://en.wikipedia.org/wiki/Aperture) defines the shape of the hole through which light passes to reach the camera sensor. Because of light's wave-like properties, this causes a [diffraction](https://en.wikipedia.org/wiki/Diffraction) pattern to form, which affects every image captured by the camera. The diffraction pattern usually looks like a star with a halo, but it generally depends on the shape of the aperture. Diffraction is what makes stars have *the star shape*.

Let's start by loading in an image that represents the geometric shape of our aperture. Click *Browse Aperture* in the top right panel *Diffraction*. There are a bunch of example aperture shapes in `demo/Apertures` ready for you. I will be using `Octagon.png`.

![An octagonal aperture](images/tutorial/1-aperture.png)
 
## Diffraction

Let's see what the diffraction pattern of our aperture looks like. We'll continue by clicking on *Compute* in the *DIFFRACTION* section. This will generate the diffraction pattern using an [FFT algorithm](https://en.wikipedia.org/wiki/Fast_Fourier_transform). Keep in mind that we are referring to the far-field [Fraunhofer diffraction pattern](https://en.wikipedia.org/wiki/Fraunhofer_diffraction) here.

![Diffraction pattern of an octagon](images/tutorial/2-diff.png)

> The *Grayscale* checkbox can be enabled for colored images, in order to make the image black-and-white before feeding it to the FFT algorithm. If disabled, FFT will be performed on all color channels separately.

I've temporarily increased my view exposure in the *Color Management* panel so that we can see the image properly.

Notice how the selected image slot has changed to *Dispersion Input*. We could use any arbitrary image as the dispersion input, but the diffraction pattern will automatically get loaded into this slot.

## Dispersion

Our little star pattern isn't quite ready to be used yet. In the real world, the scale of the pattern depends on the wavelength of the light, making it appear colorful and "rainbow-ey". We can simulate [this phenomenon](https://en.wikipedia.org/wiki/Dispersion_%28optics%29) in the *DISPERSION* section. Here's what each slider does:

| Parameter | Description |
|--|--|
| Exposure | Exposure adjustment |
| Contrast | Contrast adjustment |
| Color | Multiplies the dispersion result with a custom color. |
| Amount | Amount of dispersion. This defines the difference between the largest and the smallest scale. |
| Steps | Number of wavelengths to sample from the visible light spectrum. A value of 32 is only enough for previewing. |
| Method | Dispersion method |
| Threads | Number of threads to use in the CPU method |

After adjusting the sliders to your liking - or copying the values from the screenshot below - hit *Apply Dispersion* and wait for the simulation to end.

![Dispersion result](images/tutorial/3-disp.png)

> In the *Color Management* panel, I've set my view transform to *AgX*, and I'm using the *Punchy* artistic look. More on this in a second. 

Now, hit *Save* in the *Image List* panel, and save the dispersion result as `kernel.exr`.

## Convolution Input

This is the image we want to apply bloom on. We'll need an open-domain image for this, which preserves pixel values outside the 0-1 range. If you're confused, here are some questions and answers to hopefully help you better understand how open-domain (A.K.A. HDR) images work. If you're a nerd in this field, feel free to skip this part.

> Q: **What on earth is an Open-Domain Image?**

> A: Most everyday image formats such as PNG and JPEG can only have RGB values from 0-1, which translates to 0-255 when stored using unsigned 8-bit integers. Hence, they are closed-domain formats. Most closed-domain images have some form of an [OETF](https://en.wikipedia.org/wiki/Transfer_functions_in_imaging), widely known as a "Gamma" function, applied on the pixel values, therefore they may be called non-linear images. On the other hand, an open-domain image accepts any real number for its pixel values, while also providing more depth and precision, as it is typically stored using a 32-bit or 16-bit floating-point number per color channel. It is very common for these images to have linear pixel values. They may also be called "HDR" images in some places.

> Q: **How do you store these images?**

> A: Image formats like [OpenEXR](https://en.wikipedia.org/wiki/OpenEXR) and [TIFF](https://en.wikipedia.org/wiki/TIFF) allow us to store pixel data as floating-point numbers that aren't limited to any range.

> Q: **How do you display them on a monitor?**

> A: This is a huge topic, but I'll try to summarize what you'll need to know. Pixel values higher than 1 usually just get clamped down to 1 before being displayed on your monitor, making the bright parts of the image look overexposed and blown out, and introducing weird hue skews. Some games and programs support proper display/view transforms to nicely convert linear RGB tristimulus into something that can be correctly displayed on your monitor. Some games can produce true HDR output if your monitor supports it, but that's another story. Despite the pixel values being clamped and/or transformed when *displayed*, they are still *stored* as their original floating-point values.

> Q: **Does RealBloom support display transforms?**

> A: Yes. RealBloom supports display/view transforms through [OpenColorIO](https://opencolorio.org/). In the *Color Management* panel, you can switch the view to *AgX* to have a better view of 3D scenes. Note that this may ruin images that have already been transformed and gone through a camera, so this works best on raw linear tristimulus output from your rendering software, or a carefully developed RAW image, typically stored in the OpenEXR format.

> Q: **What is AgX?**

> A: [AgX](https://www.elsksa.me/scientia/cgi-offline-rendering/rendering-transform) is an experimental OCIO config, made by [Troy James Sobotka](https://twitter.com/troy_s), aimed at cinematic and filmic color transforms. Troy is the author of the famous Filmic config for Blender, and a true master of color science. RealBloom uses a custom version of AgX as its default user config, as well as its internal config (we'll discuss that later).

> Q: **Are you an expert?**

> A: Absolutely not. I've only been learning about color science for a couple of months. But, I have asked Troy (indirectly, thanks to Nihal) to review this part, and he's corrected a few things.

In the *Convolution* panel, click on *Browse Input* and select an HDR (open-domain) image. I have included some example images in `demo/Images`. For this tutorial, I'll be using `Leaves.exr`, which is a render I made in Blender for demonstrating convolutional bloom.

> Bloom works best on scenes with extremely bright spots on dark backgrounds. Forcing bloom on low-contrast and flat images may take away from the realism.

You can safely use the *AgX* view for this image, as this is (almost) raw HDR data from a 3D scene. I will reset the look, and increase my viewing exposure slightly.
 
![An HDR image loaded as the convolution input](images/tutorial/4-conv-input.png)

## Convolution Kernel

The kernel is what defines the "shape" of the bloom effect. Meaning, convolution will be performed on the input image using this kernel.

> Q: **What is Convolution?**

> A: Convolution is like a moving weighted average that can be performed on a 1D signal like audio, a 2D image, or any number of dimensions really. It's very powerful, and can be used to achieve lots of cool audio and image effects, as well as many other things in different fields. I highly recommend watching [this video](https://youtu.be/KuXjwB4LzSA) by 3Blue1Brown to get a better understanding of convolution.

Click on *Browse Kernel* and choose the dispersion result that we saved before. This will switch the current slot to *Conv. Kernel*. I'll also reset my view exposure.

> The kernel doesn't necessarily have to be a diffraction pattern. You can use anything as the kernel, so definitely try experimenting with it.

Let's see what each slider in the *KERNEL* section does.
| Parameter | Description |
|--|--|
| Exposure | Exposure adjustment |
| Contrast | Contrast adjustment |
| Color | Multiplies the kernel with a custom color. |
| Rotation | Rotation in degrees |
| Scale | Scale of the kernel |
| Crop | Amount of cropping from the center point |
| Center | The center point of the kernel. Adjusting this will shift the convolution layer and affect how the kernel is cropped. |

![An HDR image loaded as the convolution kernel](images/tutorial/5-kernel-1.png)

Let's adjust the kernel exposure and contrast until we like it.

![Adjusting the kernel exposure and contrast](images/tutorial/5-kernel-2.png)

Since most outer parts of the kernel are black, we can crop it to increase performance. To determine the right amount of cropping, I'll maximize my kernel exposure, and slowly decrease the crop amount until it barely touches the edges of the pattern.

> Ctrl+Click on a slider to type a custom value.

![Cropping the kernel](images/tutorial/5-kernel-3.png)

Let's finalize our kernel by lowering the exposure back.

![Kernel parameters](images/tutorial/5-kernel-4.png)

## Convolution Method

RealBloom provides 3* ways to do convolution.

 - **FFT CPU**: This method uses the [FFT](https://en.wikipedia.org/wiki/Fast_Fourier_transform) and the [convolution theorem](https://en.wikipedia.org/wiki/Convolution_theorem) to perform convolution in a much shorter timespan. So far, this is the fastest implementation available.

 - **Naive CPU**: This method uses the traditional algorithm for convolution, which is inefficient for large inputs. There's not really any point in using this unless for testing.

 - **Naive GPU**: Same as the previous method, but runs on the GPU instead. Usually quite a lot faster than the CPU method.

\*The latest build supports a GPU FFT method, but it's in an experimental state and performs very poorly at the moment.

For this tutorial, we'll go with *FFT CPU*.

### Threads 'n Chunks

In the *Naive CPU* method, you can split the job between multiple threads that run simultaneously. In *Naive GPU*, you can split the input data into several chunks to avoid overloading the GPU.

| Threads | Chunks |
|--|--|
| Each thread processes  a part of the input data *at the same time as all the other threads*.  | Chunks are processed *sequentially*, to reduce GPU load. |
| Threads speed up the process by a noticeable amount. | Chunks may slow down the process by a slight amount, while helping us avoid GPU crashes. |

> *FFT CPU* will automatically decide the optimal number of threads to use.

## Convolution Threshold

The threshold defines the darkest a pixel can go before being ignored by the convolution process. We can increase the threshold to skip pixels that aren't bright enough to contribute to the final result. The *Knee* parameter defines how smooth the transition will be. A higher threshold speeds up the process in naive convolution, but it does not affect the performance in the FFT methods.

In *Conv. Preview*, we can observe what the convolution process will see after threshold is applied.

![Threshold preview](images/tutorial/6-thres.png)

Using a threshold of 0 and mixing the convolution output with the original input generally gives more realistic and appealing results, so we'll set the threshold and knee parameters to 0 for this tutorial, which will leave the input image unchanged.

## Auto-Exposure

This option will adjust the exposure of the kernel in order to match the brightness of the convolution output to that of the input. This is done by making the pixel values in the kernel add up to 1, hence preserving the overall brightness. We'll have this on for this demonstration.
 
## Convolve

After having loaded an input and a kenrel, and set all the parameters, hit *Convolve* to perform convolution. The output will be mixed with the original input image afterwards. We can change the mixing parameters in the *LAYERS* section.

![Convolution result](images/tutorial/7-convmix.png)

Look, we have a sun! Let's hit *Show Conv. Layer* to see how the convolution output looks on its own.

![Convolution layer](images/tutorial/8-convlayer.png)

Generally speaking, the convolution layer should have the same overall brightness as the input. We can use the *Exposure* slider in the *LAYERS* section to adjust the exposure of the convolution layer. In this case, we don't need to do this, since we had *Auto-Exposure* enabled.

> You can enable additive blending using the *Additive* checkbox, although it is usually more accurate and best not to use this blending mode.

You'll notice a *Compare* button has appeared In the *Image List* panel. We can use this to compare *Conv. Input* with *Conv. Result*. Finally, while having selected *Conv. Result* as the current slot, we'll use the *Save* button in *Image List* to export the result into an image file.

**Congrats!** We've just finished the tutorial. Hopefully, you've learned some valuable information and gained knowledge on how to operate RealBloom.

## Color Management

RealBloom uses [OpenColorIO](https://opencolorio.org/) for color management. For those of you interested, I'll quickly explain the Color Management panel and what it provides. For more information, there will be links to some helpful articles about color management below.

### VIEW

Here we can alter how we *view* the image contained in the current slot, by adjusting the exposure, changing the **display** type and the **view** transform, and optionally choosing an artistic **look**. This does not affect the pixel values of the image in any way, it just defines how the image is displayed.

### INFO

This section displays the working color space of the current OCIO config, or more specifically, the color space associated with the `scene_linear` role. If there are any errors in RealBloom's Color Management System (CMS), they will also be shown in this section.

> The user config is stored in the `ocio` folder located in the program directory. You may swap the contents with your own custom OCIO config.

### IMAGE IO

We can change image input/output color management settings here.

 - **Input**: Interpreted color space when importing images in linear formats such as OpenEXR

 - **Output**: Output color space when exporting images in linear formats without view transforms

 - **Non-Linear**: Interpreted color space for importing images in non-linear formats like PNG and JPEG

 - **Auto-Detect**: Try to detect the color space when loading linear images, and discard the *Input* setting if a color space was detected. This is guaranteed to work with images exported by RealBloom, ideally if the same version is used for both exporting and importing the image.

 - **Apply View Transform**: Apply the current view transform when exporting linear images. The view transform is always applied on non-linear images.

### COLOR MATCHING

Color Matching Functions (CMF) help us go from wavelengths to [XYZ tristimulus](https://en.wikipedia.org/wiki/CIE_1931_color_space) when simulating dispersion. RealBloom looks for [CSV](https://en.wikipedia.org/wiki/Comma-separated_values) tables in the `cmf` folder located in the program directory, to recognize and parse the available CMF tables. There are a few CMF tables included by default. You can hit *Preview* to see what will be sampled for dispersion.

### XYZ CONVERSION

Here we can alter how the XYZ values from a CMF table get transformed into RGB tristimulus in the working space. The *User Config* method should be used if a CIE XYZ I-E color space exists in the user config. Otherwise, you can use the *Common Space* method and choose a color space that exists in both your config and RealBloom's internal OCIO config.

With the *Common Space* method, the XYZ values will be converted to the common space in the internal config, then from the common space in the user config to the working space.

## Command Line Interface

RealBloom provides a CLI that can be used from within a terminal, or any other program. This can be useful for doing animations and automations. The commands are self-explanatory, since they deliver the same functionality as the GUI.

You can enter CLI mode by running `realbloom cli` in the program directory. Type `help` to see a list of supported commands. Use `quit` to exit the program.

> If you run RealBloom with an empty command, or anything other than `cli`, it will start the GUI.

### Animations

As mentioned above, a CLI opens the possibility to use RealBloom on animations. You can get started by taking a look at `demo/Scripts/anim_conv.py` which is a basic Python script for performing convolution on a sequence of frames.

## GPU Helper

Some of the modules may use RealBloom's GPU helper program if needed. The GPU helper tries to perform operations on the dedicated GPU, while the main program and its GUI are intended to run on the integrated GPU. This is only relevant for Dual-GPU systems.

You can check your `%TEMP%` directory and look for the most recent text file, the name of which starts with `realbloom_gpu_operation`. This log file will contain the name of the GPU (Renderer) on which the operation was ran. If the GPU helper isn't using the desired GPU, visit **Windows Settings > System > Display > Graphics**  to change the default/preferred GPU for `RealBloomGpuHelper.exe`. This might differ for older versions of Windows.

> There's no official way to choose a specific GPU device using OpenGL on Windows. However, we can "signal" to NVIDIA and AMD drivers that "this program needs the high-performance GPU". RealBloom's main program does not contain this signal, while the GPU helper does. This makes the process easier on most Dual-GPU systems, as the UI continues to render smoothly while the dedicated GPU is busy.

## Community

If you use RealBloom on your artwork, or make an artwork using RealBloom, feel free to publish it on Twitter or Instagram under #realbloom.

☀️ **RealBloom Community Server:** [Discord](https://discord.gg/Xez5yec8Hh)

## Thank You

If you find RealBloom useful, consider introducing it to a friend or anyone who you think would be interested. Don't forget to give the project a star! I appreciate you for your time, have a bloomy day!

## Special Thanks

I'd like to say a huge thank you to [Nihal](https://twitter.com/Mulana3D) and their colleagues for supporting the development of RealBloom, by helping with research, testing dev builds and finding bugs, suggesting new features - including a CLI, the use of OCIO, adding demo kernels, etc. - and trying out RealBloom on their artworks and renders.

## Read More

 - [Convolutional Bloom in Unreal Engine](https://docs.unrealengine.com/5.0/en-US/bloom-in-unreal-engine/#bloom-convolution:~:text=%235-,Bloom%20Convolution,-The%20Bloom%20Convolution)
 - [The Hitchhiker's Guide to Digital Colour](https://hg2dc.com/)
 - [CG Cinematography - Christophe Brejon](https://chrisbrejon.com/cg-cinematography/)
 - [But what is a convolution? - 3Blue1Brown](https://www.youtube.com/watch?v=KuXjwB4LzSA)
 - [But what is the Fourier Transform? A visual introduction. - 3Blue1Brown](https://www.youtube.com/watch?v=spUNpyF58BY)


