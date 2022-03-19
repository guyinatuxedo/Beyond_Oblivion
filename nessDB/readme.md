
0x00:	Header (8 byte string)
0x08:	layout_version
0x0c:	last_nid	
0x14:	last_msn
0x1c:	root_nid
0x24:	blocksize
0x28:	blockoff


blkpairs

0x00: 	"blkpairs" string
0x08:	block_count


0x0c:	nid
0x14:	offset
0x24:	real_size
0x28:	skeleton_size
0x2c:	height




















Node Header

0x00:	8 byte string
0x08:	node->height
0x0c:	node->n children
0x10:	node_layout_version
0x14:	node->xsum
0x18:	header->base
