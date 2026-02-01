//  Copyright (C) 2025, Max Piochowiak
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cmath>
#include <string>
#include <stdexcept>

#ifdef LIB_HIGHFIVE
    #include <highfive/H5File.hpp>
#endif

namespace vvv {

template <typename T>
void read_hdf5(const std::string& url, size_t (&dim_xyz)[3], T* output_data = nullptr) {
#ifdef LIB_HIGHFIVE
    HighFive::File file(url, HighFive::File::ReadOnly);
    auto dataset = file.getDataSet(file.getObjectName(0));

    // read dimension
    std::vector<size_t> dimensions = dataset.getDimensions();
    if (dimensions.size() != 3) {
        throw std::runtime_error("hdf5 volume file data set must have exactly 3 dimensions.");
    }
    dim_xyz[0] = dimensions[2];
    dim_xyz[1] = dimensions[1];
    dim_xyz[2] = dimensions[0];
    // empty output_data pointer: only write dimensions
    if  (output_data == nullptr)
        return;

    float max_dim = static_cast<float>(std::max(dimensions.at(0), std::max(dimensions.at(1), dimensions.at(2))));
    float physical_size_x = static_cast<float>(dimensions.at(2)) / max_dim;
    float physical_size_y = static_cast<float>(dimensions.at(1)) / max_dim;
    float physical_size_z = static_cast<float>(dimensions.at(0)) / max_dim;

    if (physical_size_x <= 0.f || physical_size_y <= 0.f || physical_size_z <= 0.f || !std::isfinite(physical_size_x) || !std::isfinite(physical_size_y) || !std::isfinite(physical_size_z)) {
        throw std::invalid_argument("invalid hdf5 physical volume size");
    }

    // write hdf5 file to pre-allocated memory
    dataset.read_raw(output_data);
#else
    throw std::runtime_error("HighFIVE / HDF5 libraries not found! Cannot load .hdf5 volume file!");
#endif
}


} // namespace vvv
