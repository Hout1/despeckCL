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

#include "tile_size.h"

#include <cmath>

#include "easylogging++.h"

int goldstein::round_down(const int num, const int multiple)
{
     int remainder = num % multiple;
     return num - remainder;
}

int goldstein::round_up(const int num, const int multiple)
{
     return round_down(num, multiple) + multiple;
}

int goldstein::tile_size(const std::vector<cl::Device>& devices,
                         const int patch_size,
                         const int overlap)
{
    const int multiple = patch_size - 2*overlap;

    cl::Device dev = devices[0];

    int long max_mem_alloc_size;
    dev.getInfo(CL_DEVICE_MAX_MEM_ALLOC_SIZE, &max_mem_alloc_size);
    VLOG(0) << "maximum memory allocation size = " << max_mem_alloc_size;

    /*
     * the following buffer will take up space
     * 1) raw data, i.e. amplitude of master and slave image and their interferometric phase 
     *    => factor of 3
     * 2) real and complex value of the interferogram
     *    => factor of 2
     * 3) real and complex value of the patched interferogram
     *    => factor of 2
     * which leads to total factor of 7 times a factor of 4 as floats are stored in the buffers.
     *
     */

    const int n_pixels = max_mem_alloc_size / (4*7);

    const int tile_size_fit         = std::sqrt(n_pixels);
    const int tile_size_fit_rounded = round_down(tile_size_fit, multiple);

    VLOG(0) << "tile_size_fit = "         << tile_size_fit;
    VLOG(0) << "tile_size_fit_rounded = " << tile_size_fit_rounded;

    const float safety_factor = 0.5;
    int safe_tile_size = 0;
    if (float(tile_size_fit_rounded)/tile_size_fit < safety_factor) {
        safe_tile_size = tile_size_fit_rounded;
    } else {
        safe_tile_size = tile_size_fit_rounded - multiple;
    }
    VLOG(0) << "safe_tile_size = " << safe_tile_size;

    return safe_tile_size;
}
