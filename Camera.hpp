//  Copyright (C) 2024, Max Piochowiak and Reiner Dolp, Karlsruhe Institute of Technology
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
//
// This class contains code from the Camera implementation by Christoph Peters "MyToyRenderer" which was released under
// the GPLv3 License. Our adaptions include an added switch between orbital and translational camera modes, file
// import / export, obtaining default parameters, and registering callback functions.
// The original code can be found at https://github.com/MomentsInGraphics/vulkan_renderer/blob/main/src/camera.h

#pragma once

#include <numbers>
#include <glm/gtx/transform.hpp>

#include <functional>
#include <iostream>
#include <string>

#include <glm/glm.hpp>

#ifdef _WIN32
#undef near
#undef far
#endif

namespace vk {
    struct Extent2D {
        uint32_t width;
        uint32_t height;
    };
}

namespace vvv {

/// Holds state for a first person camera that characterizes the world to
/// projection space transform completely, except for the aspect ratio. It also
/// provides enough information to update the camera interactively. It does not
/// store any transforms or other redundant information. Such information has
/// to be computed as needed.  Our world and camera setup uses a right-handed
/// coordinate system (y is up, x to the right, z pointing out of the plane spanned by xy).
class Camera {
  public:
    /// If true, this is an orbital (rotate with mouse + scrollwheel) camera instead of the first person controls
    bool orbital;
    /// The distance of the camera to (0,0,0) if in orbital mode
    float orbital_radius;
    /// The position of the camera in world space
    glm::vec3 position_world_space;
    /// The position of the look at point in world space
    glm::vec3 position_look_at_world_space;
    /// The rotation of the camera around the global y-axis in radians
    float rotation_y;
    /// The rotation of the camera around the local x-axis in radians. Without
    /// rotation the camera looks into the negative z-direction.
    float rotation_x;
    /// The vertical field of view (top to bottom) in radians
    float vertical_fov;
    /// The distance of the near plane and the far plane to the camera position
    float near, far;
    /// The default speed of this camera in meters per second when it moves
    /// along a single axis
    float speed;
    /// 1 iff mouse movements are currently used to rotate the camera
    bool rotate_camera;
    /// The rotation that the camera would have if the mouse cursor were moved
    /// to coordinate (0, 0) with rotate_camera enabled
    float rotation_x_0, rotation_y_0;
    /// The projection mode of the camera
    enum class Mode { Perspective,
                      Orthogonal };
    Mode camera_mode;
    float orthogonal_scale;

    explicit Camera(bool is_orbital = true) : orbital(is_orbital), rotation_x(0), rotation_y(0),
                                              orbital_radius(1.5f), rotation_x_0(0), rotation_y_0(0),
                                              near(0.05f), far(1.0e3f),
                                              vertical_fov(0.33f * std::numbers::pi), speed(2.0f),
                                              position_world_space(0, 0, 5),
                                              position_look_at_world_space(0, 0, 0),
                                              rotate_camera(false), camera_mode(Mode::Perspective), orthogonal_scale(5.f) {
        reset();
    }

    glm::vec3 get_position()
    {
        position_world_space = position_look_at_world_space + glm::vec3(
                                                                  orbital_radius * cos(rotation_y) * cos(rotation_x),
                                                                  orbital_radius * sin(rotation_x),
                                                                  orbital_radius * sin(rotation_y) * cos(rotation_x));
        return position_world_space;
    }

    glm::vec3 get_up_vector() const
    {
        const glm::vec3 up = glm::normalize(glm::vec3(position_world_space.z - position_look_at_world_space.z,
                                                0.f,
                                                position_look_at_world_space.x - position_world_space.x)); // project on xz plane, orthogonal
        return glm::cross(glm::normalize(position_world_space - position_look_at_world_space), up);
    }

    /// Constructs the world to view space transform for the given camera
    glm::mat4 get_world_to_view_space() const
    {
        if (orbital) {
            glm::vec3 up = glm::normalize(glm::vec3(position_world_space.z - position_look_at_world_space.z,
                                                    0.f,
                                                    position_look_at_world_space.x - position_world_space.x)); // project on xz plane, orthogonal
            return glm::lookAt(position_world_space,
                               position_look_at_world_space,
                               glm::cross(glm::normalize(position_world_space - position_look_at_world_space), up));
        } else {
            glm::mat4 translate = glm::translate(-position_world_space);
            glm::mat4 rotY = glm::rotate(rotation_y, glm::vec3(0, 1, 0));
            glm::mat4 rotX = glm::rotate(rotation_x, glm::vec3(1, 0, 0));
            return rotX * rotY * translate;
        }
    }

    /// Constructs the view to projection space transform for the given camera and
    /// the given width / height ratio
    glm::mat4 get_view_to_projection_space(float aspect_ratio) const {
        glm::mat4 proj;
        if (camera_mode == Mode::Perspective) {
            proj = glm::perspective(vertical_fov, aspect_ratio, this->near, this->far);
            ;
        } else if (camera_mode == Mode::Orthogonal) {
            float half_width = 0.5f * orthogonal_scale;
            float half_height = 0.5f * orthogonal_scale / aspect_ratio;
            proj = glm::ortho(-half_width, half_width, -half_height, half_height);
        } else
            throw std::runtime_error("Unknown camera_mode encountered in Camera::get_view_to_projection_space()");

        // hacky fix for Vulkan's inverted y-axis
        proj[1][1] *= -1;

        return proj;
    }

    glm::mat4 get_view_to_projection_space(const vk::Extent2D extent) const {
        return get_view_to_projection_space(getAspectRatio(extent));
    }

    /// Constructs the world to projection space transform for the given camera and
    /// the given width / height ratio
    glm::mat4 get_world_to_projection_space(float aspect_ratio) const;
    glm::mat4 get_world_to_projection_space(const vk::Extent2D extent) const {
        return get_world_to_projection_space(getAspectRatio(extent));
    }

    void reset() {
        if (orbital) {
            rotation_x = 0.5f;
            rotation_y = 4.0f;
            rotation_x_0 = 0.f;
            rotation_y_0 = 0.f;
            orbital_radius = 1.5f;
            speed = 2.0f;
            position_world_space = position_look_at_world_space + glm::vec3(
                                                                      orbital_radius * cos(rotation_y) * cos(rotation_x),
                                                                      orbital_radius * sin(rotation_x),
                                                                      orbital_radius * sin(rotation_y) * cos(rotation_x));
            position_look_at_world_space = glm::vec3(0.f);
            rotate_camera = false;
            camera_mode = Mode::Perspective;
            orthogonal_scale = 5.0f;
        } else {
            rotation_x = 0.6f;
            rotation_y = 2.25;
            rotation_x_0 = 0.f;
            rotation_y_0 = 0.f;
            orbital_radius = 1.f;
            speed = 2.0f;
            position_world_space = {-0.8, 0.6666, -0.8};
            position_look_at_world_space = glm::vec3(0.f);
            rotate_camera = false;
            camera_mode = Mode::Perspective;
            orthogonal_scale = 5.0f;
        }
    }

    static inline float getAspectRatio(const vk::Extent2D extent) {
        return ((float)extent.width) / ((float)extent.height);
    }

    /// Register a function that is called whenever the camera is moved or rotated.
    /// Overrides any previously defined callback function.
    /// There is no callback function defined initially.
    /// @param callbackFunction function that is called on camera updates, may be nullptr to remove the current callback function
    void registerCameraUpdateCallback(std::function<void()> callbackFunction);

    void onCameraUpdate();

    void writeTo(std::ostream &out, bool human_readable = false) {
        if (human_readable) {
            // TODO: write out look at, rotation and radius in orbital mode
            out << "orbital: " << (orbital ? 1 : 0) << std::endl;
            out << "position: " << position_world_space.x << " " << position_world_space.y << " " << position_world_space.z << std::endl;
            out << "lookat: " << position_look_at_world_space.x << " " << position_look_at_world_space.y << " " << position_look_at_world_space.z << std::endl;
            out << "rotation: " << rotation_x << " " << rotation_y << " " << orbital_radius << std::endl;
        } else {
            out.write(reinterpret_cast<char *>(&orbital), sizeof(orbital));
            out.write(reinterpret_cast<char *>(&position_world_space), sizeof(position_world_space));
            out.write(reinterpret_cast<char *>(&position_look_at_world_space), sizeof(position_look_at_world_space));
            out.write(reinterpret_cast<char *>(&rotation_x), sizeof(rotation_x));
            out.write(reinterpret_cast<char *>(&rotation_y), sizeof(rotation_y));
            out.write(reinterpret_cast<char *>(&orbital_radius), sizeof(orbital_radius));
        }

        // if(human_readable) {
        //     out << "orbital: " << (orbital ? 1 : 0) << std::endl;
        //     out << "position: " << position_world_space.x << " " << position_world_space.y << " " << position_world_space.z << std::endl;
        //     out << "lookat: " << position_look_at_world_space.x << " " << position_look_at_world_space.y << " " << position_look_at_world_space.z << std::endl;
        //     out << "rotation: " << rotation_x << " " << rotation_y << " " << orbital_radius << std::endl;
        // } else {
        //     out << orbital;
        //     out << position_world_space.x;
        //     out << position_world_space.y;
        //     out << position_world_space.z;
        //     out << position_look_at_world_space.x;
        //     out << position_look_at_world_space.y;
        //     out << position_look_at_world_space.z;
        //     out << rotation_x;
        //     out << rotation_y;
        //     out << orbital_radius;
        // }
    }
    void readFrom(std::istream &in, bool human_readable = false) {
        if (human_readable) {
            std::string tmp;
            in >> tmp; // "orbital:"
            in >> orbital;
            in >> tmp; // "position:"
            in >> position_world_space.x;
            in >> position_world_space.y;
            in >> position_world_space.z;
            in >> tmp; // *lookat*
            in >> position_look_at_world_space.x;
            in >> position_look_at_world_space.y;
            in >> position_look_at_world_space.z;
            in >> tmp; // "rotation:"
            in >> rotation_x;
            in >> rotation_y;
            in >> orbital_radius;
        } else {
            in.read(reinterpret_cast<char *>(&orbital), sizeof(orbital));
            in.read(reinterpret_cast<char *>(&position_world_space), sizeof(position_world_space));
            in.read(reinterpret_cast<char *>(&position_look_at_world_space), sizeof(position_look_at_world_space));
            in.read(reinterpret_cast<char *>(&rotation_x), sizeof(rotation_x));
            in.read(reinterpret_cast<char *>(&rotation_y), sizeof(rotation_y));
            in.read(reinterpret_cast<char *>(&orbital_radius), sizeof(orbital_radius));
        }

        // std::string tmp;
        // if (human_readable)
        //     in >> tmp; // "orbital:"
        // in >> orbital;
        // if (human_readable)
        //     in >> tmp; // "position:"
        // in >> position_world_space.x;
        // in >> position_world_space.y;
        // in >> position_world_space.z;
        // if (human_readable)
        //     in >> tmp; // *lookat*
        // in >> position_look_at_world_space.x;
        // in >> position_look_at_world_space.y;
        // in >> position_look_at_world_space.z;
        // if (human_readable)
        //     in >> tmp; // "rotation:"
        // in >> rotation_x;
        // in >> rotation_y;
        // in >> orbital_radius;
    }

  private:
    std::function<void()> m_cameraUpdateFunction = nullptr;
};

} // namespace vvv
