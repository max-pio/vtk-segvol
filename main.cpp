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
#include <vtkOpenGLGPUVolumeRayCastMapper.h>
#include <vtkVolume.h>

#include <cstdint>
#include <iostream>
#include <functional>
#include <vector>

#include "read_hdf5.hpp"
#include "read_vcfg_tf.hpp"
#include "util.hpp"

// these could be command line arguments:
const std::string CAMERA_PATH = "";

// const std::string VTI_PATH = "/media/maxpio/data/eval/azba/azba.hdf5";
// const std::string VCFG_PATH = "/home/maxpio/code/volcanite/eval/config/azba.vcfg";
//const std::string CAMERA_PATH = "./azba.cam";

const std::string VTI_PATH = "/media/maxpio/data/eval/xtm-battery/xtm-battery.hdf5";
const std::string VCFG_PATH = "/home/maxpio/code/volcanite/eval/config/xtm-battery.vcfg";
//const std::string CAMERA_PATH = "./azba.cam";

// TODO: add other data sets, including Cells
// TODO: make data sets command line arguments, call the binary from a bash / python script

constexpr bool VERBOSE = false;
constexpr int FRAMES = 300;
constexpr bool OFFSCREEN = false;
const std::string CAMERA_EXPORT_PATH = "./camera.cam";


int main(int argc, char* argv[])
{
    // SETUP -----------------------------------------------------------------------------------------------------------

    // disable vsync
    setenv("__GL_SYNC_TO_VBLANK", "0", 1);

    // use GPU ray casting for volume rendering
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper>::New();

    // load Volcanite config file to import which labels are visible:
    // merge all visible material transfer function intervals
    VolcaniteParameters params = VcfgSegVolTFFileReader::readParameterFile(VCFG_PATH);
    std::vector<Interval> intervals;
    for (const auto& m : params.materials) {
        if (m.discrAttribute != SegmentedVolumeMaterial::DISCR_NONE) {
            intervals.emplace_back(m.discrInterval[0], m.discrInterval[1]);
        }
    }
    intervals = mergeIntervals(intervals);
    if (VERBOSE)
    {
        std::cout << "Merged transfer function intervals:" << std::endl;
        for (const auto& i : intervals) {
            std::cout << "  [" << i.start << "," << i.end << "]" << std::endl;
        }
    }


    // load volume from disk, compute min/max volume labels, and assign to VolumeMapper
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
    if (VERBOSE)
        std::cout << "volume labels: [" << label_min << "," << label_max << "]" << std::endl;


    // Set up vtk color and opacity transfer functions
    vtkSmartPointer<vtkColorTransferFunction> colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
    for (unsigned int x = 0; x < 256; x++)
        colorTF->AddHSVPoint(static_cast<double>(x), static_cast<double>((std::hash<unsigned int>{}(x) >> 2u) % 256u) / 255., 1.f, 1.f);
    // fill the opacity TF from the materials opacity vector
    vtkSmartPointer<vtkPiecewiseFunction> opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();
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

        if (VERBOSE)
            std::cout << "Interval [" << start << "," << end << "] " << std::endl;
    }

    // Set up the volume property
    vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
    volumeProperty->SetColor(colorTF);
    volumeProperty->SetScalarOpacity(opacityTF);
    volumeProperty->SetInterpolationTypeToNearest();    // no interpolation for segmentation volume labels

    // Set up the volume
    vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
    volume->SetMapper(volumeMapper);
    volume->SetProperty(volumeProperty);

    // Set up renderer and render window with offscreen rendering
    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->AddVolume(volume);
    renderer->SetBackground(0.5, 0.5, 0.5);
    // GlobalIllumination apparently has no effect on the vtkGPURayCastMapper
    // volumeMapper->SetGlobalIlluminationReach(1.f);
    volumeProperty->SetShade(true);
    //
    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(1920, 1080);

    auto& vcnt_camera = params.camera;
    auto vtk_camera = renderer->GetActiveCamera();

    // Set up the camera and volume transformations.
    {
        constexpr double world_space_scale = 1.;

        // Calculate size of "raw" volume axes
        double raw_bounds[6];
        volume->GetBounds(raw_bounds);
        int maxDim = 0;
        for (int i = 1; i < 3; i++)
            if ((raw_bounds[i*2 + 1] - raw_bounds[i*2]) > (raw_bounds[maxDim*2 + 1] - raw_bounds[maxDim*2]))
                maxDim = i;
        double maxSize = raw_bounds[maxDim*2 + 1] - raw_bounds[maxDim*2];

        // Compute center of the volume
        double centerX = world_space_scale * (raw_bounds[1] + raw_bounds[0]) / 2.0;
        double centerY = world_space_scale * (raw_bounds[3] + raw_bounds[2]) / 2.0;
        double centerZ = world_space_scale * (raw_bounds[5] + raw_bounds[4]) / 2.0;

        // Create volume transformations to center the volume around the Volcanite camera lookat / origin.
        vtkSmartPointer<vtkTransform> volumeTransform = vtkSmartPointer<vtkTransform>::New();
        vtkSmartPointer<vtkMatrix4x4> axisMat = vtkSmartPointer<vtkMatrix4x4>::New();
        for (int a = 0; a < 3; a++) {
            axisMat->SetElement(0, a, 0.);
            axisMat->SetElement(1, a, 0.);
            axisMat->SetElement(2, a, 0.);
            axisMat->SetElement(params.axis_order[a], a, params.axis_flip[a] ? -1. : 1.);
        }
        // Using no scaling: in Volcanite, volumes are scaled so that larges axis has length 1 in world space.
        // The vtkGPUVolumeRayCaster cannot handle volumes with such a small world space size, producing empty images.
        // In VTK, we therefore use the default size (1 voxel = world space length 1) and scale camera distances.
        //double scaleFactor = world_space_scale / maxSize;
        //volumeTransform->Scale(scaleFactor, scaleFactor, scaleFactor);
        volumeTransform->Translate(-centerX, -centerY, -centerZ);
        volumeTransform->Concatenate(axisMat);
        //volumeTransform->Translate(-vcnt_camera.position_look_at_world_space.x * maxSize, -vcnt_camera.position_look_at_world_space.y * maxSize, -vcnt_camera.position_look_at_world_space.z * maxSize);
        volume->SetUserTransform(volumeTransform);

        // Set up camera
        {
            // TODO: clean up the camera configurations here

            //vtk_camera->SetPosition(camera.get_position().x, camera.get_position().y, camera.get_position().z);
            vtk_camera->SetPosition(vcnt_camera.get_position().x * maxSize, vcnt_camera.get_position().y * maxSize, vcnt_camera.get_position().z * maxSize);
            double cam_up[3] = {vcnt_camera.get_up_vector().x, vcnt_camera.get_up_vector().y, vcnt_camera.get_up_vector().z};
            vtk_camera->SetViewUp(cam_up);
            //vtk_camera->SetFocalPoint(0, 0, 0);
            vtk_camera->SetFocalPoint(vcnt_camera.position_look_at_world_space.x * maxSize, vcnt_camera.position_look_at_world_space.y * maxSize, vcnt_camera.position_look_at_world_space.z * maxSize);

            // use Volcanite projection matrix
             vtkSmartPointer<vtkMatrix4x4> projMat = vtkSmartPointer<vtkMatrix4x4>::New();
             for (int x = 0; x < 4; x++)
                 for (int y = 0; y < 4; y++)
                     projMat->SetElement(x, y, params.camera.get_view_to_projection_space(1920./1080.)[y][x]);
            projMat->SetElement(1, 1, projMat->GetElement(1, 1) * -1.);
            vtk_camera->SetExplicitProjectionTransformMatrix(projMat);
            vtk_camera->SetUseExplicitProjectionTransformMatrix(true);
            vtk_camera->SetViewAngle(60);

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
            vtk_camera->SetViewAngle(vcnt_camera.vertical_fov / (2.f * M_PI) * 360.f);
            //vtk_camera->SetClippingRange(camera.near, camera.far);

            if (!CAMERA_PATH.empty()) {
                // load previously exported camera (if applicable)
                LoadCameraFromFile(vtk_camera, CAMERA_PATH);
            }
        }

        // adapt volume bounds to match Volcanite split planes:
        // note: these are in "raw" volume bound space, without the volume transform (translation) applied
        double clipped_bounds[6];
        clipped_bounds[0] = glm::max(raw_bounds[0], static_cast<double>(params.split_plane_x[0]));
        clipped_bounds[1] = glm::min(raw_bounds[1], static_cast<double>(params.split_plane_x[1]));
        clipped_bounds[2] = glm::max(raw_bounds[2], static_cast<double>(params.split_plane_y[0]));
        clipped_bounds[3] = glm::min(raw_bounds[3], static_cast<double>(params.split_plane_y[1]));
        clipped_bounds[4] = glm::max(raw_bounds[4], static_cast<double>(params.split_plane_z[0]));
        clipped_bounds[5] = glm::min(raw_bounds[5], static_cast<double>(params.split_plane_z[1]));
        volumeMapper->SetCropping(true);
        volumeMapper->SetCroppingRegionPlanes(clipped_bounds);
        // volumeMapper->SetSampleDistance(static_cast<float>((bounds[maxDim * 2 + 1] - bounds[maxDim * 2]) / maxSize));
        volumeMapper->SetSampleDistance(0.5);
        volumeMapper->Update();
        // update camera clipping ranges
        renderer->ResetCameraClippingRange();

        // Create cube axes (not when evaluating)
        if (!OFFSCREEN)
        {
            // get volume bounds after all transformations
            // note: clipping of the volume mapper is not considered here
            double bounds[6];
            volume->GetBounds(bounds);
            vtkNew<vtkCubeAxesActor> cubeAxes;
            cubeAxes->SetBounds(bounds);
            cubeAxes->SetCamera(renderer->GetActiveCamera());
            cubeAxes->DrawXGridlinesOn();
            cubeAxes->DrawYGridlinesOn();
            cubeAxes->DrawZGridlinesOn();
            // cubeAxes->GetXAxesLinesProperty()->SetColor(0.0, 0.0, 0.0);
            // cubeAxes->GetYAxesLinesProperty()->SetColor(0.0, 0.0, 0.0);
            // cubeAxes->GetZAxesLinesProperty()->SetColor(0.0, 0.0, 0.0);
            cubeAxes->SetGridLineLocation(1);       // 0 = edges, 1 = faces
            renderer->AddActor(cubeAxes);
        }

        // TODO: remove debugging of vtk projection matrix here
        // double clippingRange[2];
        // vtk_camera->GetClippingRange(clippingRange);
        // for (int y = 0; y < 4; y++)
        // {
        //     for (int x = 0; x < 4; x++)
        //         std::cout << vtk_camera->GetProjectionTransformMatrix(1920./1080, clippingRange[0], clippingRange[1])->GetElement(x, y) << ",";
        //     std::cout << std::endl;
        // }
        // std::cout << "---- VOLCANITE: ------" << std::endl;
        // for (int y = 0; y < 4; y++)
        // {
        //     for (int x = 0; x < 4; x++)
        //         std::cout << params.camera.get_view_to_projection_space(1920./1080)[y][x] << ",";
        //     std::cout << std::endl;
        // }
    }



    // RENDERING -------------------------------------------------------------------------------------------------------

    if (OFFSCREEN)
    {
        renderWindow->OffScreenRenderingOn();
        renderWindow->MakeCurrent();  // Ensure context is current

        // Render a single frame to trigger volume uploading to the GPU (should not be measured)
        renderWindow->Render();
        renderer->GetLastRenderTimeInSeconds();
        // Render and measure frame times (CPU side)
        std::array<double, FRAMES> cpuRenderTimes{};
        for (int i = 0; i < FRAMES; ++i)
        {
            renderWindow->Render();
            cpuRenderTimes[i] = renderer->GetLastRenderTimeInSeconds();
        }


        // compute timing aggregates, print timings
        std::cout << "Render time [ms/frame]: " << std::endl;
        std::cout << "  fst: ";
        double min = 99999999999.f;
        double max = 0.f;
        double avg = 0.f;
        double var = 0.f;
        //
        for (int i = 0; i < FRAMES; ++i)
        {
            // convert to [ms]
            cpuRenderTimes.at(i) *= 1000.;

            if (i < 16)
                std::cout << cpuRenderTimes.at(i) << " ";

            // tracking variables
            if (cpuRenderTimes.at(i) < min)
                min = cpuRenderTimes.at(i);
            if (cpuRenderTimes.at(i) > max)
                max = cpuRenderTimes.at(i);
            avg += cpuRenderTimes.at(i);
            var += cpuRenderTimes.at(i) * cpuRenderTimes.at(i);
        }
        avg /= FRAMES;
        var = var/FRAMES - (avg * avg);
        //
        std::cout << std::endl;
        std::cout << "  min: " << min << std::endl;
        std::cout << "  avg: " << avg << std::endl;
        std::cout << "  sdv: " << std::sqrt(var) << std::endl;
        std::cout << "  max: " << max << std::endl;

        save_image(renderWindow);
    }
    else
    {
        if (VERBOSE)
        {
            std::cout << "Initial camera:" << std::endl;
            printCameraInfo(renderer->GetActiveCamera());
        }
        vtkNew<vtkRenderWindowInteractor> interactor;
        interactor->SetRenderWindow(renderWindow);
        interactor->Start();

        SaveCameraToFile(vtk_camera, CAMERA_EXPORT_PATH);
        if (VERBOSE)
        {
            std::cout << "Shutdown camear:" << std::endl;
            printCameraInfo(renderer->GetActiveCamera());
        }
    }
    return 0;
}
