{
	.name		= "hsa_set_debug",
	.args_type	= "status:b",
	.params 	= "[on/off]\n",
	.help		= "\toff, disable debug mask\n"
				  "\ton, enable debug mask\n"
				  "\tdefault is off\n",
	.mhandler.cmd = hsa_set_debug,
},

{
	.name		= "hsa_set_debug_mask",
	.args_type	= "gidx:i,gidy:i,gidz:i,"
				  "tidx:i,tidy:i,tidz:i",
	.params 	= "[work-group ID x] [work-group ID y] [work-group ID z] "
				  "[work-item ID x] [work-item ID y] [work-item ID z]\n",
	.help		= "\tset work-group ID and work-item ID to mask other debug message\n"
				  "\tdefault is 0 0 0 0 0 0\n",
	.mhandler.cmd = hsa_set_debug_mask,
},

{
	.name		= "hsa_get_debug_info",
	.args_type	= "",
	.params 	= "(no parameters)\n",
	.help		= "\tshow the current debug setting\n",
	.mhandler.cmd = hsa_get_debug_info,
},

{
	.name		= "hsa_print_profile",
	.args_type	= "status:b",
	.params 	= "[on/off]\n",
	.help		= "\tprint HSA Component profile counter or not\n"
				  "\tdefault is on\n",
	.mhandler.cmd = hsa_print_profile,
},

{
	.name		= "hsa_reset_profile",
	.args_type	= "",
	.params 	= "(no parameters)\n",
	.help		= "\treset HSA Component profile counter\n",
	.mhandler.cmd = hsa_reset_profile,
},

{
	.name		= "hsa_get_profile_info",
	.args_type	= "",
	.params 	= "(no parameters)\n",
	.help		= "\tshow HSA Component profile counter\n",
	.mhandler.cmd = hsa_get_profile_info,
},
