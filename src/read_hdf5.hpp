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

#include <cmath>
#include <cstdint>
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
    dim_xyz[0] = dimensions[0];
    dim_xyz[1] = dimensions[1];
    dim_xyz[2] = dimensions[2];
    // empty output_data pointer: only write dimensions
    if  (output_data == nullptr)
        return;

    const float max_dim = static_cast<float>(std::max(dimensions.at(0), std::max(dimensions.at(1), dimensions.at(2))));
    const float physical_size_x = static_cast<float>(dimensions.at(0)) / max_dim;
    const float physical_size_y = static_cast<float>(dimensions.at(1)) / max_dim;
    const float physical_size_z = static_cast<float>(dimensions.at(2)) / max_dim;

    if (physical_size_x <= 0.f || physical_size_y <= 0.f || physical_size_z <= 0.f || !std::isfinite(physical_size_x) || !std::isfinite(physical_size_y) || !std::isfinite(physical_size_z)) {
        throw std::invalid_argument("invalid hdf5 physical volume size");
    }

    // write hdf5 file to pre-allocated memory
    dataset.read(output_data);
#else
    throw std::runtime_error("HighFIVE / HDF5 libraries not found! Cannot load .hdf5 volume file!");
#endif
}


//template <typename T>
//void write_hdf5_(const Volume<T> *volume, const std::string &path) {
//#ifdef LIB_HIGHFIVE
//    HighFive::File file(path, HighFive::File::ReadWrite | HighFive::File::Create | HighFive::File::Truncate);
//    const std::string datasetName = "decompressed_volume_data";
//    if (file.exist(datasetName))
//        file.unlink(datasetName);
//
//    auto dim = std::vector<size_t>{volume->dim_x, volume->dim_y, volume->dim_z};
//
//    // rewrite volume data s.t. it is a 3D vector
//    std::vector<std::vector<std::vector<T>>> tmp_volume_data(dim[0], std::vector<std::vector<T>>(dim[1], std::vector<T>(dim[2])));
//
//    // TODO: the copy to vector of vector of vector for the HighFive export is extremely expensive
//    for (size_t z = 0; z < dim[0]; ++z) {
//        for (size_t y = 0; y < dim[1]; ++y) {
//            for (size_t x = 0; x < dim[2]; ++x) {
//                size_t index = z * dim[1] * dim[2] + y * dim[2] + x;
//                tmp_volume_data[z][y][x] = volume->dataConst()[index];
//            }
//        }
//    }
//
//    HighFive::DataSetCreateProps probs;
//    probs.add(HighFive::Chunking({std::min(dim[0], size_t{128}),
//                                  std::min(dim[1], size_t{128}),
//                                  std::min(dim[2], size_t{128})}));
//    probs.add(HighFive::Deflate(9));
//
//    auto dataset = file.createDataSet<T>(datasetName, HighFive::DataSpace(dim), probs);
//    dataset.write(tmp_volume_data);
//#else
//    throw std::runtime_error("HighFIVE / HDF5 libraries not found! Cannot write volume to .hdf5 file!");
//#endif
//}

} // namespace vvv
