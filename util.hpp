#pragma once

#include <vtkCamera.h>
#include <vtkSmartPointer.h>
#include <vtkWindowToImageFilter.h>

#include "stb/stb_image_write.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>


inline void exportCamera(vtkCamera* camera, const std::filesystem::path& filename) {
    std::filesystem::create_directories(filename.parent_path());

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file " << filename << std::endl;
        return;
    }

    file << "Position " << camera->GetPosition()[0] << " "
         << camera->GetPosition()[1] << " "
         << camera->GetPosition()[2] << std::endl;

    file << "FocalPoint " << camera->GetFocalPoint()[0] << " "
         << camera->GetFocalPoint()[1] << " "
         << camera->GetFocalPoint()[2] << std::endl;

    file << "ViewUp " << camera->GetViewUp()[0] << " "
         << camera->GetViewUp()[1] << " "
         << camera->GetViewUp()[2] << std::endl;

    file << "ViewAngle " << camera->GetViewAngle() << std::endl;
    file << "ClippingRange " << camera->GetClippingRange()[0] << " "
         << camera->GetClippingRange()[1] << std::endl;

    file.close();
}

inline void importCamera(vtkCamera* camera, const std::filesystem::path& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file " << filename << std::endl;
        return;
    }

    std::string line;
    double pos[3], focal[3], up[3], clip[2];
    double viewAngle;

    while (std::getline(file, line)) {
        if (line.find("Position") == 0) {
            sscanf(line.c_str(), "Position %lf %lf %lf", &pos[0], &pos[1], &pos[2]);
            camera->SetPosition(pos);
        }
        else if (line.find("FocalPoint") == 0) {
            sscanf(line.c_str(), "FocalPoint %lf %lf %lf", &focal[0], &focal[1], &focal[2]);
            camera->SetFocalPoint(focal);
        }
        else if (line.find("ViewUp") == 0) {
            sscanf(line.c_str(), "ViewUp %lf %lf %lf", &up[0], &up[1], &up[2]);
            camera->SetViewUp(up);
        }
        else if (line.find("ViewAngle") == 0) {
            sscanf(line.c_str(), "ViewAngle %lf", &viewAngle);
            camera->SetViewAngle(viewAngle);
        }
        else if (line.find("ClippingRange") == 0) {
            sscanf(line.c_str(), "ClippingRange %lf %lf", &clip[0], &clip[1]);
            camera->SetClippingRange(clip);
        }
    }

    file.close();
}

inline void exportImage(const vtkSmartPointer<vtkRenderWindow>& renderWindow, const std::filesystem::path& file)
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

    // stbi_write_* expects row pointers from top-left, whereas VTK image origin is bottom-left
    // So we need to flip vertically before saving
    std::vector<unsigned char> flippedPixels(width * height * numberOfComponents);
    for (int y = 0; y < height; ++y)
    {
        memcpy(
            &flippedPixels[y * width * numberOfComponents],
            &vtkPixels[(height - y - 1) * width * numberOfComponents],
            width * numberOfComponents);
    }

    std::filesystem::create_directories(file.parent_path());

    if (file.extension() == ".jpg" || file.extension() == ".jpeg")
    {
        // Write PNG file using stb_image_write, with 4 components (RGBA)
        if (!stbi_write_jpg(absolute(file).c_str(), width, height, numberOfComponents, flippedPixels.data(),
                           width * numberOfComponents))
        {
            std::cerr << "Failed to save JPEG file " << file << std::endl;
            return;
        }
    } else if (file.extension() == ".png")
    {
        // Write PNG file using stb_image_write, with 4 components (RGBA)
        if (!stbi_write_png(absolute(file).c_str(), width, height, numberOfComponents, flippedPixels.data(),
                           width * numberOfComponents))
        {
            std::cerr << "Failed to save PNG file " << file <<  std::endl;
            return;
        }
    }
    else
    {
        std::cerr << "Image file extension not recognized: " << file << std::endl;
        return;
    }

    std::cout << "Saved image to " << file << std::endl;
}

struct Interval {
    uint32_t start;
    uint32_t end;
};

inline std::vector<Interval> mergeIntervals(std::vector<Interval> &intervals) {
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

        if (auto [start, end] = merged.back(); end >= intervals[i].start) {
            // If overlapping, merge by extending the end if needed
            if (end < intervals[i].end) {
                end = intervals[i].end;
            }
        } else {
            // No overlap, add interval to merged
            merged.push_back(intervals[i]);
        }
    }

    return merged;
}

inline void printCameraInfo(vtkCamera* camera)
{
    std::cout << "  Pos: " << camera->GetPosition()[0] << "," << camera->GetPosition()[1] << "," << camera->GetPosition()[2] << std::endl;
    std::cout << "  Up:  " << camera->GetViewUp()[0] << "," << camera->GetViewUp()[1] << "," << camera->GetViewUp()[2] << std::endl;
    std::cout << "  Foc: " << camera->GetFocalPoint()[0] << "," << camera->GetFocalPoint()[1] << "," << camera->GetFocalPoint()[2] << std::endl;
    std::cout << "  Dst: " << camera->GetDistance() << std::endl;
    std::cout << "  Ang: " << camera->GetViewAngle() << std::endl;
}


struct EvalResult
{
    double min = 99999999999.f;
    double max = 0.f;
    double avg = 0.f;
    double var = 0.f;
    double med = 0.f;
    double frame[16] = {0.};
};

inline void exportResults(const std::string& name, const EvalResult &result, const std::filesystem::path& file, bool consoleLog = true)
{
    if (consoleLog)
    {
        std::cout << "Render time [ms/frame]: " << std::endl;
        std::cout << "  frames: ";
        for (const double f : result.frame)
            std::cout << f << ", ";
        std::cout << std::endl;
        std::cout << "  min: " << result.min << std::endl;
        std::cout << "  avg: " << result.avg << std::endl;
        std::cout << "  sdv: " << std::sqrt(result.var) << std::endl;
        std::cout << "  max: " << result.max << std::endl;
    }

    const bool newFile = !std::filesystem::exists(file);
    if (newFile)
        std::filesystem::create_directories(file.parent_path());

    std::ofstream logFile(file, std::ios::out | std::ios::app);
    if (!logFile.is_open())
    {
        std::cerr << "Failed to open log file " << file << std::endl;
        return;
    }

    // if file did not exist: write CSV header
    if (newFile)
    {
        logFile << "Data Set,frame min [ms],frame avg [ms],frame max [ms],stdv,frame med [ms]";
        for (int i = 0; i < sizeof(EvalResult::frame)/sizeof(double); i++)
            logFile << ",frame" << i;
        logFile << ",time" << std::endl;
    }

    // append a single line for this result
    logFile << name << ",";
    logFile << result.min << "," << result.avg << "," << result.max << "," << std::sqrt(result.var) << "," << result.med;
    for (const double f : result.frame)
        logFile << "," << f;
    // get timestamp
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);
    char time_buf[20];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", local);
    logFile << "," << time_buf;
    logFile << std::endl;

    logFile.close();
}

// from PCG Hash from "Hash Functions for GPU Rendering", Mark Jarzynski and Marc Olano
unsigned int pcg_hash(uint v)
{
    unsigned int state = v * 747796405u + 2891336453u;
    unsigned int word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}