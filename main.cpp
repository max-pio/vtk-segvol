#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkXMLImageDataReader.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkVolumeProperty.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolume.h>
#include <vtkWindowToImageFilter.h>
#include <iostream>

#include "stb/stb_image_write.hpp"

void save_image(vtkSmartPointer<vtkRenderWindow> renderWindow)
{
    // Capture the rendered image from the render window
    vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
    windowToImageFilter->SetInput(renderWindow);
    windowToImageFilter->SetInputBufferTypeToRGBA(); // Capture RGBA
    windowToImageFilter->ReadFrontBufferOff(); // Read from back buffer
    windowToImageFilter->Update();

    vtkImageData* imageData = windowToImageFilter->GetOutput();

    int* dims = imageData->GetDimensions();
    int width = dims[0];
    int height = dims[1];
    int numberOfComponents = imageData->GetNumberOfScalarComponents();

    unsigned char* vtkPixels = static_cast<unsigned char*>(imageData->GetScalarPointer());

    // stbi_write_png expects row pointers from top-left, whereas VTK image origin is bottom-left
    // So we need to flip vertically before saving
    std::vector<unsigned char> flippedPixels(width * height * numberOfComponents);
    for (int y = 0; y < height; ++y)
    {
        memcpy(
            &flippedPixels[y * width * numberOfComponents],
            &vtkPixels[(height - y - 1) * width * numberOfComponents],
            width * numberOfComponents);
    }

    // Write PNG file using stb_image_write, with 4 components (RGBA)
    if (stbi_write_png("./out.png", width, height, numberOfComponents, flippedPixels.data(),
                       width * numberOfComponents))
    {
        std::cout << "Saved render to ./out.png\n";
    }
    else
    {
        std::cerr << "Failed to save PNG file.\n";
    }
}

int main(int argc, char* argv[])
{
    constexpr const char* VTI_PATH = "/media/maxpio/data/eval/cells/output_cells-00055.vti";
    constexpr int FRAMES = 1;
    constexpr bool OFFSCREEN = false;

    // SETUP -----------------------------------------------------------------------------------------------------------

    // Step 1: Read the .vti volume file
    vtkSmartPointer<vtkXMLImageDataReader> reader = vtkSmartPointer<vtkXMLImageDataReader>::New();
    reader->SetFileName(VTI_PATH);
    reader->Update();

    // Step 2: Set up color and opacity transfer functions
    vtkSmartPointer<vtkColorTransferFunction> colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
    for (int x = 0; x < 256; x++)
        colorTF->AddHSVPoint(static_cast<double>(x), static_cast<double>(((x << 4) ^ x) % 256) / 255., 1.f, 1.f);

    vtkSmartPointer<vtkPiecewiseFunction> opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();
    opacityTF->AddPoint(0.0, 0.0);
    opacityTF->AddPoint(127., 0.0);
    opacityTF->AddPoint(128., 1.0);
    opacityTF->AddPoint(255.0, 1.0);

    // Step 3: Set up the volume property
    vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
    volumeProperty->SetColor(colorTF);
    volumeProperty->SetScalarOpacity(opacityTF);
    volumeProperty->SetInterpolationTypeToNearest();
    volumeProperty->ShadeOff();

    // Step 4: Set up the volume mapper
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapper->SetInputConnection(reader->GetOutputPort());

    // Step 5: Set up the volume
    vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
    volume->SetMapper(volumeMapper);
    volume->SetProperty(volumeProperty);

    // Step 6: Set up renderer and render window with offscreen rendering
    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->AddVolume(volume);
    renderer->SetBackground(0.1, 0.1, 0.2);

    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(1920, 1080);

    // RENDERING -------------------------------------------------------------------------------------------------------

    if (OFFSCREEN)
    {
        renderWindow->OffScreenRenderingOn();


        // Step 7: Render and measure time (CPU side)
        renderWindow->Render();
        double cpuRenderTime = renderer->GetLastRenderTimeInSeconds();
        for (int i = 0; i < FRAMES; ++i)
        {
            renderWindow->Render();
            cpuRenderTime += renderer->GetLastRenderTimeInSeconds();
        }

        std::cout << "Render time [ms/frame]: " << (cpuRenderTime / FRAMES * 1000.) << std::endl;

        save_image(renderWindow);
    }
    else
    {
        vtkNew<vtkRenderWindowInteractor> interactor;
        interactor->SetRenderWindow(renderWindow);
        interactor->Start();
    }
    return 0;
}
