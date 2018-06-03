#include "nl_user.h"

unsigned int qdma_config_bar_max_addr = 0xB400;
struct xreg_info qdma_config_regs[] = {
	{"GLBL_RNGSZ",				0x204, 16, 0, 0, 0,},	
	{"GLBL_SCRATCH",			0x244, 0,  0, 0, 0,},	
	{"GLBL_ERR_STAT",			0x248, 0,  0, 0, 0,},	
	{"GLBL_ERR_MASK",			0x24C, 0,  0, 0, 0,},	
	{"GLBL_WB_ACC",				0x250, 0,  0, 0, 0,},	
	{"GLBL_DSC_ERR_STS",			0x254, 0,  0, 0, 0,},	
	{"GLBL_DSC_ERR_MSK",			0x258, 0,  0, 0, 0,},	
	{"GLBL_DSC_ERR_LOG",			0x25C, 0,  0, 0, 0,},	
	{"GLBL_TRQ_ERR_STS",			0x260, 0,  0, 0, 0,},	
	{"GLBL_TRQ_ERR_MSK",			0x264, 0,  0, 0, 0,},	
	{"GLBL_TRQ_ERR_LOG",			0x268, 0,  0, 0, 0,},	
	/* TODO: max 256, display 2 for now */
	{"TRQ_SEL_FMAP",			0x400, 2, 0, 0, 0,},	
	{"IND_CTXT_DATA",			0x804, 4, 0, 0, 0,},	
	{"IND_CTXT_MASK",			0x814, 4, 0, 0, 0,},	
	{"IND_CTXT_CMD",			0x824, 0, 0, 0, 0,},	
	{"IND_CAUSE",				0x828, 0, 0, 0, 0,},	
	{"IND_ENABLE",				0x82C, 0, 0, 0, 0,},	
	{"C2H_TIMER_CNT",			0xA00, 16, 0, 0, 0,},	
	{"C2H_CNT_THRESH",			0xA40, 16, 0, 0, 0,},	
	{"C2H_QID2VEC_MAP_QID",			0xA80, 0, 0, 0, 0,},	
	{"C2H_QID2VEC_MAP",			0xA84, 0, 0, 0, 0,},	
	{"C2H_STAT_S_AXIS_C2H_ACCEPTED", 	0xA88, 0, 0, 0, 0,},	
	{"C2H_STAT_S_AXIS_WRB_ACCEPTED",	0xA8C, 0, 0, 0, 0,},	
	{"C2H_STAT_DESC_RSP_PKT_ACCEPTED", 	0xA90, 0, 0, 0, 0,},	
	{"C2H_STAT_AXIS_PKG_CMP",	 	0xA94, 0, 0, 0, 0,},	
	{"C2H_STAT_DESC_RSP_ACCEPTED",	 	0xA98, 0, 0, 0, 0,},	
	{"C2H_STAT_DESC_RSP_CMP",	 	0xA9C, 0, 0, 0, 0,},	
	{"C2H_STAT_WRQ_OUT",	 		0xAA0, 0, 0, 0, 0,},	
	{"C2H_STAT_WPL_REN_ACCEPTED",		0xAA4, 0, 0, 0, 0,},	
	{"C2H_STAT_TOTAL_WRQ_LEN",	 	0xAA8, 0, 0, 0, 0,},	
	{"C2H_STAT_TOTAL_WPL_LEN", 		0xAAC, 0, 0, 0, 0,},	
	{"C2H_BUF_SZ",	 			0xAB0, 16, 0, 0, 0,},	
	{"C2H_ERR_STAT",	 		0xAF0, 0, 0, 0, 0,},	
	{"C2H_ERR_MASK",	 		0xAF4, 0, 0, 0, 0,},	
	{"C2H_FATAL_ERR_STAT",	 		0xAF8, 0, 0, 0, 0,},	
	{"C2H_FATAL_ERR_MASK",	 		0xAFC, 0, 0, 0, 0,},	
	{"C2H_FATAL_ERR_ENABLE", 		0xB00, 0, 0, 0, 0,},	
	{"C2H_ERR_INT", 			0xB04, 0, 0, 0, 0,},	
	{"C2H_PFCH_CFG", 			0xB08, 0, 0, 0, 0,},	
	{"C2H_INT_TIMER_TICK", 			0xB0C, 0, 0, 0, 0,},	
	{"C2H_STAT_DESC_RSP_DROP_ACCEPTED", 	0xB10, 0, 0, 0, 0,},	
	{"C2H_STAT_DESC_RSP_ERR_ACCEPTED", 	0xB14, 0, 0, 0, 0,},	
	{"C2H_STAT_DESC_REQ",		 	0xB18, 0, 0, 0, 0,},	
	{"C2H_STAT_DEBUG_DMA_ENG_0",	 	0xB1C, 0, 0, 0, 0,},	
	{"C2H_STAT_DEBUG_DMA_ENG_1", 		0xB20, 0, 0, 0, 0,},	
	{"C2H_STAT_DEBUG_DMA_ENG_2", 		0xB24, 0, 0, 0, 0,},	
	{"C2H_STAT_DEBUG_DMA_ENG_3",	 	0xB28, 0, 0, 0, 0,},	
	{"C2H_DBG_PFCH_ERR_CTXT", 		0xB2C, 0, 0, 0, 0,},	
	{"C2H_FIRST_ERR_QID",		 	0xB30, 0, 0, 0, 0,},	
	{"C2H_MM0_CONTROL",		 	0x1004, 0, 0, 0, 0,},	
	{"C2H_MM1_CONTROL",		 	0x1104, 0, 0, 0, 0,},	
	{"H2C_MM0_CONTROL",		 	0x1204, 0, 0, 0, 0,},	
	{"H2C_MM1_CONTROL",		 	0x1304, 0, 0, 0, 0,},	

	/* TODO: max 2K, display 64 for now */
	{"DMAP_SEL_INT_CIDX",		 	0x6400, 64, 0x10, 0, 0,},	
	{"DMAP_SEL_H2C_DSC_PIDX",	 	0x6404, 64, 0x10, 0, 0,},	
	{"DMAP_SEL_C2H_DSC_PIDX",	 	0x6408, 64, 0x10, 0, 0,},	
	{"DMAP_SEL_WRB_CIDX",		 	0x640C, 64, 0x10, 0, 0,},	

	{"", 0, 0, 0 }
};

unsigned int qdma_user_max_addr = 0x24;
/*
 * INTERNAL: for debug testing only
 */
struct xreg_info qdma_user_regs[] = {
	{"ST_C2H_QID",				0x0, 0, 0, 0, 0,},	
	{"ST_C2H_PKTLEN",			0x4, 0, 0, 0, 0,},	
	{"ST_C2H_CONTROL",			0x8, 0, 0, 0, 0,},	
	/*  ST_C2H_CONTROL:
	 *	[1] : start C2H
	 *	[2] : immediate data
	 *	[3] : every packet statrs with 00 instead of continuous data
	 *	      stream until # of packets is complete
	 *	[31]: gen_user_reset_n
	 */
	{"ST_H2C_CONTROL",			0xC, 0, 0, 0, 0,},	
	/*  ST_H2C_CONTROL:
	 *	[0] : clear match for H2C transfer
	 */
	{"ST_H2C_QID_MATCH",			0x10, 0, 0, 0, 0,},	
	{"ST_H2C_XFER_CNT",			0x14, 0, 0, 0, 0,},	
	{"ST_C2H_PKT_CNT",			0x20, 0, 0, 0, 0,},	
	{"ST_C2H_WRB_DATA",			0x30, 8, 0, 0, 0,},	
	{"ST_C2H_WRB_TYPE",			0x50, 0, 0, 0, 0,},	
	{"ST_SCRATCH_PAD",			0x60, 2, 0, 0, 0,},	
	{"", 0, 0, 0 }
};
