#include "routines.h"

cl::Buffer nlsar::routines::get_pixel_similarities (cl::Context context,
                                                    cl::Buffer covmat_rescaled,
                                                    const int height_overlap,
                                                    const int width_overlap,
                                                    const int dimension,
                                                    const int search_window_size,
                                                    const int scale_size,
                                                    const int scale_size_max,
                                                    cl_wrappers& nl_routines)
{
    // dimension of the precomputed patch similarity values
    const int height_sim = height_overlap - search_window_size + 1;
    const int width_sim  = width_overlap  - search_window_size + 1;
    const int n_elem_sim = height_sim * width_sim;

    std::vector<cl::Device> devices;
    context.getInfo(CL_CONTEXT_DEVICES, &devices);
    cl::CommandQueue cmd_queue{context, devices[0]};

    const int n_elem_overlap = height_overlap * width_overlap;

    cl::Buffer covmat_spatial_avg        {context, CL_MEM_READ_WRITE, 2*dimension * dimension * n_elem_overlap             * sizeof(float), NULL, NULL};
    cl::Buffer device_pixel_similarities {context, CL_MEM_READ_WRITE, search_window_size * search_window_size * n_elem_sim * sizeof(float), NULL, NULL};

    LOG(DEBUG) << "covmat_spatial_avg";
    nl_routines.covmat_spatial_avg_routine.timed_run(cmd_queue,
                                                     covmat_rescaled,
                                                     covmat_spatial_avg,
                                                     dimension,
                                                     height_overlap,
                                                     width_overlap,
                                                     scale_size,
                                                     scale_size_max);

    LOG(DEBUG) << "covmat_pixel_similarities";
    nl_routines.compute_pixel_similarities_2x2_routine.timed_run(cmd_queue,
                                                                 covmat_spatial_avg,
                                                                 device_pixel_similarities,
                                                                 height_overlap,
                                                                 width_overlap,
                                                                 search_window_size);

    return device_pixel_similarities;
}

cl::Buffer nlsar::routines::get_weights (cl::Buffer& pixel_similarities,
                                         cl::Context context,
                                         const int height_sim,
                                         const int width_sim,
                                         const int search_window_size,
                                         const int patch_size,
                                         const int patch_size_max,
                                         stats& parameter_stats,
                                         cl::Buffer& lut_dissims2relidx,
                                         cl::Buffer& lut_chi2cdf_inv,
                                         cl_wrappers& nl_routines)
{
    std::vector<cl::Device> devices;
    context.getInfo(CL_CONTEXT_DEVICES, &devices);
    cl::CommandQueue cmd_queue{context, devices[0]};

    const int wsh = (search_window_size - 1)/2;

    // original dimension of the unpadded data
    const int height_ori = height_sim - patch_size + 1;
    const int width_ori  = width_sim  - patch_size + 1;
    const int n_elem_ori = height_ori * width_ori;

    cl::Buffer patch_similarities {context, CL_MEM_READ_WRITE, search_window_size * search_window_size * n_elem_ori * sizeof(float), NULL, NULL};
    cl::Buffer weights            {context, CL_MEM_READ_WRITE, search_window_size * search_window_size * n_elem_ori * sizeof(float), NULL, NULL};

    nl_routines.compute_patch_similarities_routine.timed_run(cmd_queue,
                                                             pixel_similarities,
                                                             patch_similarities,
                                                             height_sim,
                                                             width_sim,
                                                             search_window_size,
                                                             patch_size,
                                                             patch_size_max);

    nl_routines.compute_weights_routine.timed_run(cmd_queue,
                                                  patch_similarities,
                                                  weights,
                                                  height_ori,
                                                  width_ori,
                                                  search_window_size,
                                                  patch_size,
                                                  lut_dissims2relidx,
                                                  lut_chi2cdf_inv,
                                                  parameter_stats.lut_size,
                                                  parameter_stats.dissims_min,
                                                  parameter_stats.dissims_max);

    // set weight for self similarity
    const cl_int self_weight = 1;
    cmd_queue.enqueueFillBuffer(weights,
                                self_weight,
                                height_ori * width_ori * (search_window_size * wsh + wsh) * sizeof(float), //offset
                                height_ori * width_ori * sizeof(float),
                                NULL, NULL);

    return weights;
}

cl::Buffer nlsar::routines::get_enls_nobias (cl::Context context,
                                             cl::Buffer& device_weights,
                                             cl::Buffer& covmat_ori,
                                             const int height_ori,
                                             const int width_ori,
                                             const int search_window_size,
                                             const int patch_size,
                                             const int scale_size_max,
                                             const int nlooks,
                                             const int dimension,
                                             cl_wrappers& nl_routines)
{
    std::vector<cl::Device> devices;
    context.getInfo(CL_CONTEXT_DEVICES, &devices);
    cl::CommandQueue cmd_queue{context, devices[0]};

    const int n_elem_ori = height_ori * width_ori;

    cl::Buffer device_enl                {context, CL_MEM_READ_WRITE, n_elem_ori * sizeof(float), NULL, NULL};
    cl::Buffer device_intensities_nl     {context, CL_MEM_READ_WRITE, n_elem_ori * sizeof(float), NULL, NULL};
    cl::Buffer device_weighted_variances {context, CL_MEM_READ_WRITE, n_elem_ori * sizeof(float), NULL, NULL};
    cl::Buffer device_wsums              {context, CL_MEM_READ_WRITE, n_elem_ori * sizeof(float), NULL, NULL};
    cl::Buffer device_alphas             {context, CL_MEM_READ_WRITE, n_elem_ori * sizeof(float), NULL, NULL};
    cl::Buffer device_enls_nobias        {context, CL_MEM_READ_WRITE, n_elem_ori * sizeof(float), NULL, NULL};

    nl_routines.compute_number_of_looks_routine.timed_run(cmd_queue,
                                                          device_weights,
                                                          device_enl,
                                                          height_ori,
                                                          width_ori,
                                                          search_window_size);

    nl_routines.compute_nl_statistics_routine.run(cmd_queue, 
                                                  covmat_ori,
                                                  device_weights,
                                                  device_intensities_nl,
                                                  device_weighted_variances,
                                                  device_wsums,
                                                  height_ori,
                                                  width_ori,
                                                  search_window_size,
                                                  patch_size,
                                                  scale_size_max);

    nl_routines.compute_alphas_routine.run(cmd_queue, 
                                           device_intensities_nl,
                                           device_weighted_variances,
                                           device_alphas,
                                           height_ori,
                                           width_ori,
                                           dimension,
                                           nlooks);

    nl_routines.compute_enls_nobias_routine.run(cmd_queue, 
                                                device_enl,
                                                device_alphas,
                                                device_wsums,
                                                device_enls_nobias,
                                                height_ori,
                                                width_ori);

    return device_enls_nobias;
}