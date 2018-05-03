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

#include "precompute_similarities_2nd_pass.h"

constexpr const char* nlinsar::precompute_similarities_2nd_pass::routine_name;
constexpr const char* nlinsar::precompute_similarities_2nd_pass::kernel_source;

void nlinsar::precompute_similarities_2nd_pass::run(cl::CommandQueue cmd_queue,
                                                    const int height_overlap,
                                                    const int width_overlap,
                                                    const int search_window_size,
                                                    cl::Buffer device_similarities,
                                                    cl::Buffer device_kullback_leiblers)
{
    const int height_sim = height_overlap - search_window_size + 1;
    const int width_sim  = width_overlap  - search_window_size + 1;

    const int border_max = (search_window_size - 1) / 2;

    // if the sub image is too small, all the values have already been computed in 
    // the first pass and the 2nd kerned does not have to be called.
    // For me this always resulted in a crash.
    if ( height_sim > border_max && width_sim > 2*border_max ) {

        cl::NDRange global_offset {(size_t) border_max, (size_t) border_max};
        cl::NDRange global_size   {(size_t) block_size*((height_sim-1-border_max)/block_size+1), (size_t) block_size*((width_sim-1-2*border_max)/block_size+1)};
        cl::NDRange local_size    {block_size, block_size};

        kernel.setArg( 0, device_similarities);
        kernel.setArg( 1, device_kullback_leiblers);
        kernel.setArg( 2, height_overlap);
        kernel.setArg( 3, width_overlap);
        kernel.setArg( 4, border_max);
        kernel.setArg( 5, search_window_size);

        cmd_queue.enqueueNDRangeKernel(kernel, global_offset, global_size, local_size, NULL, NULL);
    }
}
