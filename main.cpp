#include <vtkCamera.h>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkCubeAxesActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkXMLImageDataReader.h>
#include <vtkHDFReader.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkVolumeProperty.h>
#include <vtkTransform.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolume.h>
#include <vtkWindowToImageFilter.h>

#include <cstdint>
#include <iostream>
#include <functional>
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

void printCameraInfo(vtkCamera* camera)
{
    std::cout << "Pos: " << camera->GetPosition()[0] << "," << camera->GetPosition()[1] << "," << camera->GetPosition()[2] << std::endl;
    std::cout << "Up:  " << camera->GetViewUp()[0] << "," << camera->GetViewUp()[1] << "," << camera->GetViewUp()[2] << std::endl;
    std::cout << "Foc: " << camera->GetFocalPoint()[0] << "," << camera->GetFocalPoint()[1] << "," << camera->GetFocalPoint()[2] << std::endl;
    std::cout << "Dst: " << camera->GetDistance() << std::endl;
    std::cout << "Ang: " << camera->GetViewAngle() << std::endl;
}

int main(int argc, char* argv[])
{
    // these could be command line arguments:
    //const std::string VTI_PATH = "/media/maxpio/data/eval/azba/azba.hdf5";
    const std::string VTI_PATH = "/media/maxpio/data/eval/xtm-battery/xtm-battery.hdf5";
    //const std::string VTI_PATH = "/media/maxpio/data/eval/cells/output_cells-00055.vti";

    //const std::string VCFG_PATH = "/home/maxpio/code/volcanite/eval/config/azba.vcfg";
    const std::string VCFG_PATH = "/home/maxpio/code/volcanite/eval/config/xtm-battery.vcfg";
    constexpr int FRAMES = 300;
    constexpr bool OFFSCREEN = false;

    // SETUP -----------------------------------------------------------------------------------------------------------

    // load config file
    VolcaniteParameters params = VcfgSegVolTFFileReader::readParameterFile(VCFG_PATH);

    // merge all visible intervals
    std::vector<Interval> intervals;
    std::cout << "Segmentation Volume Transfer Function. Visible intervals:" << std::endl;
    for (const auto& m : params.materials) {
        if (m.discrAttribute != SegmentedVolumeMaterial::DISCR_NONE) {
            // std::cout << "[" << m.discrInterval[0] << "," << m.discrInterval[1] << "]" << std::endl;
            intervals.emplace_back(m.discrInterval[0], m.discrInterval[1]);
        }
    }
    intervals = mergeIntervals(intervals);
    // std::cout << "Merged intervals:" << std::endl;
    // for (const auto& i : intervals) {
    //         std::cout << "[" << i.start << "," << i.end << "]" << std::endl;
    // }



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
    for (unsigned int x = 0; x < 256; x++)
        colorTF->AddHSVPoint(static_cast<double>(x), static_cast<double>((std::hash<unsigned int>{}(x) >> 2u) % 256u) / 255., 1.f, 1.f);

    vtkSmartPointer<vtkPiecewiseFunction> opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();

    // fill the opacity TF from the materials opacity vector
    constexpr int TF_SIZE = (1 << 16) - 1;
    opacityTF->AddPoint(0., 0.0);
    opacityTF->AddPoint(TF_SIZE, 0.0);
    for (const auto& i : intervals) {

        double start = static_cast<double>(i.start - label_min) / static_cast<double>(label_max - label_min);
        double end = static_cast<double>(i.end + 1 - label_min) / static_cast<double>(label_max - label_min);

        opacityTF->AddPoint(static_cast<int>(std::round(start * TF_SIZE)), 0.);
        opacityTF->AddPoint(static_cast<int>(std::round(start * TF_SIZE)), VTK_FLOAT_MAX);
        opacityTF->AddPoint(static_cast<int>(std::round(end * TF_SIZE)), VTK_FLOAT_MAX);
        opacityTF->AddPoint(static_cast<int>(std::round(end * TF_SIZE)), 0.);

        std::cout << "[" << start << "," << end << "] ";
    }
    std::cout << std::endl;

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
    renderer->SetBackground(0., 0., 0.);
    //renderer->SetBackground(1., 1., 1.);
    renderer->SetUseShadows(false);

    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(1920, 1080);

    // Step 7: Set up the camera and volume transformations.
    // * Longest axis of volume has length 1 in world space.
    // * Volume is centered around the origin.
    // * Camera uses the parameters from the configuration.
    {
        constexpr double world_space_scale = 1.;
        double bounds[6];
        volume->GetBounds(bounds);

        std::cout << "x: " << bounds[0] << " - " << bounds[1] << ", ";
        std::cout << "y: " << bounds[2] << " - " << bounds[3] << "; ";
        std::cout << "z: " << bounds[4] << " - " << bounds[5] << std::endl;

        // TODO: check if configuration has clipping planes set -> reduce bounds

        // Calculate size of axes
        double sizeX = bounds[1] - bounds[0];
        double sizeY = bounds[3] - bounds[2];
        double sizeZ = bounds[5] - bounds[4];

        double maxSize = std::max({sizeX, sizeY, sizeZ});
        double scaleFactor = world_space_scale / maxSize;

        std::cout << "maxSize: " << maxSize << std::endl;

        // Center of the volume
        double centerX = world_space_scale * (bounds[1] + bounds[0]) / 2.0;
        double centerY = world_space_scale * (bounds[3] + bounds[2]) / 2.0;
        double centerZ = world_space_scale * (bounds[5] + bounds[4]) / 2.0;

        // Create transform
        vtkSmartPointer<vtkTransform> volumeTransform = vtkSmartPointer<vtkTransform>::New();
        // center
        // (no scaling: Volcanite world space, the larges volume axis has length 1.
        //  In VTK, we use the default size - 1 voxel = length 1 - and scale the camera distance to prevent bugs.)
        // transform axes
        vtkSmartPointer<vtkMatrix4x4> axisMat = vtkSmartPointer<vtkMatrix4x4>::New();
        std::cout << "Axes: " << params.axis_order.x * (params.axis_flip.x ? -1. : 1.) << params.axis_order.y * (params.axis_flip.y ? -1. : 1.) << params.axis_order.z * (params.axis_flip.z ? -1. : 1.) << std::endl;
        for (int a = 0; a < 3; a++) {
            axisMat->SetElement(0, a, 0.);
            axisMat->SetElement(1, a, 0.);
            axisMat->SetElement(2, a, 0.);
            // // flip Y
            // if (params.axis_order[a] == 1)
            //     axisMat->SetElement(params.axis_order[a], a, params.axis_flip[a] ? 1. : -1.);
            // else
            axisMat->SetElement(params.axis_order[a], a, params.axis_flip[a] ? -1. : 1.);
        }
        // axisMat->SetElement(0, 0, 1.f);
        // axisMat->SetElement(1, 2, 1.f);
        // axisMat->SetElement(2, 1, 1.f);
        //axisMat->Invert();
        //volumeTransform->Scale(scaleFactor, scaleFactor, scaleFactor);
        volumeTransform->Translate(-centerX, -centerY, -centerZ);
        // volumeTransform->Concatenate(axisMat);

        //transform->Translate(-camera.position_look_at_world_space.x * maxSize, -camera.position_look_at_world_space.z * maxSize, -camera.position_look_at_world_space.y * maxSize);
        volume->SetUserTransform(volumeTransform);

        // Camera
        auto& camera = params.camera;
        auto vtk_camera = renderer->GetActiveCamera();

        vtk_camera->SetPosition(camera.get_position().x * maxSize * camera.orbital_radius, camera.get_position().y * maxSize * camera.orbital_radius, camera.get_position().z * maxSize * camera.orbital_radius);
        double up[3] = {camera.get_up_vector().x, camera.get_up_vector().y, camera.get_up_vector().z};
        vtk_camera->SetViewUp(up);
        //vtk_camera->SetDistance(camera.orbital_radius);
        vtk_camera->SetFocalPoint(0, 0, 0);

        //  vtkSmartPointer<vtkMatrix4x4> projMat = vtkSmartPointer<vtkMatrix4x4>::New();
        //  for (int x = 0; x < 4; x++)
        //      for (int y = 0; y < 4; y++)
        //          projMat->SetElement(x, y, params.camera.get_view_to_projection_space(1920./1080.)[y][x]);
        // vtk_camera->SetExplicitProjectionTransformMatrix(projMat);
        // vtk_camera->SetUseExplicitProjectionTransformMatrix(true);
        vtk_camera->SetViewAngle(60); //

        // vtkSmartPointer<vtkMatrix4x4> viewMat = vtkSmartPointer<vtkMatrix4x4>::New();
        // for (int x = 0; x < 4; x++)
        //     for (int y = 0; y < 4; y++)
        //         viewMat->SetElement(x, y, params.camera.get_world_to_view_space()[y][x]);


        // vtkSmartPointer<vtkTransform> cameraTransform = vtkSmartPointer<vtkTransform>::New();
        // // model to world
        // cameraTransform->Concatenate(axisMat);
        // //cameraTransform->Scale(1./scaleFactor, 1./scaleFactor, 1./scaleFactor);
        // cameraTransform->Translate(-centerX, -centerY, -centerZ);
        // cameraTransform->Inverse();
        // // world to view
        //cameraTransform->Concatenate(viewMat);
        // vtk_camera->ApplyTransform(cameraTransform);
        // //vtk_camera->SetClippingRange(camera.near, camera.far);
        // vtk_camera->SetViewAngle(camera.vertical_fov / (2.f * M_PI) * 360.f);

        volume->GetBounds(bounds);
        std::cout << "x: " << bounds[0] << " - " << bounds[1] << ", ";
        std::cout << "y: " << bounds[2] << " - " << bounds[3] << ", ";
        std::cout << "z: " << bounds[4] << " - " << bounds[5] << std::endl;

        renderer->ResetCameraClippingRange();

        // Create cube axes
        vtkNew<vtkCubeAxesActor> cubeAxes;
        cubeAxes->SetBounds(bounds);
        cubeAxes->SetCamera(renderer->GetActiveCamera());
        cubeAxes->DrawXGridlinesOn();
        cubeAxes->DrawYGridlinesOn();
        cubeAxes->DrawZGridlinesOn();
        cubeAxes->SetGridLineLocation(1); // 0 = edges, 1 = faces

        renderer->AddActor(cubeAxes);
    }




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
        printCameraInfo(renderer->GetActiveCamera());
        vtkNew<vtkRenderWindowInteractor> interactor;
        interactor->SetRenderWindow(renderWindow);
        interactor->Start();
    }

    printCameraInfo(renderer->GetActiveCamera());
    return 0;
}
