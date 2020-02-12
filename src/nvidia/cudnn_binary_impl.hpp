/***************************************************************************
 *  Copyright 2020 Codeplay Software Limited
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  For your convenience, a copy of the License has been included in this
 *  repository.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 **************************************************************************/

#ifndef CUDNN_BINARY_IMPL_HPP
#define CUDNN_BINARY_IMPL_HPP

#include "cudnn.h"

#include "nvidia/sycl_cuda_utils.hpp"

namespace dnnl {
namespace impl {
namespace cuda {

struct cudnn_binary_impl_base_t {
    enum io { src_0 = 0, src_1, dst_0, NUM_IO };
    cudnnDataType_t data_types[NUM_IO];
    int ndims;
    int dims[NUM_IO][DNNL_MAX_NDIMS];
    cudnnOpTensorDescriptor_t op_desc = nullptr;
    cudnnTensorDescriptor_t tensor_descs[NUM_IO] = {};
    cudnnOpTensorOp_t alg_kind;
    float alpha[2];
    float beta = 0.0f;

    virtual ~cudnn_binary_impl_base_t() {
        if (op_desc) {
            CUDNN_EXECUTE_FUNC_V(cudnnDestroyOpTensorDescriptor, op_desc);
        }
        for (size_t i = 0; i < NUM_IO; i++) {
            if (tensor_descs[i]) {
                CUDNN_EXECUTE_FUNC_V(
                        cudnnDestroyTensorDescriptor, tensor_descs[i]);
            }
        }
    }

    virtual status_t init(const binary_pd_t *pd) = 0;

    virtual void execute(cudnnHandle_t handle, void *a, void *b, void *c) {
        CUDNN_EXECUTE_FUNC(cudnnOpTensor, handle, op_desc, &alpha[0],
                tensor_descs[src_0], a, &alpha[1], tensor_descs[src_1], b,
                &beta, tensor_descs[dst_0], c);
    }

    virtual status_t create_and_set_op_descriptor() {
        CHECK(CUDNN_EXECUTE_FUNC_S(cudnnCreateOpTensorDescriptor, &op_desc));

        CHECK(CUDNN_EXECUTE_FUNC_S(cudnnSetOpTensorDescriptor, op_desc,
                alg_kind, cudnnDataType_t::CUDNN_DATA_FLOAT,
                cudnnNanPropagation_t::CUDNN_NOT_PROPAGATE_NAN));

        return status::success;
    }

    status_t convert_alg_kind(
            alg_kind_t alg_kind, cudnnOpTensorOp_t *cuda_alg_kind) {
        switch (alg_kind) {
            case alg_kind::binary_add:
                *cuda_alg_kind = cudnnOpTensorOp_t::CUDNN_OP_TENSOR_ADD;
                break;
            case alg_kind::binary_mul:
                *cuda_alg_kind = cudnnOpTensorOp_t::CUDNN_OP_TENSOR_MUL;
                break;
            case alg_kind::binary_min:
                *cuda_alg_kind = cudnnOpTensorOp_t::CUDNN_OP_TENSOR_MIN;
                break;
            case alg_kind::binary_max:
                *cuda_alg_kind = cudnnOpTensorOp_t::CUDNN_OP_TENSOR_MAX;
                break;
            default: return status::unimplemented;
        }
        return status::success;
    }
};

struct cudnn_binary_impl_t : public cudnn_binary_impl_base_t {
    int strides[NUM_IO][DNNL_MAX_NDIMS];

    virtual status_t init(const binary_pd_t *pd) override {
        // If any of the dimensions are 0 we should not continue with creating
        // cudnn descriptors
        if (has_zero_dims(pd->src_md(0)->dims, pd->ndims())) {
            return status::success;
        }
        if (pd->ndims() > CUDNN_DIM_MAX) { return status::invalid_arguments; }
        ndims = pd->ndims() < 4 ? 4 : pd->ndims();
        convert_dims(pd->src_md(0)->padded_dims, dims[src_0], pd->ndims());
        convert_dims(pd->src_md(1)->padded_dims, dims[src_1], pd->ndims());
        convert_dims(pd->dst_md()->padded_dims, dims[dst_0], pd->ndims());

        convert_dims(pd->src_md(0)->format_desc.blocking.strides,
                strides[src_0], pd->ndims());
        convert_dims(pd->src_md(1)->format_desc.blocking.strides,
                strides[src_1], pd->ndims());
        convert_dims(pd->dst_md()->format_desc.blocking.strides, strides[dst_0],
                pd->ndims());
        alg_kind_t alg = pd->desc()->alg_kind;
        auto alg_ok = convert_alg_kind(alg, &alg_kind);
        if (alg_ok != status::success) { return status::unimplemented; }

        CHECK(convert_data_type(pd->src_md(0), &data_types[src_0]));
        CHECK(convert_data_type(pd->src_md(1), &data_types[src_1]));
        CHECK(convert_data_type(pd->dst_md(), &data_types[dst_0]));

        bool do_scaling = pd->src_md(0)->data_type == dnnl_data_type_t::dnnl_s8;
        auto scales_0 = pd->attr()->scales_.get(1).scales_;
        auto scales_1 = pd->attr()->scales_.get(2).scales_;
        alpha[0] = do_scaling ? scales_0[0] : 1.0f;
        alpha[1] = do_scaling ? scales_1[0] : 1.0f;

        CHECK(create_and_set_tensor_descriptor(&tensor_descs[src_0],
                data_types[src_0], ndims, dims[src_0], strides[src_0]));
        CHECK(create_and_set_tensor_descriptor(&tensor_descs[src_1],
                data_types[src_1], ndims, dims[src_1], strides[src_1]));
        CHECK(create_and_set_tensor_descriptor(&tensor_descs[dst_0],
                data_types[dst_0], ndims, dims[dst_0], strides[dst_0]));
        CHECK(create_and_set_op_descriptor());
        return status::success;
    }
};

} // namespace cuda
} // namespace impl
} // namespace dnnl

#endif // CUDNN_BINARY_IMPL_HPP
