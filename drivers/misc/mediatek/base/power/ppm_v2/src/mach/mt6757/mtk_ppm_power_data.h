/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "mtk_ppm_platform.h"

#ifdef PPM_TURBO_CORE_SUPPORT
static struct ppm_pwr_idx_ref_tbl cpu_pwr_idx_ref_tbl_FY_v2[] = {
	    [0] = {
		   {138, 124, 114, 103, 93, 82, 75, 68, 56, 46, 35, 27, 21, 16,
		     13, 8},
		   {124, 111, 102, 92, 83, 73, 66, 60, 49, 40, 30, 24, 20, 16,
		     13, 8},
		   {138, 124, 114, 103, 93, 82, 75, 68, 56, 46, 35, 27, 21, 16,
		     13, 8},
		    {157, 141, 131, 119, 109, 97, 89, 83, 70, 58, 47, 38, 31,
		     26, 24, 18},
		    {175, 159, 147, 135, 124, 112, 104, 97, 83, 71, 60, 51, 47,
		     43, 41, 36},
		    {157, 141, 131, 119, 109, 97, 89, 83, 70, 58, 47, 38, 31,
		     26, 24, 18},
		    {55, 50, 47, 44, 40, 37, 35, 33, 29, 26, 23, 21, 19, 18, 17,
		     16},
		    {93,
		    86, 81, 76, 71, 66, 62, 59, 53, 47, 42, 39, 38, 37, 36, 35},
		    {55,
		    50, 47, 44, 40, 37, 35, 33, 29, 26, 23, 21, 19, 18, 17, 16},
		},
	    [1] = {
		    {250, 227, 209, 192, 177, 156, 143, 131, 109,
		     88, 69, 51, 41, 33, 24, 20},
		    {225, 203, 187, 172, 157, 139, 126, 115,
		     96, 77, 59, 46, 40, 33, 24, 20},
		    {250, 227, 209, 192, 177, 156, 143, 131, 109,
		     88, 69, 51, 41, 33, 24, 20},
		    {320, 294, 273, 254, 236, 212, 197, 183, 158, 134, 111,
		     91, 79, 69, 60, 56},
		    {422, 389, 364, 340, 317, 288, 271, 254, 223, 194, 167, 146,
		     140, 134, 124, 120},
		    {320, 294, 273, 254, 236, 212, 197, 183, 158, 134, 111,
		     91, 79, 69, 60, 56},
		    {135, 126, 119, 112, 105,
		     97, 92, 88, 79, 71, 63, 56, 52, 49, 47, 46},
		    {245, 230, 219, 207, 197, 183, 175, 167, 153, 139, 125, 116,
		     114, 113, 110, 109},
		    {135, 126, 119, 112, 105,
		     97, 92, 88, 79, 71, 63, 56, 52, 49, 47, 46},
		},
};

static struct ppm_pwr_idx_ref_tbl cpu_pwr_idx_ref_tbl_FY_v3[] = {
	    [0] = {
		    {155, 141, 126, 113, 104, 85, 75, 63, 52, 42, 33, 24, 19,
		     15, 12, 8},
		    {139, 125, 112, 100, 91, 75, 65, 54, 45, 35, 27, 21, 18, 14,
		     12, 8},
		    {155, 141, 126, 113, 104, 85, 75, 63, 52, 42, 33, 24, 19,
		     15, 12, 8},
		    {167, 152, 137, 123, 113, 94, 84, 71, 60, 49, 39, 30, 25,
		     20, 18, 14},
		    {208, 190, 170, 155, 145, 123, 111, 97, 84, 72, 61, 53, 50,
		     46, 44, 40},
		    {167, 152, 137, 123, 113, 94, 84, 71, 60, 49, 39, 30, 25,
		     20, 18, 14},
		    {51, 46, 41, 38, 35, 30, 27, 24, 21, 19, 16, 14, 13, 12, 12,
		     11},
		    {162, 152, 140, 130, 123, 109, 101,
		     92, 83, 75, 67, 61, 60, 60, 59, 58},
		    {51,
		    46, 41, 38, 35, 30, 27, 24, 21, 19, 16, 14, 13, 12, 12, 11},
		},
	    [1] = {
		    {250, 225, 201, 184, 171, 147, 130, 110,
		     92, 74, 58, 43, 35, 27, 21, 18},
		    {224, 201, 178, 163, 150, 128, 113,
		     95, 79, 63, 49, 37, 32, 27, 21, 18},
		    {250, 225, 201, 184, 171, 147, 130, 110,
		     92, 74, 58, 43, 35, 27, 21, 18},
		    {320, 291, 261, 242, 227, 198, 179, 155, 135, 114,
		     95, 77, 68, 59, 52, 49},
		    {742, 686, 628, 592, 564, 513, 480, 441, 407, 374, 343, 319,
		     314, 309, 302, 300},
		    {320, 291, 261, 242, 227, 198, 179, 155, 135, 114,
		     95, 77, 68, 59, 52, 49},
		    {121, 111, 101,
		     94, 90, 81, 75, 68, 62, 56, 51, 46, 43, 40, 39, 38},
		    {375, 348, 319, 302, 289, 262, 246, 227, 209, 191, 174, 162,
		     161, 160, 159, 158},
		    {121, 111, 101,
		     94, 90, 81, 75, 68, 62, 56, 51, 46, 43, 40, 39, 38},
		},
};

static int cpu_perf_idx_ref_tbl_FY_v2[NR_PPM_CLUSTERS][DVFS_OPP_NUM] = {
	{704, 671, 648, 620, 592, 559, 531, 509, 458, 408, 347, 291, 241, 190,
	162, 95},
	{1006, 972, 944, 916, 888, 844, 805, 771, 704, 626, 537, 447, 386, 324,
	 229, 190},
};

static int cpu_perf_idx_ref_tbl_FY_v3[NR_PPM_CLUSTERS][DVFS_OPP_NUM] = {
	{727, 715, 704, 671, 648, 592, 559, 509, 458, 403, 347, 285, 241, 196,
	 168, 107},
	{1028, 1006, 983, 961, 939, 894, 849, 782, 715, 632, 542, 447, 391, 330,
	 246, 213},
};

static struct ppm_pwr_idx_ref_tbl cpu_pwr_idx_ref_tbl_SB_v2[] = {
	    [0] = {
		    {179, 158, 133, 114, 97, 82, 75, 68, 56, 46, 35, 27, 21, 16,
		     13, 8},
		    {161, 142, 119, 101, 85, 72, 65, 60, 49, 39, 30, 24, 20, 16,
		     13, 8},
		    {179, 158, 133, 114, 97, 82, 75, 68, 56, 46, 35, 27, 21, 16,
		     13, 8},
		    {200, 178, 151, 131, 113, 97, 89, 83, 70, 58, 47, 38, 31,
		     26, 24, 18},
		    {219, 196, 168, 146, 128, 111, 103, 96, 83, 71, 59, 51, 47,
		     43, 41, 36},
		    {200, 178, 151, 131, 113, 97, 89, 83, 70, 58, 47, 38, 31,
		     26, 24, 18},
		    {68, 62, 53, 47, 42, 37, 35, 33, 29, 26, 23, 21, 19, 18, 17,
		     16},
		    {110, 102,
		     90, 80, 72, 65, 62, 59, 53, 47, 42, 39, 38, 37, 36, 35},
		    {68,
		    62, 53, 47, 42, 37, 35, 33, 29, 26, 23, 21, 19, 18, 17, 16},
		},
	    [1] = {
		    {319, 285, 246, 212, 183, 156, 143, 131, 109,
		     88, 69, 51, 41, 33, 24, 20},
		    {287, 255, 219, 188, 161, 137, 125, 114,
		     95, 76, 58, 46, 40, 33, 24, 20},
		    {319, 285, 246, 212, 183, 156, 143, 131, 109,
		     88, 69, 51, 41, 33, 24, 20},
		    {398, 360, 315, 277, 243, 212, 197, 183, 158, 134, 111,
		     91, 79, 69, 60, 56},
		    {513, 468, 413, 365, 323, 286, 268, 251, 221, 192, 165, 146,
		     140, 134, 124, 120},
		    {398, 360, 315, 277, 243, 212, 197, 183, 158, 134, 111,
		     91, 79, 69, 60, 56},
		    {162, 149, 133, 120, 108,
		     97, 92, 88, 79, 71, 63, 56, 52, 49, 47, 46},
		    {285, 266, 241, 219, 199, 182, 174, 166, 152, 138, 124, 116,
		     114, 113, 110, 109},
		    {162, 149, 133, 120, 108,
		     97, 92, 88, 79, 71, 63, 56, 52, 49, 47, 46},
		},
};

static struct ppm_pwr_idx_ref_tbl cpu_pwr_idx_ref_tbl_SB_v3[] = {
	    [0] = {
		    {155, 141, 126, 113, 104, 85, 75, 63, 52, 42, 33, 24, 19,
		     15, 12, 8},
		    {139, 125, 112, 100, 91, 75, 65, 54, 45, 35, 27, 21, 18, 14,
		     12, 8},
		    {155, 141, 126, 113, 104, 85, 75, 63, 52, 42, 33, 24, 19,
		     15, 12, 8},
		    {167, 152, 137, 123, 113, 94, 84, 71, 60, 49, 39, 30, 25,
		     20, 18, 14},
		    {208, 190, 170, 155, 145, 123, 111, 97, 84, 72, 61, 53, 50,
		     46, 44, 40},
		    {167, 152, 137, 123, 113, 94, 84, 71, 60, 49, 39, 30, 25,
		     20, 18, 14},
		    {51, 46, 41, 38, 35, 30, 27, 24, 21, 19, 16, 14, 13, 12, 12,
		     11},
		    {162, 152, 140, 130, 123, 109, 101,
		     92, 83, 75, 67, 61, 60, 60, 59, 58},
		    {51,
		    46, 41, 38, 35, 30, 27, 24, 21, 19, 16, 14, 13, 12, 12, 11},
		},
	    [1] = {
		    {250, 225, 201, 184, 171, 147, 130, 110,
		     92, 74, 58, 43, 35, 27, 21, 18},
		    {224, 201, 178, 163, 150, 128, 113,
		     95, 79, 63, 49, 37, 32, 27, 21, 18},
		    {250, 225, 201, 184, 171, 147, 130, 110,
		     92, 74, 58, 43, 35, 27, 21, 18},
		    {320, 291, 261, 242, 227, 198, 179, 155, 135, 114,
		     95, 77, 68, 59, 52, 49},
		    {742, 686, 628, 592, 564, 513, 480, 441, 407, 374, 343, 319,
		     314, 309, 302, 300},
		    {320, 291, 261, 242, 227, 198, 179, 155, 135, 114,
		     95, 77, 68, 59, 52, 49},
		    {121, 111, 101,
		     94, 90, 81, 75, 68, 62, 56, 51, 46, 43, 40, 39, 38},
		    {375, 348, 319, 302, 289, 262, 246, 227, 209, 191, 174, 162,
		     161, 160, 159, 158},
		    {121, 111, 101,
		     94, 90, 81, 75, 68, 62, 56, 51, 46, 43, 40, 39, 38},
		},
};

static int cpu_perf_idx_ref_tbl_SB_v2[NR_PPM_CLUSTERS][DVFS_OPP_NUM] = {
	{788, 738, 693, 648, 604, 559, 531, 509, 458, 408, 347, 291, 241, 190,
	  162, 95},
	{1106, 1050, 1000, 950, 900, 844, 805, 771, 704, 626, 537, 447, 386,
	  324, 229, 190},
};

static int cpu_perf_idx_ref_tbl_SB_v3[NR_PPM_CLUSTERS][DVFS_OPP_NUM] = {
	{727, 715, 704, 671, 648, 592, 559, 509, 458, 403, 347, 285, 241, 196,
	  168, 107},
	{1028, 1006, 983, 961, 939, 894, 849, 782, 715, 632, 542, 447, 391, 330,
	 246, 213},
};
#else
static struct ppm_pwr_idx_ref_tbl cpu_pwr_idx_ref_tbl_FY[] = {
	    [0] = {
		    {138, 124, 114, 103, 93, 82, 75, 68, 56, 46, 35, 27, 21, 16,
		     13, 8},
		    {124, 111, 102, 92, 83, 73, 66, 60, 49, 40, 30, 24, 20, 16,
		     13, 8},
		    {138, 124, 114, 103, 93, 82, 75, 68, 56, 46, 35, 27, 21, 16,
		     13, 8},
		    {157, 141, 131, 119, 109, 97, 89, 83, 70, 58, 47, 38, 31,
		     26, 24, 18},
		    {175, 159, 147, 135, 124, 112, 104, 97, 83, 71, 60, 51, 47,
		     43, 41, 36},
		    {157, 141, 131, 119, 109, 97, 89, 83, 70, 58, 47, 38, 31,
		     26, 24, 18},
		    {55, 50, 47, 44, 40, 37, 35, 33, 29, 26, 23, 21, 19, 18, 17,
		     16},
		    {93,
		    86, 81, 76, 71, 66, 62, 59, 53, 47, 42, 39, 38, 37, 36, 35},
		    {55,
		    50, 47, 44, 40, 37, 35, 33, 29, 26, 23, 21, 19, 18, 17, 16},
		},
	    [1] = {
		    {250, 227, 209, 192, 177, 156, 143, 131, 109,
		     88, 69, 51, 41, 33, 24, 20},
		    {225, 203, 187, 172, 157, 139, 126, 115,
		     96, 77, 59, 46, 40, 33, 24, 20},
		    {250, 227, 209, 192, 177, 156, 143, 131, 109,
		     88, 69, 51, 41, 33, 24, 20},
		    {320, 294, 273, 254, 236, 212, 197, 183, 158, 134, 111,
		     91, 79, 69, 60, 56},
		    {422, 389, 364, 340, 317, 288, 271, 254, 223, 194, 167, 146,
		     140, 134, 124, 120},
		    {320, 294, 273, 254, 236, 212, 197, 183, 158, 134, 111,
		     91, 79, 69, 60, 56},
		    {135, 126, 119, 112, 105,
		     97, 92, 88, 79, 71, 63, 56, 52, 49, 47, 46},
		    {245, 230, 219, 207, 197, 183, 175, 167, 153, 139, 125, 116,
		     114, 113, 110, 109},
		    {135, 126, 119, 112, 105,
		     97, 92, 88, 79, 71, 63, 56, 52, 49, 47, 46},
		},
};

static int cpu_perf_idx_ref_tbl_FY[NR_PPM_CLUSTERS][DVFS_OPP_NUM] = {
	{704, 671, 648, 620, 592, 559, 531, 509, 458, 408, 347, 291, 241, 190,
	  162, 95},
	{1006, 972, 944, 916, 888, 844, 805, 771, 704, 626, 537, 447, 386, 324,
	  229, 190},
};

static struct ppm_pwr_idx_ref_tbl cpu_pwr_idx_ref_tbl_SB[] = {
	    [0] = {
		    {179, 158, 133, 114, 97, 82, 75, 68, 56, 46, 35, 27, 21, 16,
		     13, 8},
		    {161, 142, 119, 101, 85, 72, 65, 60, 49, 39, 30, 24, 20, 16,
		     13, 8},
		    {179, 158, 133, 114, 97, 82, 75, 68, 56, 46, 35, 27, 21, 16,
		     13, 8},
		    {200, 178, 151, 131, 113, 97, 89, 83, 70, 58, 47, 38, 31,
		     26, 24, 18},
		    {219, 196, 168, 146, 128, 111, 103, 96, 83, 71, 59, 51, 47,
		     43, 41, 36},
		    {200, 178, 151, 131, 113, 97, 89, 83, 70, 58, 47, 38, 31,
		     26, 24, 18},
		    {68, 62, 53, 47, 42, 37, 35, 33, 29, 26, 23, 21, 19, 18, 17,
		     16},
		    {110, 102,
		     90, 80, 72, 65, 62, 59, 53, 47, 42, 39, 38, 37, 36, 35},
		    {68,
		    62, 53, 47, 42, 37, 35, 33, 29, 26, 23, 21, 19, 18, 17, 16},
		},
	    [1] = {
		    {319, 285, 246, 212, 183, 156, 143, 131, 109,
		     88, 69, 51, 41, 33, 24, 20},
		    {287, 255, 219, 188, 161, 137, 125, 114,
		     95, 76, 58, 46, 40, 33, 24, 20},
		    {319, 285, 246, 212, 183, 156, 143, 131, 109,
		     88, 69, 51, 41, 33, 24, 20},
		    {398, 360, 315, 277, 243, 212, 197, 183, 158, 134, 111,
		     91, 79, 69, 60, 56},
		    {513, 468, 413, 365, 323, 286, 268, 251, 221, 192, 165, 146,
		     140, 134, 124, 120},
		    {398, 360, 315, 277, 243, 212, 197, 183, 158, 134, 111,
		     91, 79, 69, 60, 56},
		    {162, 149, 133, 120, 108,
		     97, 92, 88, 79, 71, 63, 56, 52, 49, 47, 46},
		    {285, 266, 241, 219, 199, 182, 174, 166, 152, 138, 124, 116,
		     114, 113, 110, 109},
		    {162, 149, 133, 120, 108,
		     97, 92, 88, 79, 71, 63, 56, 52, 49, 47, 46},
		},
};

static int cpu_perf_idx_ref_tbl_SB[NR_PPM_CLUSTERS][DVFS_OPP_NUM] = {
	{788, 738, 693, 648, 604, 559, 531, 509, 458, 408, 347, 291, 241, 190,
	  162, 95},
	{1106, 1050, 1000, 950, 900, 844, 805, 771, 704, 626, 537, 447, 386,
	  324, 229, 190},
};
#endif
