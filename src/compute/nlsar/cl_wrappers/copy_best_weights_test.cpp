/* Copyright 2015, 2016 Gerald Baier
 *
 * This file is part of despeckCL.
 *
 * despeckCL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * despeckCL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with despeckCL. If not, see <http://www.gnu.org/licenses/>.
 */

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

#include <complex>
#include <random>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "unit_test_helper.h"

#include "copy_best_weights.h"

using namespace nlsar;
using testing::Pointwise;

TEST(copy_best_weights, rand_test) {

        // data setup
        const int height             = 30;
        const int width              = 50;
        const int nparams            = 3;
        const int search_window_size = 1;

        std::vector<float> all_weights          (nparams*height*width, -1.0);
        std::vector<int>   best_params          (        height*width, -1.0);
        std::vector<float> best_weights         (        height*width, -1.0);
        std::vector<float> desired_best_weights (        height*width, -1.0);

        for(int bp = 0; bp < nparams; bp++) {
            for(int i = 0; i < height*width; i++) {
                all_weights[bp*height*width + i] = bp;
            }
        }

        static std::default_random_engine rand_eng{};
        static std::uniform_int_distribution<int> dist_params(0, nparams-1);

        for(int i = 0; i < height*width; i++) {
            best_params[i] = dist_params(rand_eng);
            desired_best_weights[i] = best_params[i];
        }

        // opencl setup
        auto cl_devs = get_platform_devs(0);
        cl::Context context{cl_devs};

        std::vector<cl::Device> devices;
        context.getInfo(CL_CONTEXT_DEVICES, &devices);

        cl::CommandQueue cmd_queue{context};

        // kernel setup
        const int block_size = 16;
        copy_best_weights KUT{block_size, context};

        // allocate memory
        cl::Buffer device_all_weights  {context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, nparams*height*width*sizeof(float), all_weights.data(), NULL};
        cl::Buffer device_best_params  {context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,         height*width*sizeof(float), best_params.data(), NULL};
        cl::Buffer device_best_weights {context, CL_MEM_READ_WRITE,                               height*width*sizeof(float), NULL, NULL};

        KUT.run(cmd_queue, 
                device_all_weights,
                device_best_params,
                device_best_weights,
                height,
                width,
                search_window_size);

        cmd_queue.enqueueReadBuffer(device_best_weights, CL_TRUE, 0, height*width*sizeof(float), best_weights.data(), NULL, NULL);

        ASSERT_THAT(best_weights, Pointwise(FloatNearPointwise(1e-4), desired_best_weights));
}
