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
#include <vector>

#include "read_hdf5.hpp"
#include "read_vcfg_tf.hpp"
#include "util.hpp"

#include "args.hpp"

int main(int argc, char* argv[])
{

    // PARSE ARGUMENTS
    const Config config = parseConfig(argc, argv);

    // TODO: add option to loop over all data sets?
    DataSet dataSet = config.data_set;
    if (!std::filesystem::exists(getDataInputPath(config, dataSet)))
    {
        std::cerr << "Could not find segmentation volume file " << getDataInputPath(config, dataSet) << std::endl;
        std::cerr << "Did you set the data set base directory as --data-dir <directory> ?" << std::endl;
        return 1;
    }
    if (!std::filesystem::exists(getVcfgPath(config, dataSet)))
    {
        std::cerr << "Could not find VCFG configuration file " << getVcfgPath(config, dataSet) << std::endl;
        std::cerr << "Did you set the .vcfg base directory as --vcfg-dir <directory> ?" << std::endl;
        return 1;
    }


    std::cout << "Rendering segmentation volume '" << getDataOutputName(config.data_set) << "'" << std::endl;

    // SETUP -----------------------------------------------------------------------------------------------------------

    // disable vsync for VTK
    setenv("__GL_SYNC_TO_VBLANK", "0", 1);

    // load Volcanite configuration file (.vcfg) for importing translatebale parameters
    // note: this is all hardcoded for version 0.6.0
    VolcaniteParameters params = VcfgSegVolTFFileReader::readParameterFile(getVcfgPath(config, dataSet));;

    // RENDERING OBJECTS
    // use GPU ray casting for volume rendering
    const vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper>::New();
    const vtkSmartPointer<vtkColorTransferFunction> colorTF = vtkSmartPointer<vtkColorTransferFunction>::New();
    const vtkSmartPointer<vtkPiecewiseFunction> opacityTF = vtkSmartPointer<vtkPiecewiseFunction>::New();

    // VOLUME IMPORT
    uint32_t label_min = UINT32_MAX, label_max = 0u;
    {
        const std::filesystem::path volume_file = getDataInputPath(config, dataSet);

        // load volume from disk, compute min/max volume labels, and assign to VolumeMapper
        if (volume_file.extension() == ".vti") {
            const vtkSmartPointer<vtkXMLImageDataReader> reader = vtkSmartPointer<vtkXMLImageDataReader>::New();
            reader->SetFileName(volume_file.c_str());
            reader->Update();

            volumeMapper->SetInputConnection(reader->GetOutputPort());

            double range[2];
            reader->GetOutput()->GetScalarRange(range);
            label_min = static_cast<uint32_t>(range[0]);
            label_max = static_cast<uint32_t>(range[1]);
        } else if (volume_file.extension() == ".hdf5" || volume_file.extension() == ".h5") {
            size_t dimensions[3];

            // obtain volume dimensions from file, allocate memory
            const vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
            vvv::read_hdf5<uint32_t>(volume_file, dimensions);
            image->SetDimensions(static_cast<int>(dimensions[0]),
                                 static_cast<int>(dimensions[1]),
                                 static_cast<int>(dimensions[2]));
            image->AllocateScalars(VTK_UNSIGNED_INT, 1);
            image->SetSpacing(params.axis_scale[0], params.axis_scale[1], params.axis_scale[2]);
            std::cout << params.axis_scale[0] << "," << params.axis_scale[1] << "," << params.axis_scale[2] << std::endl;
            vvv::read_hdf5<uint32_t>(volume_file, dimensions, static_cast<uint32_t*>(image->GetScalarPointer()));
            volumeMapper->SetInputData(image);
            volumeMapper->Update();

            double range[2];
            image->GetScalarRange(range);
            label_min = static_cast<uint32_t>(range[0]);
            label_max = static_cast<uint32_t>(range[1]);
        }
        std::cout << "Imported segmentation volume from file " << volume_file << std::endl;
        if (config.verbose)
        {
            std::cout << "  labels: [" << label_min << "," << label_max << "]" << std::endl;
        }
    }

    // TRANSFER FUNCTION CREATION
    {
        // merge volcanite label intervals from visible materials
        std::vector<Interval> intervals;
        for (const auto& m : params.materials) {
            if (m.discrAttribute != SegmentedVolumeMaterial::DISCR_NONE) {
                intervals.emplace_back(m.discrInterval[0], m.discrInterval[1]);
            }
        }
        intervals = mergeIntervals(intervals);
        if (config.verbose)
        {
            std::cout << "Merged transfer function intervals:" << std::endl;
            for (const auto& i : intervals) {
                std::cout << "  [" << i.start << "," << i.end << "]" << std::endl;
            }
        }

        // Set up a single VTK color and opacity transfer function from the merged intervals
        const int COLOR_TF_SIZE = glm::min(256u, label_max);
        for (unsigned int x = 0; x < COLOR_TF_SIZE; x++)
            colorTF->AddHSVPoint(static_cast<double>(x) * ((label_max + 1) / static_cast<double>(COLOR_TF_SIZE)),
                                 static_cast<double>(pcg_hash(x) % 512u) / 512.,
                                 0.8f,
                                 1.f);
        // fill the opacity TF from the materials opacity vector
        //constexpr int TF_SIZE = (1 << 16) - 1;
        const uint32_t TF_SIZE = label_max;
        opacityTF->AddPoint(0., 0.0);
        opacityTF->AddPoint(TF_SIZE, 0.0);
        for (const auto& i : intervals) {
            opacityTF->AddPoint(i.start, 0.);
            opacityTF->AddPoint(i.start, VTK_FLOAT_MAX);
            if (i.start == i.end && label_max < 32768u) {
                // fix for single-label materials in data sets where the transfer function can actually sample all labels
                opacityTF->AddPoint(i.end + 0.9, VTK_FLOAT_MAX);
                opacityTF->AddPoint(i.end + 0.9, 0.);
            } else {
                // in data sets with more labels than transfer function entries, assign regions conservatively to retain empty space
                opacityTF->AddPoint(i.end, VTK_FLOAT_MAX);
                opacityTF->AddPoint(i.end, 0.);
            }
        }
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

    // Set up renderer configuration:
    // - white background
    // - local shading
    // - step size approx. half a voxel
    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->AddVolume(volume);
    renderer->SetBackground(1., 1., 1.);
    // GlobalIllumination apparently has no effect on the vtkGPURayCastMapper
    // volumeMapper->SetGlobalIlluminationReach(maxSize);
    volumeProperty->SetShade(true);
    volumeProperty->SetAmbient(0.3);
    // Create rendering window
    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(config.render_width, config.render_height);

    // CAMERA AND VOLUME TRANSFORMATIONS
    {
        auto& vcnt_camera = params.camera;
        auto vtk_camera = renderer->GetActiveCamera();

        // Calculate size of "raw" volume axes
        double raw_bounds[6];
        volume->GetBounds(raw_bounds);
        int maxDim = 0;
        for (int i = 0; i < 3; i++) {
            if ((raw_bounds[i*2 + 1] - raw_bounds[i*2]) > (raw_bounds[maxDim*2 + 1] - raw_bounds[maxDim*2]))
                maxDim = i;
        }
        double maxSize = raw_bounds[maxDim*2 + 1] - raw_bounds[maxDim*2];

        // Compute center of the volume
        double centerX = (raw_bounds[1] + raw_bounds[0]) / 2.0;
        double centerY = (raw_bounds[3] + raw_bounds[2]) / 2.0;
        double centerZ = (raw_bounds[5] + raw_bounds[4]) / 2.0;

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
        // double scaleFactor = 1.f / maxSize;
        // volumeTransform->Scale(scaleFactor, scaleFactor, scaleFactor);
        volumeTransform->Concatenate(axisMat);
        volumeTransform->Translate(-centerX, -centerY, -centerZ);
        //volumeTransform->Scale(1, 1, 1);
        volume->SetUserTransform(volumeTransform);

        // adapt volume bounds to match Volcanite split planes:
        // note: these are in "raw" volume bound space, without the volume transform (translation) applied
        double clipped_bounds[6];
        clipped_bounds[0] = glm::max(raw_bounds[0], static_cast<double>(params.split_plane_x[0]) * params.axis_scale[0]);
        clipped_bounds[1] = glm::min(raw_bounds[1], static_cast<double>(params.split_plane_x[1]) * params.axis_scale[0]);
        clipped_bounds[2] = glm::max(raw_bounds[2], static_cast<double>(params.split_plane_y[0]) * params.axis_scale[1]);
        clipped_bounds[3] = glm::min(raw_bounds[3], static_cast<double>(params.split_plane_y[1]) * params.axis_scale[1]);
        clipped_bounds[4] = glm::max(raw_bounds[4], static_cast<double>(params.split_plane_z[0]) * params.axis_scale[2]);
        clipped_bounds[5] = glm::min(raw_bounds[5], static_cast<double>(params.split_plane_z[1]) * params.axis_scale[2]);
        volumeMapper->SetCropping(true);
        volumeMapper->SetCroppingRegionPlanes(clipped_bounds);
        volumeMapper->SetSampleDistance(0.5);

        // Create camera transformations and projections
        {
            // update camera clipping ranges (renderer->.. probably has no effect because of our manual projection matrix later)
            renderer->ResetCameraClippingRange(clipped_bounds);
            renderer->SetClippingRangeExpansion(1000.);
            volumeMapper->Update();
            // Volcanite clipping assumes volume world space size of 1 in its clipping planes.
            // Move the far plane away before computing the projection matrix to not clip the volume back side in VTK.
            vcnt_camera.far = static_cast<float>(3.f * maxSize * vcnt_camera.far);
            vtk_camera->SetPosition(vcnt_camera.get_position().x * maxSize,
                                    vcnt_camera.get_position().y * maxSize,
                                    vcnt_camera.get_position().z * maxSize);
            const double cam_up[3] = {vcnt_camera.get_up_vector().x,
                                      vcnt_camera.get_up_vector().y,
                                      vcnt_camera.get_up_vector().z};
            vtk_camera->SetViewUp(cam_up);
            vtk_camera->SetFocalPoint(vcnt_camera.position_look_at_world_space.x * maxSize,
                                      vcnt_camera.position_look_at_world_space.y * maxSize,
                                      vcnt_camera.position_look_at_world_space.z * maxSize);

            // Copy Volcanite camera projection matrix
            // TODO: near and far planes are different...
            const vtkSmartPointer<vtkMatrix4x4> projMat = vtkSmartPointer<vtkMatrix4x4>::New();
            for (int x = 0; x < 4; x++)
                for (int y = 0; y < 4; y++)
                    projMat->SetElement(x, y, params.camera.get_view_to_projection_space(static_cast<float>(config.render_width)/static_cast<float>(config.render_height))[y][x]);
            projMat->SetElement(1, 1, projMat->GetElement(1, 1) * -1.);
            vtk_camera->SetExplicitProjectionTransformMatrix(projMat);
            vtk_camera->SetUseExplicitProjectionTransformMatrix(true);
            vtk_camera->SetViewAngle(vcnt_camera.vertical_fov / (2.f * M_PI) * 360.f);

            // Load previously exported camera (if requested)
            if (!config.camera_import_file.empty()) {
                std::cout << "Importing camera parameters from " << config.camera_import_file << std::endl;
                importCamera(vtk_camera, config.camera_import_file);
            }
        }

        // Display info (not when evaluating): create cube axes and transfer function overlay image
        if (!config.offscreen)
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
            cubeAxes->SetGridLineLocation(1);       // 0 = edges, 1 = faces
            renderer->AddActor(cubeAxes);
        }
    }



    // RENDERING -------------------------------------------------------------------------------------------------------

    if (config.offscreen)
    {
        renderWindow->OffScreenRenderingOn();
        renderWindow->MakeCurrent();  // Ensure context is current

        // Render a single frame to trigger volume uploading to the GPU (should not be measured)
        renderWindow->Render();
        renderer->GetLastRenderTimeInSeconds();
        // Render and measure frame times (CPU side)
        std::vector<double> cpuRenderTimes(config.render_frames, 0.);
        for (int i = 0; i < config.render_frames; ++i)
        {
            renderWindow->Render();
            cpuRenderTimes[i] = renderer->GetLastRenderTimeInSeconds();
        }

        EvalResult res = {};
        for (int i = 0; i < config.render_frames; ++i)
        {
            // convert to [ms]
            cpuRenderTimes.at(i) *= 1000.;

            // tracking variables
            if (i < sizeof(EvalResult::frame) / sizeof(double))
                res.frame[i] = cpuRenderTimes.at(i);
            if (cpuRenderTimes.at(i) < res.min)
                res.min = cpuRenderTimes.at(i);
            if (cpuRenderTimes.at(i) > res.max)
                res.max = cpuRenderTimes.at(i);
            res.avg += cpuRenderTimes.at(i);
            res.var += cpuRenderTimes.at(i) * cpuRenderTimes.at(i);
        }
        res.avg /= config.render_frames;
        res.var = res.var/config.render_frames - (res.avg * res.avg);
        {
            std::sort(cpuRenderTimes.begin(), cpuRenderTimes.end());
            if (config.render_frames % 2 == 0)
                res.med = (cpuRenderTimes[config.render_frames / 2] + cpuRenderTimes[config.render_frames / 2 + 1]) / 2.;
            else
                res.med = cpuRenderTimes[config.render_frames / 2];
        }

        std::cout << "Rendered " << config.render_frames << " frames. Average render time: " << res.avg << " ms/frame." << std::endl;

        // export results
        exportResults(getDataOutputName(config.data_set), res, config.csv_result_file, config.verbose);
        exportImage(renderWindow, config.image_export_dir / (getDataOutputName(config.data_set) + ".png"));
    }
    else
    {
        if (config.verbose)
        {
            std::cout << "Initial camera:" << std::endl;
            printCameraInfo(renderer->GetActiveCamera());
        }
        vtkNew<vtkRenderWindowInteractor> interactor;
        interactor->SetRenderWindow(renderWindow);
        interactor->Start();

        exportCamera(renderer->GetActiveCamera(), config.camera_export_file);

        if (config.verbose)
        {
            std::cout << "Shutdown camera:" << std::endl;
            printCameraInfo(renderer->GetActiveCamera());
        }

        std::cout << std::endl;
    }
    return 0;
}
