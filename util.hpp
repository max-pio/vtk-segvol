#pragma once

#include <vtkCamera.h>
#include <vtkSmartPointer.h>
#include <vtkWindowToImageFilter.h>

#include "stb/stb_image_write.hpp"

#include <cstdint>
#include <iostream>


inline void SaveCameraToFile(vtkCamera* camera, const std::string& filename) {
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

inline void LoadCameraFromFile(vtkCamera* camera, const std::string& filename) {
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

inline void save_image(const vtkSmartPointer<vtkRenderWindow>& renderWindow)
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

inline void printCameraInfo(vtkCamera* camera)
{
    std::cout << "Pos: " << camera->GetPosition()[0] << "," << camera->GetPosition()[1] << "," << camera->GetPosition()[2] << std::endl;
    std::cout << "Up:  " << camera->GetViewUp()[0] << "," << camera->GetViewUp()[1] << "," << camera->GetViewUp()[2] << std::endl;
    std::cout << "Foc: " << camera->GetFocalPoint()[0] << "," << camera->GetFocalPoint()[1] << "," << camera->GetFocalPoint()[2] << std::endl;
    std::cout << "Dst: " << camera->GetDistance() << std::endl;
    std::cout << "Ang: " << camera->GetViewAngle() << std::endl;
}