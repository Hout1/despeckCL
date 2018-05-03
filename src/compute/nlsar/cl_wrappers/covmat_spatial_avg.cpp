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

#include "covmat_spatial_avg.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <cmath>
#include <numeric>

constexpr const char* nlsar::covmat_spatial_avg::routine_name;
constexpr const char* nlsar::covmat_spatial_avg::kernel_source;

int nlsar::covmat_spatial_avg::get_output_block_size(const int scale_size)
{
    return block_size - scale_size + 1;
}

std::vector<float> nlsar::covmat_spatial_avg::gen_gauss(const int scale_size)
{
    std::vector<float> gauss;
    gauss.reserve(scale_size*scale_size);
    const int ssh = (scale_size - 1)/2;

    for(int x = -ssh; x <= ssh; x++) {
        for(int y = -ssh; y <= ssh; y++) {
            gauss.push_back(std::exp(-M_PI*(x*x + y*y)/std::pow(ssh+0.5f, 2.0f)));
        }
    }
    const float K = std::accumulate(gauss.begin(), gauss.end(), 0.0f);
    std::for_each(gauss.begin(), gauss.end(), [K] (float& el) { el = el/K; });

    return gauss;
}

void nlsar::covmat_spatial_avg::run(cl::CommandQueue cmd_queue,
                                    cl::Buffer covmat_in,
                                    cl::Buffer covmat_out,
                                    const int dimension,
                                    const int height_overlap,
                                    const int width_overlap,
                                    const int scale_size,
                                    const int scale_size_max)
{
    const int output_block_size = get_output_block_size(scale_size);
    std::vector<float> gauss {gen_gauss(scale_size)};

    cl::Buffer dev_gauss {context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, scale_size * scale_size * sizeof(float), gauss.data(), NULL};

    kernel.setArg(0, covmat_in);
    kernel.setArg(1, covmat_out);
    kernel.setArg(2, dimension);
    kernel.setArg(3, height_overlap);
    kernel.setArg(4, width_overlap);
    kernel.setArg(5, scale_size);
    kernel.setArg(6, scale_size_max);
    kernel.setArg(7, cl::Local(block_size*block_size*sizeof(float)));
    kernel.setArg(8, dev_gauss);

    cl::NDRange global_size {(size_t) block_size*( (height_overlap - 1)/output_block_size + 1), \
                             (size_t) block_size*( (width_overlap  - 1)/output_block_size + 1)};
    cl::NDRange local_size  {block_size, block_size};

    cmd_queue.enqueueNDRangeKernel(kernel, cl::NullRange, global_size, local_size, NULL, NULL);
}
