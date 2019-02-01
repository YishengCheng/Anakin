#ifndef ANAKIN_SABER_FUNCS_CALIBRATE_H
#define ANAKIN_SABER_FUNCS_CALIBRATE_H

#include "saber/core/tensor.h"
#include "saber/core/context.h"
#include <vector>
namespace anakin {
namespace saber {

// keep origin layout
template<typename TargetType, typename dst_dtype, typename src_dtype>
SaberStatus flatten_calibrate(
        Tensor<TargetType> &out_tensor,
        const Tensor<TargetType> &in_tensor,
        Context<TargetType> &ctx);

template<typename TargetType>
SaberStatus conv_calibrate_fp32_int8_c4(
        Tensor<TargetType> &out_tensor,
        const Tensor<TargetType> &in_tensor,
        const float in_scale, Context<TargetType> ctx);

template<typename TargetType>
SaberStatus conv_calibrate_int32_fp32(
        Tensor<TargetType> &out_tensor,
        const Tensor<TargetType> &in_tensor,
        const float in_scale, const float* weight_scale,
        Context<TargetType> ctx);

template<typename TargetType>
SaberStatus conv_calibrate_int8_c4_fp32(
        Tensor<TargetType> &out_tensor,
        const Tensor<TargetType> &in_tensor,
        const float* weight_scale,
        Context<TargetType> ctx);
template<typename TargetType>
SaberStatus calibrate_int8_c4_fp32(
        Tensor<TargetType> &out_tensor,
        const Tensor<TargetType> &in_tensor,
        const float out_scale,
        Context<TargetType> ctx);
template<typename TargetType,
        LayoutType dst_layout,
        typename dst_dtype,
        LayoutType src_layout,
        typename src_dtype>
SaberStatus conv_data_calibrate(Tensor<TargetType> &out_tensor,
        const Tensor<TargetType> &in_tensor,
        const float in_scale,
        const float* weight_scale,
        Context<TargetType> ctx);

template<typename TargetType>
void float2char(bool col_direct, signed char* dst, const float* src,
                float *scale, int height, int width,
                Context<TargetType> ctx);

template<typename TargetType>
void fix2float(float * dst,
               const float *sA, const float *sB,
               const float alpha, const float beta, int height, int width,
               Context<TargetType> ctx);

template <typename TargetType>
SaberStatus get_tensor_scale(std::vector<float> &vector_scale,
        const Tensor<TargetType> &tensor, const int axis, bool scale_per_k) {

    int out_dims = tensor.valid_shape()[axis];
    if (scale_per_k) {
        vector_scale.resize(out_dims);
    } else {
        vector_scale.resize(1);
    }

    const float* in_data = (const float*)(tensor.data());
    if (scale_per_k) {
        long long inner_dim = tensor.count_valid(axis + 1, tensor.dims());
        for (int c = 0; c < out_dims; ++c) {
            float max_val = -1.f;

            for (int i = 0; i < inner_dim; ++i) {
                float read_data = fabs(in_data[i]);
                max_val = (read_data > max_val) ? read_data : max_val;
            }

            vector_scale[c] = max_val / 127.f;
            in_data += inner_dim;
        }
    } else {
        long long count = tensor.valid_size();
        float max_val = -1.f;
        for (int i = 0; i < count; ++i) {
            float read_data = fabs(in_data[i]);
            max_val = (read_data > max_val) ? read_data : max_val;
        }
        vector_scale[0] = max_val / 127.f;
    }
}

template<typename TargetType, typename TargetType_H>
SaberStatus convert_weights_to_nchw_c4_host(Tensor<TargetType_H>& out_tensor,
        const Tensor<TargetType_H>& in_tensor, const Context<TargetType> &ctx,
        bool scale_per_k = false) {

    int output_channel = out_tensor.num();
    std::vector<float> vector_weight_scale;
    get_tensor_scale(vector_weight_scale, in_tensor, 0, scale_per_k);

    int o_num = out_tensor.num();
    int out_channel = in_tensor.channel();
    int out_channel_4 = in_tensor.channel() / 4;
    bool channel_rest_4 = (out_channel  & 0x3) != 0;
    out_channel_4 += channel_rest_4 ? 1 : 0;
    int o_height = out_tensor.height();
    int o_width = out_tensor.width();

    int out_n_stride = out_channel_4 * o_height * o_width;
    int out_c_stride = o_height * o_width;
    int out_h_stride = o_width;

    Shape in_stride = in_tensor.get_stride();
    const float* in_weight_data = (const float*)in_tensor.data();
    char* out_weight_data = (char*)out_tensor.mutable_data();

    for (int idx = 0; idx < o_num * out_channel_4 * o_height * o_width; ++idx) {

        int n = (idx / (out_n_stride)) % o_num;
        int in_offset = ((idx / (out_n_stride)) % o_num) * in_stride[0]
                        + ((idx / (out_c_stride)) % out_channel_4) * (in_stride[1] * 4)
                        + ((idx / (out_h_stride)) % o_height) * in_stride[2]
                        + (idx % o_width) * in_stride[3];
        int read_channel = ((idx / (out_c_stride)) % out_channel_4);
        int out_offset = ((idx / (out_n_stride)) % o_num) * out_n_stride
                         + ((idx / (out_c_stride)) % out_channel_4) * out_c_stride
                         + ((idx / (out_h_stride)) % o_height) * out_h_stride
                         + (idx % o_width);
        float scale = scale_per_k ? vector_weight_scale[n] : vector_weight_scale[0];
        bool p0, p1, p2, p3;
        p0 = (4 * read_channel + 0) < out_channel;
        p1 = (4 * read_channel + 1) < out_channel;
        p2 = (4 * read_channel + 2) < out_channel;
        p3 = (4 * read_channel + 3) < out_channel;
        float read;
        if (p0) {
            read = in_weight_data[in_offset + 0 * in_stride[1]];
        } else {
            read = 0.f;
        }
        out_weight_data[out_offset * 4 + 0] = (char)(round(read / scale));
        if (p1) {
            read = in_weight_data[in_offset + 1 * in_stride[1]];
        } else {
            read = 0;
        }
        out_weight_data[out_offset * 4 + 1] = (char)(round(read / scale));
        if (p2) {
            read = in_weight_data[in_offset + 2 * in_stride[1]];
        } else {
            read = 0;
        }
        out_weight_data[out_offset * 4 + 2] = (char)(round(read / scale));
        if (p3) {
            read = in_weight_data[in_offset + 3 * in_stride[1]];
        } else {
            read = 0;
        }
        out_weight_data[out_offset * 4 + 3] = (char)(round(read / scale));
    }
    out_tensor.set_scale(vector_weight_scale);
//    for (auto i : vector_weight_scale) {
//        LOG(INFO) << i;
//    }
    return SaberSuccess;
}

template<typename TargetType, typename TargetType_H>
SaberStatus convert_weights_to_direct(Tensor<TargetType_H>& out_tensor,
        const Tensor<TargetType_H>& in_tensor, const Context<TargetType> &ctx,
        bool scale_per_k = false) {

    Tensor<TargetType_H> weight_temp;
    weight_temp.re_alloc(in_tensor.valid_shape(), AK_INT8);
//    CHECK_EQ((in_tensor.channel() % 4), 0);
//    CHECK_EQ((in_tensor.num() % 4), 0);
    int input_channel = in_tensor.channel();
    int output_channel = in_tensor.num();
    std::vector<float> vector_weight_scale;
    get_tensor_scale(vector_weight_scale, in_tensor, 0, scale_per_k);

    int num = in_tensor.num();
    int channel = in_tensor.channel();
    int channel_4 = channel >> 2;
    bool channel_rest_4 = (channel & 0x3) != 0;
    channel_4 += channel_rest_4 ? 1 : 0;
    int height = in_tensor.height();
    int width = in_tensor.width();
    int out_n_stride = channel * height * width;
    int out_c_stride = height * width;
    int out_h_stride = width;

    Shape in_stride = in_tensor.get_stride();
    const float* in_weight_data = (const float*)in_tensor.data();
    char* out_weight_data = (char*)out_tensor.mutable_data();
    // data scale
    for (int idx = 0; idx < num * channel * height * width; ++idx) {
        int n = (idx / (out_n_stride)) % num;
        float scale = scale_per_k ? vector_weight_scale[n] : vector_weight_scale[0];
        out_weight_data[idx] = (char)(round(
                in_weight_data[idx] / scale));
    }
    // finished scale
    // layout transform
    char *weight_temp_ptr = (char*)weight_temp.mutable_data();
    const int in_loop = in_tensor.channel() * in_tensor.height() * in_tensor.width();
    for (int var_k = 0; var_k < in_tensor.num(); var_k++) {
        for (int var_crs = 0; var_crs < in_loop; var_crs++) {
            weight_temp_ptr[var_crs * in_tensor.num() + var_k] =
                    out_weight_data[var_k * in_loop + var_crs];
        }
    }
    int read_in = 0;
    int write_out = 0;
    const int out_loop = channel_4;
    const int inner_loop =  in_tensor.num() * in_tensor.height() * in_tensor.width() * 4;
    for (int i = 0; i < out_loop; ++i) {
        for (int j = 0; j < inner_loop; ++j) {
            write_out = i * inner_loop + j;
            if ((i * 4 + j % 4) < channel) {
                read_in = ((i * 4) + (j % 4)) * (inner_loop / 4) + j / 4;
                out_weight_data[write_out] = weight_temp_ptr[read_in];
            } else {
                out_weight_data[write_out] = 0;
            }
        }
    }
    // finished transform

    out_tensor.set_scale(vector_weight_scale);

//    for (auto i : vector_weight_scale) {
//        LOG(INFO) << i;
//    }
    return SaberSuccess;
}

template<typename TargetType, typename TargetType_H>
SaberStatus convert_bias_host(Tensor<TargetType_H>& out_tensor,
        const Tensor<TargetType_H>& in_tensor,
        float in_scale, std::vector<float> vector_weight_scale,
        Context<TargetType> ctx, bool scale_per_k = false) {
    unsigned long weight_size = vector_weight_scale.size();
    unsigned long bias_size = in_tensor.size();
    CHECK_GT(in_scale, 0);
    CHECK_GT(weight_size, 0);

    const float* in_data = (const float*)in_tensor.data();
    float* out_data = (float*)out_tensor.mutable_data();

    for (int i = 0; i < bias_size; ++i) {
        float weights_scale = (scale_per_k && weight_size != 1) ? vector_weight_scale[i] : vector_weight_scale[0];
        out_data[i] = in_data[i] / in_scale / weights_scale;
    }

    return SaberSuccess;
}

//// reverse quantization
//template <typename TargetType, typename TargetType_H>
//class Dequantization {
//public:
//
//};
//
//// high precision quantize to low precision
//template <typename TargetType, typename TargetType_H>
//class Quantization {
//public:
//
//};
//
//// scale transform while keep precision
//template <typename TargetType, typename TargetType_H>
//class Requantization {
//public:
//
//};

} // namespace saber
} // namespace anakin

#endif
