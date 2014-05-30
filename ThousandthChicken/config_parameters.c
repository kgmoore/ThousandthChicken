#include "config_parameters.h"

void default_config_values(type_parameters *param) {
	param->param_tile_w = -1;
	param->param_tile_h = -1;
	param->param_tile_comp_dlvls = 4;
	param->param_cblk_exp_w = 6;
	param->param_cblk_exp_h = 6;
	param->param_wavelet_type = 1;
	param->param_use_mct = 0;
	param->param_device = 0;
	param->param_target_size = 0;
	param->param_bp = 0.0;
	param->param_use_part2_mct = 0;
	param->param_mct_compression_method = 0;
	param->param_mct_klt_iterations = 10000;
	param->param_mct_klt_err = 1.0e-7;
	param->param_mct_klt_border_eigenvalue = 0.000001;
}