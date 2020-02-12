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

#ifndef SYCL_CUDA_STREAM_HPP
#define SYCL_CUDA_STREAM_HPP

#include <cuda.h>
#include <cudnn.h>
#include <cublas_v2.h>

#include "common/engine.hpp"
#include "sycl/sycl_stream.hpp"

namespace dnnl {
namespace impl {
namespace cuda {

class sycl_cuda_stream_t : public dnnl::impl::sycl::sycl_stream_t {
public:
    using base_t = dnnl::impl::sycl::sycl_stream_t;
    cublasHandle_t &get_cublas_handle();
    cudnnHandle_t &get_cudnn_handle();

    static status_t create_stream(
            stream_t **stream, engine_t *engine, unsigned generic_flags) {
        unsigned flags;
        status_t status = base_t::init_flags(&flags, generic_flags);
        if (status != status::success) return status;

        std::unique_ptr<sycl_cuda_stream_t> sycl_stream(
                new sycl_cuda_stream_t(engine, flags));
        if (!sycl_stream) return status::out_of_memory;

        status = sycl_stream->init();
        if (status != status::success) return status;
        *stream = sycl_stream.release();
        return status::success;
    }

    static status_t create_stream(
            stream_t **stream, engine_t *engine, cl::sycl::queue &queue) {
        unsigned flags;
        status_t status = base_t::init_flags(&flags, queue);
        if (status != status::success) return status;

        std::unique_ptr<sycl_cuda_stream_t> sycl_stream(
                new sycl_cuda_stream_t(engine, flags, queue));

        status = sycl_stream->init();
        if (status != status::success) return status;

        *stream = sycl_stream.release();
        return status::success;
    }
    status_t interop_task(std::function<void(cl::sycl::handler &)>);
    CUstream get_underlying_stream();
    CUcontext get_underlying_context();

private:
    status_t init();
    sycl_cuda_stream_t(engine_t *engine, unsigned flags, cl::sycl::queue &queue)
        : base_t(engine, flags, queue) {}
    sycl_cuda_stream_t(engine_t *engine, unsigned flags)
        : base_t(engine, flags) {}
};

} // namespace cuda
} // namespace impl
} // namespace dnnl

#endif // SYCL_CUDA_STREAM_HPP
