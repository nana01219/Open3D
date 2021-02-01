// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#pragma once

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "open3d/core/Dtype.h"
#include "open3d/core/Tensor.h"
#include "open3d/core/kernel/UnaryEW.h"
#include "open3d/geometry/Image.h"
#include "open3d/t/geometry/Geometry.h"

namespace open3d {
namespace t {
namespace geometry {

/// \class Image
///
/// \brief The Image class stores image with customizable rols, cols, channels,
/// dtype and device.
class Image : public Geometry {
public:
    /// \brief Constructor for image.
    ///
    /// Row-major storage is used, similar to OpenCV. Use (row, col, channel)
    /// indexing order for image creation and accessing. In general, (r, c, ch)
    /// are the preferred variable names for consistency, and avoid using width,
    /// height, u, v, x, y for coordinates.
    ///
    /// \param rows Number of rows of the image, i.e. image height. \p rows must
    /// be non-negative.
    /// \param cols Number of columns of the image, i.e. image width. \p cols
    /// must be non-negative.
    /// \param channels Number of channels of the image. E.g. for RGB image,
    /// channels == 3; for grayscale image, channels == 1. \p channels must be
    /// greater than 0.
    /// \param dtype Data type of the image.
    /// \param device Device where the image is stored.
    Image(int64_t rows = 0,
          int64_t cols = 0,
          int64_t channels = 1,
          core::Dtype dtype = core::Dtype::Float32,
          const core::Device &device = core::Device("CPU:0"));

    /// \brief Construct from a tensor. The tensor won't be copied and memory
    /// will be shared.
    ///
    /// \param tensor: Tensor of the image. The tensor must be contiguous. The
    /// tensor must be 2D (rows, cols) or 3D (rows, cols, channels).
    Image(const core::Tensor &tensor);

    virtual ~Image() override {}

    /// \enum FilterType
    ///
    /// \brief Specifies the Image filter type.
    enum class FilterType {
        /// Gaussian filter.
        Gaussian1x3,
        Gaussian1x5,
        Gaussian3x1,
        Gaussian5x1,
        Gaussian3x3,
        Gaussian5x5,
        Gaussian7x7,
        Gaussian9x9,
        Gaussian11x11,
        Gaussian13x13,
        Gaussian15x15,
        /// Horizotal Sobel filter.
        SobelHorizontal,
        /// Vertical Sobel filter.
        SobelVertical,
    };

public:
    /// Clear image contents by resetting the rows and cols to 0, while
    /// keeping channels, dtype and device unchanged.
    Image &Clear() override {
        data_ = core::Tensor({0, 0, GetChannels()}, GetDtype(), GetDevice());
        return *this;
    }

    /// Returns true if rows * cols * channels == 0.
    bool IsEmpty() const override {
        return GetRows() * GetCols() * GetChannels() == 0;
    }

    /// Reinitialize image with new parameters.
    Image &Reset(int64_t rows = 0,
                 int64_t cols = 0,
                 int64_t channels = 1,
                 core::Dtype dtype = core::Dtype::Float32,
                 const core::Device &device = core::Device("CPU:0"));

public:
    /// Get the number of rows of the image.
    int64_t GetRows() const { return data_.GetShape()[0]; }

    /// Get the number of columns of the image.
    int64_t GetCols() const { return data_.GetShape()[1]; }

    /// Get the number of channels of the image.
    int64_t GetChannels() const { return data_.GetShape()[2]; }

    /// Get dtype of the image.
    core::Dtype GetDtype() const { return data_.GetDtype(); }

    /// Get device of the image.
    core::Device GetDevice() const { return data_.GetDevice(); }

    /// Get pixel(s) in the image. If channels == 1, returns a tensor with shape
    /// {}, otherwise returns a tensor with shape {channels,}. The returned
    /// tensor is a slice of the image's tensor, so when modifying the slice,
    /// the original tensor will also be modified.
    core::Tensor At(int64_t r, int64_t c) const {
        if (GetChannels() == 1) {
            return data_[r][c][0];
        } else {
            return data_[r][c];
        }
    }

    /// Get pixel(s) in the image. Returns a tensor with shape {}.
    core::Tensor At(int64_t r, int64_t c, int64_t ch) const {
        return data_[r][c][ch];
    }

    /// Get raw buffer of the Image data.
    void *GetDataPtr() { return data_.GetDataPtr(); }

    /// Get raw buffer of the Image data.
    const void *GetDataPtr() const { return data_.GetDataPtr(); }

    /// Retuns the underlying Tensor of the Image.
    core::Tensor AsTensor() const { return data_; }

    /// Transfer the image to a specified device.
    /// \param device The targeted device to convert to.
    /// \param copy If true, a new image is always created; if false, the
    /// copy is avoided when the original image is already on the targeted
    /// device.
    Image To(const core::Device &device, bool copy = false) const {
        return Image(data_.To(device, copy));
    }

    /// Returns copy of the image on the same device.
    Image Clone() const { return To(GetDevice(), /*copy=*/true); }

    /// Transfer the image to CPU.
    ///
    /// If the image is already on CPU, no copy will be performed.
    Image CPU() const { return To(core::Device("CPU:0")); };

    /// Transfer the image to a CUDA device.
    ///
    /// If the image is already on the specified CUDA device, no copy will
    /// be performed.
    Image CUDA(int device_id = 0) const {
        return To(core::Device(core::Device::DeviceType::CUDA, device_id));
    };

    /// Returns an Image with the specified \p dtype.
    /// \param dtype The targeted dtype to convert to.
    /// \param copy If true, a new tensor is always created; if false, the copy
    /// is avoided when the original tensor already has the targeted dtype.
    /// \param scale Optional scale value. This is 1./255 for UInt8 ->
    /// Float{32,64}, 1./65535 for UInt16 -> Float{32,64} and 1 otherwise
    /// \param offset Optional shift value. Default 0.
    Image To(core::Dtype dtype,
             bool copy = false,
             utility::optional<double> scale = utility::nullopt,
             double offset = 0.0) const;

    /// Function to linearly transform pixel intensities in place.
    /// image = scale * image + offset.
    /// \param scale First multiply image pixel values with this factor. This
    /// should be positive for unsigned dtypes.
    /// \param offset Then add this factor to all image pixel values.
    /// \return Reference to self.
    Image &LinearTransform(double scale = 1.0, double offset = 0.0) {
        To(GetDtype(), false, scale, offset);
        return *this;
    }

    /// Converts a 3-channel RGB image to a new 1-channel Grayscale image.
    Image RGBToGray() const;

    /// Function to filter image with pre-defined filtering type.
    /// \param filter_type pre-defined filtering type (Gaussian, Sobel).
    Image Filter(Image::FilterType filter_type) const;

    /// Return a new image after performing morphological dilation. Supported
    /// datatypes are UInt8, UInt16 and Float32 with {1, 3, 4} channels. An
    /// 8-connected neighborhood is used to create the dilation mask.
    /// \param half_kernel_size A dilation mask of size 2*half_kernel_size+1 is
    /// used.
    Image Dilate(int half_kernel_size = 1) const;

    /// Return a new image after bilateral filtering.
    Image BilateralFilter(int half_kernel_size = 1,
                          float value_sigma = 400.0f,
                          float dist_sigma = 100.0f) const;

    /// Return a new image after Gaussian filtering.
    /// A fixed sigma is computed by sigma = 0.4F + (mask width / 2) * 0.6F.
    /// Possible kernel_size: odd numbers >= 3 are supported for CPU, and only
    /// up to 15 are supported for GPU.
    Image GaussianFilter(int kernel_size = 3) const;

    /// Return a pair of new gradient images (dx, dy) after Sobel filtering.
    /// Possible kernel_size: 3 and 5.
    std::pair<Image, Image> SobelFilter(int kernel_size = 3) const;

    /// Compute min 2D coordinates for the data (always {0, 0}).
    core::Tensor GetMinBound() const {
        return core::Tensor::Zeros({2}, core::Dtype::Int64);
    }

    /// Compute max 2D coordinates for the data ({rows, cols}).
    core::Tensor GetMaxBound() const {
        return core::Tensor(std::vector<int64_t>{GetRows(), GetCols()}, {2},
                            core::Dtype::Int64);
    }

    /// Create from a legacy Open3D Image.
    static Image FromLegacyImage(
            const open3d::geometry::Image &image_legacy,
            const core::Device &Device = core::Device("CPU:0"));

    /// Convert to legacy Image type.
    open3d::geometry::Image ToLegacyImage() const;

    /// Text description
    std::string ToString() const;

    /// Do we use IPP ICV for accelerating image processing operations?
#ifdef WITH_IPPICV
    static constexpr bool HAVE_IPPICV = true;
#else
    static constexpr bool HAVE_IPPICV = false;
#endif

protected:
    /// Internal data of the Image, represented as a contiguous 3D tensor of
    /// shape {rols, cols, channels}. Image properties can be obtained from the
    /// tensor.
    core::Tensor data_;
};

}  // namespace geometry
}  // namespace t
}  // namespace open3d
