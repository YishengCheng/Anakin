/* Copyright (c) 2018 Anakin Authors, Inc. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "framework/operators/flatten.h"

namespace anakin {

namespace ops {

#define INSTANCE_FLATTEN(Ttype, Ptype) \
template<> \
void Flatten<Ttype, Ptype>::operator()(OpContext<Ttype>& ctx, \
    const std::vector<Tensor4dPtr<Ttype> >& ins, \
    std::vector<Tensor4dPtr<Ttype> >& outs) { \
    auto* impl = \
        static_cast<FlattenHelper<Ttype, Ptype>*>(this->_helper); \
    auto& param = static_cast<FlattenHelper<Ttype, Ptype>*> \
                  (this->_helper)->_param_flatten; \
    impl->_funcs_flatten(ins, outs, param, ctx); \
}

template<typename Ttype, Precision Ptype>
Status FlattenHelper<Ttype, Ptype>::InitParam() {
    DLOG(WARNING) << "Parsing Flatten op parameter.";
    auto start_axis = GET_PARAMETER(int, start_axis);
    auto end_axis = GET_PARAMETER(int, end_axis);

    saber::FlattenParam<Ttype> flatten_param(start_axis, end_axis);
    _param_flatten = flatten_param;
    return Status::OK();
}

template<typename Ttype, Precision Ptype>
Status FlattenHelper<Ttype, Ptype>::Init(OpContext<Ttype> &ctx, const std::vector<Tensor4dPtr<Ttype>> &ins,
                           std::vector<Tensor4dPtr<Ttype>> &outs) {
    SABER_CHECK(_funcs_flatten.init(ins, outs, _param_flatten, SPECIFY, SABER_IMPL, ctx));
    return Status::OK();
}
template<typename Ttype, Precision Ptype>
Status FlattenHelper<Ttype, Ptype>::InferShape(const std::vector<Tensor4dPtr<Ttype>> &ins,
                                 std::vector<Tensor4dPtr<Ttype>> &outs) {
    SABER_CHECK(_funcs_flatten.compute_output_shape(ins, outs, _param_flatten));
    return Status::OK();
}
#ifdef USE_CUDA
INSTANCE_FLATTEN(NV, Precision::FP32);
template class FlattenHelper<NV, Precision::FP32>;
ANAKIN_REGISTER_OP_HELPER(Flatten, FlattenHelper, NV, Precision::FP32);
#endif

#ifdef AMD_GPU
INSTANCE_FLATTEN(AMD, Precision::FP32);
template class FlattenHelper<AMD, Precision::FP32>;
ANAKIN_REGISTER_OP_HELPER(Flatten, FlattenHelper, AMD, Precision::FP32);
#endif

#if defined USE_X86_PLACE || defined BUILD_LITE
INSTANCE_FLATTEN(X86, Precision::FP32);
template class FlattenHelper<X86, Precision::FP32>;
ANAKIN_REGISTER_OP_HELPER(Flatten, FlattenHelper, X86, Precision::FP32);
#endif

#ifdef USE_ARM_PLACE
INSTANCE_FLATTEN(ARM, Precision::FP32);
template class FlattenHelper<ARM, Precision::FP32>;
ANAKIN_REGISTER_OP_HELPER(Flatten, FlattenHelper, ARM, Precision::FP32);
#endif

//! register op
ANAKIN_REGISTER_OP(Flatten)
.Doc("Flatten operator")
#ifdef USE_CUDA
.__alias__<NV, Precision::FP32>("flatten")
#endif
#ifdef USE_ARM_PLACE
.__alias__<ARM, Precision::FP32>("flatten")
#endif
#if defined USE_X86_PLACE || defined BUILD_LITE
.__alias__<X86, Precision::FP32>("flatten")
#endif
#ifdef AMD_GPU
.__alias__<AMD, Precision::FP32>("flatten")
#endif
.num_in(1)
.num_out(1);

} /* namespace ops */

} /* namespace anakin */


