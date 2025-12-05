#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>

#include "Camera.hpp"

class SegmentedVolumeMaterial {

public:
    static const int DISCR_NONE = -2; // disabled material
    static const int DISCR_ANY = -1;
    char name[64] = "";
    int discrAttribute = 0;               // discriminator attribute used to determine which labels belong to the material
    float discrInterval[2] = {0.f, 1.f}; // labels with the discrAttribute within this interval belong to the material
    int tfAttribute = 0;
    void* tf = nullptr;
    float tfMinMax[2] = {0.f, 1.f};
    float opacity = 1.f;
    float emission = 0.f;
    int wrapping = 0; // wrap mode: 0 = clamp, 1 = repeat, 2 = random
};

struct VolcaniteParameters {
    vvv::Camera camera;
    std::vector<SegmentedVolumeMaterial> materials;
    glm::ivec3 axis_order; ///< permutation of 012 (xyz) axes
    glm::bvec3 axis_flip;
};

class VcfgSegVolTFFileReader {

private:

    static bool readParameter(const std::string &parameter_label, std::istream &parameter_stream, std::vector<SegmentedVolumeMaterial>& mats, glm::ivec3& axis_order, glm::bvec3& axis_flip) {
        // check if this element list contains a parameter of the given label_name
        if (parameter_label == "Materials:") {
            size_t matCount;
            parameter_stream >> matCount;
            mats.resize(matCount);

            for (int m = 0; m < matCount; m++) {
                auto& mat = mats[m];

                std::string name;
                parameter_stream >> name;
                if (name == "#")
                    mat.name[0] = '\0';
                else
                    mat.name[0] = name[0];

                parameter_stream >> mat.discrAttribute;
                parameter_stream >> mat.discrInterval[0];
                parameter_stream >> mat.discrInterval[1];
                parameter_stream >> mat.tfAttribute;
                parameter_stream >> mat.tfMinMax[0];
                parameter_stream >> mat.tfMinMax[1];
                parameter_stream >> mat.opacity;
                parameter_stream >> mat.emission;
                parameter_stream >> mat.wrapping;
                //
                size_t colormap_control_points = 0;
                parameter_stream >> colormap_control_points;
                if (colormap_control_points > 65536) {
                    std::cout << "Invalid color map control point count " << colormap_control_points;
                    return false;
                }
                for (int i = 0; i < colormap_control_points; i++) {
                    float tmp;
                    parameter_stream >> tmp;
                    parameter_stream >> tmp;
                    parameter_stream >> tmp;
                }
                int precomputedIdx;
                parameter_stream >> precomputedIdx;
                int type;
                parameter_stream >> type;
                if (type < 0 || type > 3) {
                    std::cout << "Unsupported color map type " << type;
                    return false;
                }
            }

            // parameter was consumed
            std::cout << "successfully imported materials." << std::endl;
            return true;
        } else if (parameter_label == "Axis_Order:") {
            std::string tmp;
            parameter_stream >> tmp;
            axis_order = {~0u,~0u,~0u};
            axis_order[static_cast<int>(tmp.find('X'))] = 0;
            axis_order[static_cast<int>(tmp.find('Y'))] = 1;
            axis_order[static_cast<int>(tmp.find('Z'))] = 2;
            return true;
        } else if (parameter_label == "X_Axis:") {
            int i;
            parameter_stream >> i;
            axis_flip.x = i;
            return true;
        } else if (parameter_label == "Y_Axis:") {
            int i;
            parameter_stream >> i;
            axis_flip.y = i;
            return true;
        } else if (parameter_label == "Z_Axis:") {
            int i;
            parameter_stream >> i;
            axis_flip.z = i;
            return true;
        } else {
            // parameter was not consumed
            return false;
        }
    }


    static bool readParameters(std::istream &in, VolcaniteParameters &params) {
        std::string line;
        while (!in.eof() && std::getline(in, line)) {

            // skip any empty lines
            if (std::ranges::all_of(line, [](const unsigned char &c) { return std::isspace(c); })) {
                continue;
            }
            //        // if this is the next window name which is a single line containing the name between braces as [name],
            //        // return and let the next window continue
            //        if (line.starts_with('[') && line.ends_with(']')) {
            //            next_window_name = line.substr(1, line.size() - 2);
            //            return true;
            //        }

            // read camera parameters
            if (line == "[Camera]") {
                params.camera.readFrom(in, true);
                std::cout << "successfully imported camera." << std::endl;
            }

            // one line contains data for one parameter. a single parameter is read from:
            // [sanitized_parameter_label]: [parameter_values]
            std::istringstream parameter_stream(line);
            std::string parameter_label;
            parameter_stream >> parameter_label;

            // consumed:
            if (readParameter(parameter_label, parameter_stream, params.materials, params.axis_order, params.axis_flip))
                continue;

            if ((!parameter_stream.eof() && parameter_stream.fail()) || (!in.eof() && in.fail())) {
                std::cout << "Error reading parameter " << parameter_label;
                return false;
            }
        }
        return true;
    }

public:

    /// Reads all rendering and camera parameters from the given path.
    /// If parameters could not be imported from path, the previous parameter state is restored.
    /// @param path path of the .vcfg file to import
    /// @param backup_parameters if the current parameters will be backed up to a tmp file and re-imported on failure
    /// @return true if parameters were successfully read from path, false otherwise
    static VolcaniteParameters readParameterFile(const std::string &path) {

        VolcaniteParameters params;

        // Try to load selected config path
        // Load backup config in case of failure
        if (std::ifstream in(path); in.is_open()) {
            // read version strings from file
            std::string tmp;
            in >> tmp; // "Version"
            in >> tmp; // VOLCANITE_VERSION

            if (!readParameters(in, params)) {
                std::cout << "Could not import rendering parameters from " << path << std::endl;
            } else {
                std::cout << "Imported rendering parameters from " << path << std::endl;
            }
            in.close();
        } else {
            std::cout << "Could not open parameter file " << path << std::endl;
        }

        return params;
    }

};
