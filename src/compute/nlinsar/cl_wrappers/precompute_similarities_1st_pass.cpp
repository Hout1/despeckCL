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

#include "precompute_similarities_1st_pass.h"
#include <iostream>

constexpr const char* nlinsar::precompute_similarities_1st_pass::routine_name;
constexpr const char* nlinsar::precompute_similarities_1st_pass::kernel_source;

void nlinsar::precompute_similarities_1st_pass::run(cl::CommandQueue cmd_queue,
                                                    cl::Buffer device_a1,
                                                    cl::Buffer device_a2,
                                                    cl::Buffer device_dp,
                                                    cl::Buffer device_ref_filt,
                                                    cl::Buffer device_phi_filt,
                                                    cl::Buffer device_coh_filt,
                                                    const int height_overlap,
                                                    const int width_overlap,
                                                    const int search_window_size,
                                                    cl::Buffer device_similarities,
                                                    cl::Buffer device_kullback_leiblers)
{
    const int height_sim = height_overlap - search_window_size + 1;
    const int width_sim  = width_overlap  - search_window_size + 1;

    const int border_max = (search_window_size - 1) / 2;

    cl::NDRange global_size {(size_t) block_size*((height_sim-1)/block_size+1), (size_t) block_size*((width_sim-1)/block_size+1)};
    cl::NDRange local_size  {block_size, block_size};

    kernel.setArg(0,  device_a1);
    kernel.setArg(1,  device_a2);
    kernel.setArg(2,  device_dp);
    kernel.setArg(3,  device_ref_filt);
    kernel.setArg(4,  device_phi_filt);
    kernel.setArg(5,  device_coh_filt);
    kernel.setArg(6,  device_similarities);
    kernel.setArg(7,  device_kullback_leiblers);
    kernel.setArg(8,  height_overlap);
    kernel.setArg(9,  width_overlap);
    kernel.setArg(10, border_max);
    kernel.setArg(11, search_window_size);

    cmd_queue.enqueueNDRangeKernel(kernel, cl::NullRange, global_size, local_size, NULL, NULL);
}
