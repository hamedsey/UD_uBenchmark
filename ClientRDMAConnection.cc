/*
 * Written by Hamed Seyedroudbari (Arm Research Intern - Summer 2022)
 */
#ifndef _CRDMACONNECTION_
#define _CRDMACONNECTION_
#include <arpa/inet.h>
//#include <event2/event.h>
#include <sys/uio.h>
#include <unistd.h>

#include <string>

#include "ClientRDMAConnection.h"

#ifdef __linux__
#include <malloc.h>
#endif

int RDMAConnection::pp_get_port_info(struct ibv_context *context, int port,
		     struct ibv_port_attr *attr)
{
	return ibv_query_port(context, port, attr);
}

void RDMAConnection::wire_gid_to_gid(const char *wgid, union ibv_gid *gid)
{
	char tmp[9];
	__be32 v32;
	int i;
	uint32_t tmp_gid[4];

	for (tmp[8] = 0, i = 0; i < 4; ++i) {
		memcpy(tmp, wgid + i * 8, 8);
		sscanf(tmp, "%x", &v32);
		tmp_gid[i] = be32toh(v32);
	}
	memcpy(gid, tmp_gid, sizeof(*gid));
}

void RDMAConnection::gid_to_wire_gid(const union ibv_gid *gid, char wgid[])
{
	uint32_t tmp_gid[4];
	int i;

	memcpy(tmp_gid, gid, sizeof(tmp_gid));
	for (i = 0; i < 4; ++i)
		sprintf(&wgid[i * 8], "%08x", htobe32(tmp_gid[i]));
}

struct pingpong_context* RDMAConnection::pp_init_ctx(struct ibv_device *ib_dev, int rx_depth, int port, int use_event, int id)
{
	ctx = (pingpong_context*)malloc(sizeof(struct pingpong_context));
	memset(ctx, 0x00, sizeof(struct pingpong_context));
	if (!ctx)
		return NULL;

	ctx->send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
	ctx->rx_depth   = rx_depth;

	for (unsigned int j = 0; j<bufs_num; j++) {
		buf_recv[j] = (char*)memalign(page_size, 4096);
		if (!buf_recv[j]) {
			fprintf(stderr, "Couldn't allocate work buf.\n");
			goto clean_ctx;
		}
		memset(buf_recv[j], 0x7b, 4096);

		buf_send[j] = (char*)memalign(page_size, 4096);
		if (!buf_send[j]) {
			fprintf(stderr, "Couldn't allocate work buf.\n");
			goto clean_ctx;
		}
		memset(buf_send[j], 0x7b, 4096);

		int mean = 2000;
		uint mean_lower = mean % 0x100;
		uint mean_upper = mean / 0x100;
		buf_send[j][0] = mean_lower;
		buf_send[j][1] = mean_upper;
	}

	ctx->context = ibv_open_device(ib_dev);
	if (!ctx->context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		goto clean_buffer;
	}

	{
		struct ibv_port_attr port_info = {};

		if (ibv_query_port(ctx->context, port, &port_info)) {
			fprintf(stderr, "Unable to query port info for port %d\n", port);
			goto clean_device;
		}
	}

	if (use_event) {
		ctx->channel = ibv_create_comp_channel(ctx->context);
		if (!ctx->channel) {
			fprintf(stderr, "Couldn't create completion channel\n");
			goto clean_device;
		}
	} else
		ctx->channel = NULL;

	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		goto clean_comp_channel;
	}

	for (unsigned int j = 0; j<bufs_num; j++) {
		mr_recv[j] = ibv_reg_mr(ctx->pd, buf_recv[j], 4096, IBV_ACCESS_LOCAL_WRITE);
		if (!mr_recv[j]) {
			fprintf(stderr, "Couldn't register MR\n");
			goto clean_pd;
		}
		mr_send[j] = ibv_reg_mr(ctx->pd, buf_send[j], 4096, IBV_ACCESS_LOCAL_WRITE);
		if (!mr_send[j]) {
			fprintf(stderr, "Couldn't register MR\n");
			goto clean_pd;
		}
	}

	ctx->cq = ibv_create_cq(ctx->context, 2*rx_depth +1, NULL,
				ctx->channel, 0);
	if (!ctx->cq) {
		fprintf(stderr, "Couldn't create CQ\n");
		goto clean_mr;
	}

	{
		struct ibv_qp_attr attr;
		memset(&attr, 0, sizeof(attr));
		struct ibv_qp_init_attr init_attr;
		memset(&init_attr, 0, sizeof(init_attr));

		init_attr.send_cq = ctx->cq;
		init_attr.recv_cq = ctx->cq;
		init_attr.cap.max_send_wr  = rx_depth;
		init_attr.cap.max_recv_wr  = rx_depth;
		init_attr.cap.max_send_sge = 1;
		init_attr.cap.max_recv_sge = 1;
		init_attr.qp_type = IBV_QPT_UD;

		if(id == 0) {
			while(1)
			{
				ctx->qp = ibv_create_qp(ctx->pd, &init_attr);
				if (!ctx->qp)  {
					fprintf(stderr, "Couldn't create QP\n");
					goto clean_cq;
				}
				if(ctx->qp->qp_num %(16*12) == 0) break;
				else ibv_destroy_qp(ctx->qp);
			}
		}
		else {
			ctx->qp = ibv_create_qp(ctx->pd, &init_attr);
				if (!ctx->qp)  {
					fprintf(stderr, "Couldn't create QP\n");
					goto clean_cq;
				}
		}

		ibv_query_qp(ctx->qp, &attr, IBV_QP_CAP, &init_attr);
		if (init_attr.cap.max_inline_data >= (unsigned int) 1024) { //size
			ctx->send_flags |= IBV_SEND_INLINE;
		}
	}
	{
		struct ibv_qp_attr attr; 
		memset(&attr, 0, sizeof(attr));

		attr.qp_state = IBV_QPS_INIT;
		attr.pkey_index = 0;
		attr.port_num = port;
		attr.qkey = 0x11111111;

		if (ibv_modify_qp(ctx->qp, &attr,
				  IBV_QP_STATE              |
				  IBV_QP_PKEY_INDEX         |
				  IBV_QP_PORT               |
				  IBV_QP_QKEY)) {
			fprintf(stderr, "Failed to modify QP to INIT\n");
			goto clean_qp;
		}
	}
	return ctx;

clean_qp:
	ibv_destroy_qp(ctx->qp);

clean_cq:
	ibv_destroy_cq(ctx->cq);

clean_mr:
	for (unsigned int j = 0; j<bufs_num; j++) {
		ibv_dereg_mr(mr_recv[j]);
		ibv_dereg_mr(mr_send[j]);
	}

clean_pd:
	ibv_dealloc_pd(ctx->pd);

clean_comp_channel:
	if (ctx->channel)
		ibv_destroy_comp_channel(ctx->channel);

clean_device:
	ibv_close_device(ctx->context);

clean_buffer:
	for (unsigned int j = 0; j<bufs_num; j++) {
		free(buf_recv[j]);
		free(buf_send[j]);
	}

clean_ctx:
	free(ctx);

	return NULL;
}

int RDMAConnection::pp_post_recv(struct pingpong_context *ctx, int wr_id, bool sync)
{
	struct ibv_sge list;
	memset(&list, 0, sizeof(list));

	if(!sync) {
		list.addr = (uintptr_t)buf_recv[wr_id - bufs_num];
		list.lkey = mr_recv[wr_id- bufs_num]->lkey;
	}
	else {
		list.addr = (uintptr_t)buf_recv[wr_id - sync_bufs_num];
		list.lkey = mr_recv[wr_id- sync_bufs_num]->lkey;
	}

	list.length = 40 + size;
	struct ibv_recv_wr wr;
	memset(&wr, 0, sizeof(wr));

	wr.wr_id = wr_id;
	wr.sg_list = &list;
	wr.num_sge = 1;

	struct ibv_recv_wr *bad_wr;
	memset(&bad_wr, 0, sizeof(bad_wr));
	return ibv_post_recv(ctx->qp, &wr, &bad_wr);
}

int RDMAConnection::pp_close_ctx(struct pingpong_context *ctx)
{
	if (ibv_destroy_qp(ctx->qp)) {
		fprintf(stderr, "Couldn't destroy QP\n");
		return 1;
	}

	
	if (ibv_destroy_cq(ctx->cq)) {
		fprintf(stderr, "Couldn't destroy CQ\n");
		return 1;
	}
	
	for (unsigned int j = 0; j<bufs_num; j++) {
		if (ibv_dereg_mr(mr_recv[j])) {
			fprintf(stderr, "Couldn't deregister MR\n");
			return 1;
		}
		if (ibv_dereg_mr(mr_send[j])) {
			fprintf(stderr, "Couldn't deregister MR\n");
			return 1;
		}
	}

	if (ibv_destroy_ah(ctx->ah)) {
		fprintf(stderr, "Couldn't destroy AH\n");
		return 1;
	}

	if (ibv_dealloc_pd(ctx->pd)) {
		fprintf(stderr, "Couldn't deallocate PD\n");
		return 1;
	}

	if (ctx->channel) {
		if (ibv_destroy_comp_channel(ctx->channel)) {
			fprintf(stderr, "Couldn't destroy completion channel\n");
			return 1;
		}
	}

	if (ibv_close_device(ctx->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}

	for (unsigned int j = 0; j<bufs_num; j++) {
		free(buf_recv[j]);
		free(buf_send[j]);
	}

	free(ctx);

	return 0;
}


struct pingpong_dest* RDMAConnection::pp_client_exch_dest(char *servername, int server_qpn0,/*int port,*/ const struct pingpong_dest *my_dest)
{
	rem_dest = (struct pingpong_dest*)malloc(sizeof *rem_dest);
	if (!rem_dest)
		return NULL;

	rem_dest->qpn = server_qpn0;
	rem_dest->lid = 0;
	rem_dest->psn = 0;
	rem_dest->gid = my_dest->gid;

	char * pch;
	pch = strtok (servername," ,.-");
	int i = 0;
	while (pch != NULL)
	{
		rem_dest->gid.raw[12+i] = strtol(pch, NULL, 10);
		i++;
		pch = strtok (NULL, ".");
	}

	rem_dest->gid.global.subnet_prefix = 0;
	return rem_dest;
}

int RDMAConnection::pp_connect_ctx(struct pingpong_context *ctx, int port, int my_psn, int sl, struct pingpong_dest *dest, int sgid_idx)
{
	struct ibv_ah_attr ah_attr;

	memset(&ah_attr, 0, sizeof(ah_attr));

	ah_attr.is_global = 0;
	ah_attr.dlid = dest->lid;
	ah_attr.sl = sl;
	ah_attr.src_path_bits = 0;
	ah_attr.port_num = port;

	struct ibv_qp_attr attr;
	memset(&attr, 0, sizeof(attr));
	attr.qp_state = IBV_QPS_RTR;

	if (ibv_modify_qp(ctx->qp, &attr, IBV_QP_STATE)) {
		fprintf(stderr, "Failed to modify QP to RTR\n");
		return 1;
	}

	attr.qp_state	= IBV_QPS_RTS;
	attr.sq_psn	    = my_psn;

	if (ibv_modify_qp(ctx->qp, &attr,
			  IBV_QP_STATE              |
			  IBV_QP_SQ_PSN)) {
		fprintf(stderr, "Failed to modify QP to RTS\n");
		return 1;
	}

	if (dest->gid.global.interface_id) {
		ah_attr.is_global = 1;
		ah_attr.grh.hop_limit = 100;
		ah_attr.grh.dgid = dest->gid;
		ah_attr.grh.sgid_index = sgid_idx;
	}

	ctx->ah = ibv_create_ah(ctx->pd, &ah_attr);
	if (!ctx->ah) {
		fprintf(stderr, "Failed to create AH\n");
		return 1;
	}

	return 0;
}

int RDMAConnection::pp_post_send(struct pingpong_context *ctx, uint32_t qpn, unsigned int length, int wr_id)
{
	struct ibv_sge list;

	list.addr   = (uintptr_t)buf_send[wr_id];
	list.length = length;
	list.lkey	= mr_send[wr_id]->lkey;

	struct ibv_send_wr wr; 
	memset(&wr, 0, sizeof(wr));

	wr.wr_id	  = wr_id;
	wr.sg_list    = &list;
	wr.num_sge    = 1;
	wr.opcode     = IBV_WR_SEND;
	wr.send_flags = ctx->send_flags;
	wr.wr.ud.ah = ctx->ah;
	wr.wr.ud.remote_qpn  = qpn;
	wr.wr.ud.remote_qkey = 0x11111111;
	struct ibv_send_wr *bad_wr;
	return ibv_post_send(ctx->qp, &wr, &bad_wr);
}

RDMAConnection::RDMAConnection(int id, int isLast, char *ib_devname_in, int gidx_in, int remote_qp0_in, char* servername)
{
	strncpy(ib_devname, ib_devname_in, 7);
	gidx = gidx_in;
	//string hostname = "192.168.1.5";
	//char servername [12] = "192.168.1.5";
	int port = connect_port + id;

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	assert(fd != -1);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	//int ret = inet_aton(servername.c_str(), &addr.sin_addr);
	//assert(ret);

	//addr.sin_port = htons(port);
	//ret = connect(fd, (sockaddr *) &addr, sizeof(addr));
	//assert(!ret);

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get IB devices list");
	}
	
	int b;
	for (b = 0; dev_list[b]; ++b)
		if (!strcmp(ibv_get_device_name(dev_list[b]), ib_devname))
			break;
	ib_dev = dev_list[b];
	if (!ib_dev) {
		fprintf(stderr, "IB device %s not found\n", ib_devname);
	}
	
	//if(isLast == 1) {	
	//	rx_depth = 1;
	//}
	
	ctx = pp_init_ctx(ib_dev, rx_depth, ib_port, use_event, id);
	if (!ctx) {
		printf("context creation invalid \n");
	}

	if (use_event)
		if (ibv_req_notify_cq(ctx->cq, 0)) {
			fprintf(stderr, "Couldn't request CQ notification\n");
		}

	if (pp_get_port_info(ctx->context, ib_port, &ctx->portinfo)) {
		fprintf(stderr, "Couldn't get port info\n");
	}
	my_dest.lid = ctx->portinfo.lid;

	my_dest.qpn = ctx->qp->qp_num;
	my_dest.psn = lrand48() & 0x000000;

	if (gidx >= 0) {
		if (ibv_query_gid(ctx->context, ib_port, gidx, &my_dest.gid)) {
			fprintf(stderr, "Could not get local gid for gid index "
								"%d\n", gidx);
		}
	} else
		memset(&my_dest.gid, 0, sizeof my_dest.gid);
	inet_ntop(AF_INET6, &my_dest.gid, gid, sizeof gid);
	printf("  local address:  LID 0x%04x, QPN %d, PSN 0x%06x: GID %s\n",my_dest.lid, my_dest.qpn, my_dest.psn, gid);
	int server_qpn0 = remote_qp0_in;
	if(id == 0) {
		rem_dest = pp_client_exch_dest(servername, server_qpn0, &my_dest);

		if (!rem_dest){
		 	printf("rem_dest was not exchanged correctly \n");	
		}
		
		inet_ntop(AF_INET6, &rem_dest->gid, gid, sizeof gid);
		printf("  remote address: LID 0x%04x, QPN %d, PSN 0x%06x, GID %s\n",rem_dest->lid, rem_dest->qpn, rem_dest->psn, gid);
	}

	if (pp_connect_ctx(ctx, ib_port, my_dest.psn, sl, rem_dest, gidx))
	{
		printf("couldn't connect to remote server \n");	
	}
}
#endif
