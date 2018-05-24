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

#include <CL/cl.h>
#include <chrono>
#include <vector>
#include <string>

#include "cl_wrappers.h"
#include "insar_data.h"
#include "tile_iterator.h"
#include "tile_size.h"
#include "goldstein_filter_sub_image.h"
#include "sub_images.h"
#include "clcfg.h"
#include "logging.h"
#include "timings.h"

int despeckcl::goldstein(float* ampl_master,
                         float* ampl_slave,
                         float* phase,
                         float* ref_filt,
                         float* phase_filt,
                         float* coh_filt,
                         const unsigned int height,
                         const unsigned int width,
                         const unsigned int patch_size,
                         const unsigned int overlap,
                         const float alpha,
                         std::vector<std::string> enabled_log_levels)
{
    timings::map tm;

    logging_setup(enabled_log_levels);

    LOG(INFO) << "filter parameters";
    LOG(INFO) << "patch_size: " << patch_size;
    LOG(INFO) << "alpha: "      << alpha;
    LOG(INFO) << "overlap: "    << overlap;

    LOG(INFO) << "data dimensions";
    LOG(INFO) << "height: " << height;
    LOG(INFO) << "width: "  << width;

    // legacy opencl setup
    cl::Context context = opencl_setup();


    // get the maximum possible tile_size, but make sure that it is not larger (only by patch_size - 2*overlap)
    // than the height or width of the image
    const int sub_image_size = std::min(goldstein::round_up(std::max(height + 2*overlap,
                                                                     width  + 2*overlap), patch_size - 2*overlap),
                                        goldstein::tile_size(context, patch_size, overlap));


    // new build kernel interface
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds = end-start;
    start = std::chrono::system_clock::now();
    VLOG(0) << "Building kernels";
    goldstein::cl_wrappers goldstein_cl_wrappers (context, 16);
    end = std::chrono::system_clock::now();
    elapsed_seconds = end-start;
    VLOG(0) << "Time it took to build all kernels: " << elapsed_seconds.count() << "secs";

    // prepare data
    insar_data total_image{ampl_master, ampl_slave, phase,
                           ref_filt, phase_filt, coh_filt,
                           (int) height, (int) width};

    // filtering
    start = std::chrono::system_clock::now();
    LOG(INFO) << "starting filtering";
    for( auto t : tile_iterator(total_image.height, total_image.width, sub_image_size, sub_image_size, overlap, overlap) ) {
        insar_data imgtile = tileget(total_image, t);
        try {
            timings::map tm_sub = filter_sub_image(context,
                                                   goldstein_cl_wrappers, // opencl stuff
                                                   imgtile, // data
                                                   patch_size,
                                                   overlap,
                                                   alpha);
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
    LOG(INFO) << "filtering done";
    timings::print(tm);
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> duration = end-start;
    std::cout << "filtering ran for " << duration.count() << " secs" << std::endl;

    memcpy(ref_filt,    total_image.ref_filt, sizeof(float)*height*width);
    memcpy(phase_filt, total_image.phi_filt, sizeof(float)*height*width);
    memcpy(coh_filt,    total_image.coh_filt, sizeof(float)*height*width);

    return 0;
}
