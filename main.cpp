#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkXMLImageDataReader.h>
#include <vtkHDFReader.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkVolumeProperty.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolume.h>
#include <vtkWindowToImageFilter.h>

#include <cstdint>
#include <iostream>
#include <vector>

#include "stb/stb_image_write.hpp"

#include "read_hdf5.hpp"
#include "read_vcfg_tf.hpp"

void save_image(const vtkSmartPointer<vtkRenderWindow>& renderWindow)
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

struct Interval {
    uint32_t start;
    uint32_t end;
};

std::vector<Interval> mergeIntervals(std::vector<Interval> &intervals) {
    if (intervals.empty()) {
        return {};
    }

    // Sort intervals by their start value
    std::sort(intervals.begin(), intervals.end(), [](const Interval a, const Interval b) {
        return a.start < b.start;
    });

    std::vector<Interval> merged;

    // Push first interval to merged
    merged.push_back(intervals[0]);

    for (size_t i = 1; i < intervals.size(); ++i) {
        // Reference to last merged interval

        if (Interval last = merged.back(); last.end >= intervals[i].start) {
            // If overlapping, merge by extending the end if needed
            if (last.end < intervals[i].end) {
                last.end = intervals[i].end;
            }
        } else {
            // No overlap, add interval to merged
            merged.push_back(intervals[i]);
        }
    }

    return merged;
}

int main(int argc, char* argv[])
{
    // these could be command line arguments:
    const std::string VTI_PATH = "/media/maxpio/data/eval/azba/azba.hdf5";
    //const std::string VTI_PATH = "/media/maxpio/data/eval/cells/output_cells-00055.vti";
    const std::string VCFG_PATH = "/home/maxpio/code/volcanite/eval/config/azba.vcfg";
    constexpr int FRAMES = 300;
    constexpr bool OFFSCREEN = true;

    // SETUP -----------------------------------------------------------------------------------------------------------

    // load config file
    std::vector<SegmentedVolumeMaterial> mats = VcfgSegVolTFFileReader::readParameterFile(VCFG_PATH);

    // merge all visible intervals
    std::vector<Interval> intervals;
    std::cout << "Segmentation Volume Transfer Function. Visible intervals:" << std::endl;
    for (const auto& m : mats) {
        if (m.discrAttribute != SegmentedVolumeMaterial::DISCR_NONE) {
            std::cout << "[" << m.discrInterval[0] << "," << m.discrInterval[1] << "]" << std::endl;

            intervals.emplace_back(m.discrInterval[0], m.discrInterval[1]);
        }
    }
    intervals = mergeIntervals(intervals);
    std::cout << "Merged intervals:" << std::endl;
    for (const auto& i : intervals) {
        std::cout << "[" << i.start << "," << i.end << "]" << std::endl;
    }



    // load volume from disk an provide as VolumeMapper
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();

    // min/max volume labels
    uint32_t label_min = UINT32_MAX, label_max = 0u;

    if (VTI_PATH.ends_with(".vti")) {
        vtkSmartPointer<vtkXMLImageDataReader> reader = vtkSmartPointer<vtkXMLImageDataReader>::New();
        reader->SetFileName(VTI_PATH.c_str());
        reader->Update();

        volumeMapper->SetInputConnection(reader->GetOutputPort());

        double range[2];
        reader->GetOutput()->GetScalarRange(range);
        label_min = static_cast<uint32_t>(range[0]);
        label_max = static_cast<uint32_t>(range[1]);
    } else if (VTI_PATH.ends_with(".hdf5") || VTI_PATH.ends_with(".h5")) {
        size_t dimensions[3];

        // obtain volume dimensions from file, allocate memory
        const vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
        vvv::read_hdf5<uint32_t>(VTI_PATH, dimensions);
        image->SetDimensions(static_cast<int>(dimensions[0]),
                             static_cast<int>(dimensions[1]),
                             static_cast<int>(dimensions[2]));
        image->AllocateScalars(VTK_UNSIGNED_INT, 1);
        vvv::read_hdf5<uint32_t>(VTI_PATH, dimensions, static_cast<uint32_t*>(image->GetScalarPointer()));
        volumeMapper->SetInputData(image);

        double range[2];
        image->GetScalarRange(range);
        label_min = static_cast<uint32_t>(range[0]);
        label_max = static_cast<uint32_t>(range[1]);
    }

    std::cout << "labels: [" << label_min << "," << label_max << "]" << std::endl;


    // ------------------------

    // Step 2: Set up color and opacity transfer functions
    vtkSmartPointer<vtkColorTransferFunction> colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
    for (int x = 0; x < 256; x++)
        colorTF->AddHSVPoint(static_cast<double>(x), static_cast<double>(((x << 4) ^ x) % 256) / 255., 1.f, 1.f);

    vtkSmartPointer<vtkPiecewiseFunction> opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();

    // TODO: fill the opacity TF from the mats vector
    opacityTF->AddPoint(0., 0.0);
    opacityTF->AddPoint(255., 0.0);
    for (const auto& i : intervals) {

        double start = static_cast<double>(i.start - label_min) / static_cast<double>(label_max - label_min) * 255.0;
        double end = static_cast<double>(i.end - label_min) / static_cast<double>(label_max - label_min) * 255.0;

        const double EPS = 0.5 / static_cast<double>(label_max - label_min);
        opacityTF->AddPoint(start - EPS, 0.);
        opacityTF->AddPoint(start + EPS, VTK_FLOAT_MAX);
        opacityTF->AddPoint(end - EPS, VTK_FLOAT_MAX);
        opacityTF->AddPoint(end + EPS, 0.);

        std::cout << ">> [" << start << "," << end << "]" << std::endl;
    }

    // Step 3: Set up the volume property
    vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
    volumeProperty->SetColor(colorTF);
    volumeProperty->SetScalarOpacity(opacityTF);
    volumeProperty->SetInterpolationTypeToNearest();
    volumeProperty->ShadeOn();

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

        std::cout << "Render time [ms/frame]: " << (cpuRenderTime * 1000. / FRAMES) << std::endl;

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
