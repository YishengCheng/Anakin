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

#ifndef ANAKIN_OPERATOR_YOLO_BOX_H
#define ANAKIN_OPERATOR_YOLO_BOX_H

#include "framework/core/base.h"
#include "framework/core/data_types.h"
#include "framework/core/operator/operator.h"
#include "utils/logger/logger.h"
#include "saber/funcs/yolo_box.h"

namespace anakin {

namespace ops {

template<typename Ttype, Precision Ptype>
class YoloBoxHelper;

/// pooling op
/**
 * \brief operation of ops class
 * public inheritance Operator
 */
template<typename Ttype, Precision Ptype>
class YoloBox : public Operator<Ttype, Ptype> {
public:
    YoloBox() {}

    /// forward impl
    virtual void operator() (OpContext<Ttype> &ctx,
                             const std::vector<Tensor4dPtr<Ttype> >& ins,
                             std::vector<Tensor4dPtr<Ttype> >& outs) {
        LOG(ERROR) << "Not Impl Yet Operator YoloBox< Ttype("
                   << target_name<Ttype>::value << "), Precision("
                   << Ptype << ") >";
    }

    friend class YoloBoxHelper<Ttype, Ptype>;
};

/**
 * \breif provide defined help for some operation
 *  public inheritance OperatorHelper
 *  including init operation context and the size of shape
 */
template<typename Ttype, Precision Ptype>
class YoloBoxHelper : public OperatorHelper<Ttype, Ptype> {
public:
    YoloBoxHelper()=default;

    ~YoloBoxHelper();

    Status InitParam() override;

    /**
    * \brief initial all the resource needed by pooling
    * \param ctx stand for operation context
    * \param ins stand for input tensor vector
    * \param outs stand for output tensor vector
    * \return status
    */
    Status Init(OpContext<Ttype> &ctx,
                const std::vector<Tensor4dPtr<Ttype> >& ins,
                std::vector<Tensor4dPtr<Ttype> >& outs) override;

    /**
    * \brief infer the shape of output and input.
    * \param ins stand for input tensor vector
    * \param outs stand for output tensor vector
    * \return status
    */
    Status InferShape(const std::vector<Tensor4dPtr<Ttype> >& ins,
                      std::vector<Tensor4dPtr<Ttype> >& outs) override;

public:
    ///< _param_yolo_box stand for yolo_box parameter
    saber::YoloBoxParam<Ttype> _param_yolo_box;
    ///< _funcs_yolo_box stand for yolo_box function
    saber::YoloBox<Ttype, PrecisionWrapper<Ptype>::saber_type> _funcs_yolo_box;
};



} /* namespace ops */

} /* namespace anakin */

#endif
