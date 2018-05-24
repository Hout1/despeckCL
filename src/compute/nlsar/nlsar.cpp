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

#include "despeckcl.h"

#include <chrono>
#include <vector>
#include <string>
#include <numeric>

#include "cl_wrappers.h"
#include "insar_data.h"
#include "tile_iterator.h"
#include "tile_size.h"
#include "nlsar_filter_sub_image.h"
#include "sub_images.h"
#include "stats.h"
#include "get_stats.h"
#include "clcfg.h"
#include "logging.h"
#include "best_params.h"
#include "timings.h"

int despeckcl::nlsar(float* ampl_master,
                     float* ampl_slave,
                     float* phase,
                     float* ref_filt,
                     float* phase_filt,
                     float* coh_filt,
                     const int height,
                     const int width,
                     const int search_window_size,
                     const std::vector<int> patch_sizes,
                     const std::vector<int> scale_sizes,
                     std::map<nlsar::params, nlsar::stats> nlsar_stats,
                     std::vector<std::string> enabled_log_levels)
{
    timings::map tm;

    const int patch_size_max = *std::max_element(patch_sizes.begin(), patch_sizes.end());
    const int scale_size_max = *std::max_element(scale_sizes.begin(), scale_sizes.end());

    // FIXME
    const int dimension = 2;
    // overlap consists of:
    // - (patch_size_max - 1)/2 + (search_window_size - 1)/2 for similarities
    // - (window_width - 1)/2 for spatial averaging of covariance matrices
    const int overlap = (patch_size_max - 1)/2 + (search_window_size - 1)/2 + (scale_size_max - 1)/2;

    logging_setup(enabled_log_levels);

    LOG(INFO) << "filter parameters";
    LOG(INFO) << "search window size: " << search_window_size;
    auto intvec2string = [] (std::vector<int> ints) { return std::accumulate(ints.begin(), ints.end(), (std::string)"",
                                                                             [] (std::string acc, int i) {return acc + std::to_string(i) + ", ";});
                                                    };

    LOG(INFO) << "patch_sizes: " << intvec2string(patch_sizes);
    LOG(INFO) << "scale_sizes: " << intvec2string(scale_sizes);
    LOG(INFO) << "overlap: " << overlap;

    LOG(INFO) << "data dimensions";
    LOG(INFO) << "height: " << height;
    LOG(INFO) << "width: " << width;

    // legacy opencl setup
    cl::Context context = opencl_setup();

    std::pair<int, int> tile_dims = nlsar::tile_size(context, height, width, dimension, search_window_size, patch_sizes, scale_sizes);

    LOG(INFO) << "tile height: " << tile_dims.first;
    LOG(INFO) << "tile width: " << tile_dims.second;

    // new build kernel interface
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> duration = end-start;
    start = std::chrono::system_clock::now();
    VLOG(0) << "Building kernels";
    nlsar::cl_wrappers nlsar_cl_wrappers (context, search_window_size, dimension);
    end = std::chrono::system_clock::now();
    duration = end-start;
    VLOG(0) << "Time it took to build all kernels: " << duration.count() << "secs";

    // prepare data
    insar_data total_image{ampl_master, ampl_slave, phase,
                           ref_filt, phase_filt, coh_filt,
                           height, width};

    // filtering
    start = std::chrono::system_clock::now();
    LOG(INFO) << "starting filtering";
#pragma omp parallel shared(total_image)
{
#pragma omp master
    {
    for( auto t : tile_iterator(total_image.height, total_image.width, tile_dims.first, tile_dims.second, overlap, overlap) ) {
#pragma omp task firstprivate(t)
        {
        insar_data imgtile = tileget(total_image, t);
        try {
            timings::map tm_sub = filter_sub_image(context, nlsar_cl_wrappers, // opencl stuff
                                                   imgtile, // data
                                                   search_window_size,
                                                   patch_sizes,
                                                   scale_sizes,
                                                   dimension,
                                                   nlsar_stats);
#pragma omp critical
            tm = timings::join(tm, tm_sub);
        } catch (cl::Error error) {
            LOG(ERROR) << error.what() << "(" << error.err() << ")";
            LOG(ERROR) << "ERR while filtering sub image";
            std::terminate();
        }
        tile<2> rel_sub {t[0].get_sub(overlap, -overlap), t[1].get_sub(overlap, -overlap)};
        tile<2> tsub {slice{overlap, imgtile.height-overlap}, slice{overlap, imgtile.width-overlap}};
        insar_data imgtile_sub = tileget(imgtile, tsub);
        tilecpy(total_image, imgtile_sub, rel_sub);
        }
    }
#pragma omp taskwait
    }
}
    LOG(INFO) << "filtering done";
    timings::print(tm);
    end = std::chrono::system_clock::now();
    duration = end-start;
    std::cout << "filtering ran for " << duration.count() << " secs" << std::endl;

    memcpy(ref_filt, total_image.ref_filt, total_image.height*total_image.width*sizeof(float));
    memcpy(phase_filt, total_image.phi_filt, total_image.height*total_image.width*sizeof(float));
    memcpy(coh_filt, total_image.coh_filt, total_image.height*total_image.width*sizeof(float));

    return 0;
}
