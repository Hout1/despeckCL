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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "unit_test_helper.h"

#include <string>
#include <random>

#include "compute_alphas.h"

using namespace nlsar;
using testing::Pointwise;
using testing::Each;

TEST(compute_alphas, single_val_check) {

        // data setup
        const int height = 50;
        const int width = 40;
        const int dimensions = 2;
        const int nlooks = 1;

        std::vector<float> intensities_nl      (height*width*dimensions,  0.0);
        std::vector<float> weighted_variances  (height*width*dimensions,  2.0);
        std::vector<float> alphas              (height*width,            -1.0);
        std::vector<float> desired_alphas      (height*width,             1.0);

        // opencl setup
        auto cl_devs = get_platform_devs(0);
        cl::Context context{cl_devs};
        cl::CommandQueue cmd_queue{context};

        // kernel setup
        const int block_size = 16;
        compute_alphas KUT{block_size, context};

        // allocate memory
        cl::Buffer device_intensities_nl     {context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, dimensions*height*width*sizeof(float), intensities_nl.data(),     NULL};
        cl::Buffer device_weighted_variances {context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, dimensions*height*width*sizeof(float), weighted_variances.data(), NULL};
        cl::Buffer device_alphas             {context, CL_MEM_READ_WRITE,                                  height*width*sizeof(float), NULL,                      NULL};

        KUT.run(cmd_queue, 
                device_intensities_nl,
                device_weighted_variances,
                device_alphas,
                height,
                width,
                dimensions,
                nlooks);

        cmd_queue.enqueueReadBuffer(device_alphas, CL_TRUE, 0, height*width*sizeof(float), alphas.data(), NULL, NULL);

        ASSERT_THAT(alphas, Pointwise(FloatNearPointwise(1e-4), desired_alphas));
}

TEST(compute_alphas, range_check) {

        // data setup
        const int height = 50;
        const int width = 40;
        const int dimensions = 2;
        const int nlooks = 1;

        std::vector<float> intensities_nl      (height*width*dimensions,  0.0);
        std::vector<float> weighted_variances  (height*width*dimensions,  2.0);
        std::vector<float> alphas              (height*width,            -1.0);
        const float alpha_lower = 0.0f;
        const float alpha_upper = 1.0f;

        static std::default_random_engine rand_eng{};
        static std::exponential_distribution<float> dist_intensities_nl(2.0);
        static std::gamma_distribution<float>       dist_weighted_variances(1.0, 5.0);

        for(int i = 0; i<dimensions*height*width; i++) {
            intensities_nl     [i] = dist_intensities_nl(rand_eng);
            weighted_variances [i] = dist_weighted_variances(rand_eng);
        }


        // opencl setup
        auto cl_devs = get_platform_devs(0);
        cl::Context context{cl_devs};
        cl::CommandQueue cmd_queue{context};

        // kernel setup
        const int block_size = 16;
        compute_alphas KUT{block_size, context};

        // allocate memory
        cl::Buffer device_intensities_nl     {context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, dimensions*height*width*sizeof(float), intensities_nl.data(),     NULL};
        cl::Buffer device_weighted_variances {context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, dimensions*height*width*sizeof(float), weighted_variances.data(), NULL};
        cl::Buffer device_alphas             {context, CL_MEM_READ_WRITE,                                  height*width*sizeof(float), NULL,                      NULL};

        KUT.run(cmd_queue, 
                device_intensities_nl,
                device_weighted_variances,
                device_alphas,
                height,
                width,
                dimensions,
                nlooks);

        cmd_queue.enqueueReadBuffer(device_alphas, CL_TRUE, 0, height*width*sizeof(float), alphas.data(), NULL, NULL);

        ASSERT_THAT(alphas, Each(IsBetween(alpha_lower, alpha_upper)));
}
