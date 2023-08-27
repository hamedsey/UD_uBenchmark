/*
 * Written by Hamed Seyedroudbari (Arm Research Intern - Summer 2022)
 */

#ifndef _RDMACONNECTION_
#define _RDMACONNECTION_
#include <arpa/inet.h>
//#include <event2/event.h>
#include <sys/uio.h>
#include <unistd.h>

#include <string>

#include "ServerRDMAConnection.h"

#ifdef __linux__
#include <malloc.h>
#endif

extern uint64_t buffersPerQ;

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

struct pingpong_context* RDMAConnection::pp_init_ctx(struct ibv_device *ib_dev, int rx_depth, int port, int use_event, int id, uint64_t numQueues, uint64_t numThreads)
{
	if(id == 0) {

		buf_recv = (char**)malloc(recv_bufs_num*sizeof(char*));
		buf_send = (char**)malloc(recv_bufs_num*sizeof(char*));

		mr_recv = (struct ibv_mr**)malloc(recv_bufs_num*sizeof(struct ibv_mr*));
		mr_send = (struct ibv_mr**)malloc(recv_bufs_num*sizeof(struct ibv_mr*));


		ctxGlobal = (pingpong_context_global*)malloc(sizeof(struct pingpong_context_global));
		memset(ctxGlobal, 0x00, sizeof(struct pingpong_context_global));
		if (!ctxGlobal)
			return NULL;

		//printf("b4 allocating qps \n");
		//ctx->qp = (struct ibv_qp **)malloc(numQueues*sizeof(struct ibv_qp *));
		//memset(ctx->qp, 0x00, numQueues*sizeof(struct ibv_qp *));
		//if (!ctx->qp)
		//	return NULL;
		
		//printf("after allocating qps \n");

		for (int j = 0; j<recv_bufs_num; j++) {
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
		}

		ctxGlobal->context = ibv_open_device(ib_dev);
		if (!ctxGlobal->context) {
			fprintf(stderr, "Couldn't get context for %s\n",
				ibv_get_device_name(ib_dev));
			goto clean_buffer;
		}

		{
			struct ibv_port_attr port_info = {};
			int mtu;

			if (ibv_query_port(ctxGlobal->context, port, &port_info)) {
				fprintf(stderr, "Unable to query port info for port %d\n", port);
				goto clean_device;
			}
			mtu = 1 << (port_info.active_mtu + 7);
			if (1024 > mtu) { //size > mtu
				fprintf(stderr, "Requested size larger than port MTU (%d)\n", mtu);
				goto clean_device;
			}
		}

		if (use_event) {
			ctxGlobal->channel = ibv_create_comp_channel(ctxGlobal->context);
			if (!ctxGlobal->channel) {
				fprintf(stderr, "Couldn't create completion channel\n");
				goto clean_device;
			}
		} else
			ctxGlobal->channel = NULL;

		ctxGlobal->pd = ibv_alloc_pd(ctxGlobal->context);
		if (!ctxGlobal->pd) {
			fprintf(stderr, "Couldn't allocate PD\n");
			goto clean_comp_channel;
		}

		for (int j = 0; j<recv_bufs_num; j++) {
			mr_recv[j] = ibv_reg_mr(ctxGlobal->pd, buf_recv[j], 4096, IBV_ACCESS_LOCAL_WRITE);
			if (!mr_recv[j]) {
				fprintf(stderr, "Couldn't register MR\n");
				goto clean_pd;
			}
			mr_send[j] = ibv_reg_mr(ctxGlobal->pd, buf_send[j], 4096, IBV_ACCESS_LOCAL_WRITE);
			if (!mr_send[j]) {
				fprintf(stderr, "Couldn't register MR\n");
				goto clean_pd;
			}
		}

		#if SHARED_CQ //for signaled sends use the 2x multiple
			uint64_t cqDepth = 2*numQueues*buffersPerQ + 1;
			printf("cqDepth = %llu \n", cqDepth);
			ctxGlobal->cq = (struct ibv_cq **)malloc(numThreads*sizeof(struct ibv_cq *));
			for(uint8_t t = 0; t < numThreads; t++) {
				printf("t = = %llu \n", t);
				ctxGlobal->cq[t] = ibv_create_cq(ctxGlobal->context, cqDepth, NULL, ctxGlobal->channel, 0);
				if (!ctxGlobal->cq[t]) {
					fprintf(stderr, "Couldn't create CQ\n");
					goto clean_mr;
				}
			}
		#endif

		#if USE_SRQ
			struct ibv_srq_init_attr srq_init_attr;
			
			memset(&srq_init_attr, 0, sizeof(srq_init_attr));
			
			srq_init_attr.attr.max_wr  = 32767;
			srq_init_attr.attr.max_sge = 31;
			
			ctxGlobal->srq = ibv_create_srq(ctxGlobal->pd, &srq_init_attr);
			if (!ctxGlobal->srq) {
				fprintf(stderr, "Error, ibv_create_srq() failed\n");
				exit(0);
			}
		#endif
	}

	ctx = (pingpong_context*)malloc(sizeof(struct pingpong_context));
	memset(ctx, 0x00, sizeof(struct pingpong_context));
	if (!ctx)
		return NULL;


	ctx->send_flags = IBV_SEND_INLINE;
	ctx->rx_depth   = rx_depth;

	#if !SHARED_CQ
		//uint64_t cqDepth = 2*buffersPerQ + 1;
		//printf("cqDepth = %llu \n", cqDepth);
		//ctx->cq = ibv_create_cq(ctxGlobal->context, 2*rx_depth + 1, NULL, ctxGlobal->channel, 0);
		ctx->cq = ibv_create_cq(ctxGlobal->context, 2*buffersPerQ + 1, NULL, ctxGlobal->channel, 0);

		if (!ctx->cq) {
			fprintf(stderr, "Couldn't create CQ\n");
			goto clean_mr;
		}
	#endif

	//{
		struct ibv_qp_attr attr;
		memset(&attr, 0, sizeof(attr));
		struct ibv_qp_init_attr init_attr;
		memset(&init_attr, 0, sizeof(init_attr));
				
		#if SHARED_CQ
			init_attr.send_cq = ctxGlobal->cq[id%numThreads];
			init_attr.recv_cq = ctxGlobal->cq[id%numThreads];
		#else
			init_attr.send_cq = ctx->cq;
			init_attr.recv_cq = ctx->cq;
		#endif

		init_attr.cap.max_send_wr  = rx_depth;
		init_attr.cap.max_recv_wr  = rx_depth;
		init_attr.cap.max_send_sge = 1;
		init_attr.cap.max_recv_sge = 1;
		init_attr.sq_sig_all = 0;

		#if USE_SRQ
			init_attr.srq = ctxGlobal->srq;
		#endif

		init_attr.qp_type = IBV_QPT_UD;

		if(id == 0) {
			while(1)
			{
				ctx->qp = ibv_create_qp(ctxGlobal->pd, &init_attr);
				if (!ctx->qp)  {
					fprintf(stderr, "Couldn't create QP\n");
					goto clean_cq;
				}

				if(ctx->qp->qp_num %(512) == 0) break;
				else ibv_destroy_qp(ctx->qp);
			}
		}
		else {
			ctx->qp = ibv_create_qp(ctxGlobal->pd, &init_attr);
				if (!ctx->qp)  {
					fprintf(stderr, "Couldn't create QP\n");
					goto clean_cq;
				}
		}

		ibv_query_qp(ctx->qp, &attr, IBV_QP_CAP, &init_attr);
		if (init_attr.cap.max_inline_data >= (unsigned int) 1024) { //size
			ctx->send_flags |= IBV_SEND_INLINE;
		}
	//}
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
	#if SHARED_CQ
		for(uint8_t t = 0; t < numThreads; t++)
			ibv_destroy_cq(ctxGlobal->cq[t]);
	#else
		ibv_destroy_cq(ctx->cq);
	#endif

clean_srq:
	#if USE_SRQ
		if (ibv_destroy_srq(ctxGlobal->srq)) {
			fprintf(stderr, "Error, ibv_destroy_srq() failed\n");
			exit(0);
		}
	#endif

clean_mr:
	for (int j = 0; j<recv_bufs_num; j++) {
		ibv_dereg_mr(mr_recv[j]);
		ibv_dereg_mr(mr_send[j]);
	}

clean_pd:
	ibv_dealloc_pd(ctxGlobal->pd);

clean_comp_channel:
	if (ctxGlobal->channel)
		ibv_destroy_comp_channel(ctxGlobal->channel);

clean_device:
	ibv_close_device(ctxGlobal->context);

clean_buffer:
	for (int j = 0; j<recv_bufs_num; j++) {
		free(buf_recv[j]);
		free(buf_send[j]);
	}

clean_ctx:
	free(ctx);

	return NULL;
}

int RDMAConnection::pp_post_recv(struct pingpong_context *ctx, int wr_id)
{
	struct ibv_sge list;
	memset(&list, 0, sizeof(list));

	struct ibv_recv_wr wr;
	memset(&wr, 0, sizeof(wr));

	struct ibv_recv_wr *bad_wr;
	memset(&bad_wr, 0, sizeof(bad_wr));

	uint16_t bufIndex = wr_id-recv_bufs_num;
	//uint16_t qpID = bufIndex/bufsPerQP;
	//printf("bufIndex = %d \n", bufIndex);
	//printf("qpID = %d \n",qpID);
	//printf("bufsPerQP = %d \n", bufsPerQP);

	list.addr = (uintptr_t) buf_recv[bufIndex];
	list.length = 40 + size;
	list.lkey = mr_recv[bufIndex]->lkey;

	wr.wr_id = wr_id;
	wr.sg_list = &list;
	wr.num_sge = 1;

	#if USE_SRQ
		return ibv_post_srq_recv(ctxGlobal->srq, &wr, &bad_wr);
	#else
		return ibv_post_recv(ctx->qp, &wr, &bad_wr);
	#endif

}

int RDMAConnection::pp_close_ctx(struct pingpong_context *ctx, uint64_t numQueues, uint64_t numThreads)
{
	//for(int i = 0; i < numQueues; i++){
		if (ibv_destroy_qp(ctx->qp)) {
			fprintf(stderr, "Couldn't destroy QP\n");
			return 1;
		}
	//}
	#if SHARED_CQ
		for(uint8_t t = 0; t < numThreads; t++) {
			if (ibv_destroy_cq(ctxGlobal->cq[t])) {
				fprintf(stderr, "Couldn't destroy CQ\n");
				return 1;
			}	
		}
	#else
		if (ibv_destroy_cq(ctx->cq)) {
			fprintf(stderr, "Couldn't destroy CQ\n");
			return 1;
		}	
	#endif


	for (int j = 0; j<recv_bufs_num; j++) {
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

	if (ibv_dealloc_pd(ctxGlobal->pd)) {
		fprintf(stderr, "Couldn't deallocate PD\n");
		return 1;
	}

	if (ctxGlobal->channel) {
		if (ibv_destroy_comp_channel(ctxGlobal->channel)) {
			fprintf(stderr, "Couldn't destroy completion channel\n");
			return 1;
		}
	}

	if (ibv_close_device(ctxGlobal->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}

	for (int j = 0; j<recv_bufs_num; j++) {
		free(buf_recv[j]);
		free(buf_send[j]);
	}

	free(ctx);

	return 0;
}


struct pingpong_dest* RDMAConnection::pp_server_exch_dest(char *servername)
{
	rem_dest = (struct pingpong_dest*)malloc(sizeof(struct pingpong_dest));
	if (!rem_dest)
		return NULL;
	rem_dest->gid = my_dest.gid;

	char * pch;
	pch = strtok (servername," ,.-");
	int i = 0;
	while (pch != NULL)
	{
		rem_dest->gid.raw[12+i] = strtol(pch, NULL, 10);
		i++;
		pch = strtok (NULL, ".");
	}
	return rem_dest;
}

int RDMAConnection::pp_connect_ctx(struct pingpong_context *ctx, int port, int my_psn, int sl, int sgid_idx, int id)
{
	struct ibv_ah_attr ah_attr;
	memset(&ah_attr, 0, sizeof(ah_attr));
	ah_attr.is_global = 0;

	ah_attr.dlid = my_dest.lid;

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

	attr.qp_state = IBV_QPS_RTS;
	attr.sq_psn	  = my_psn;

	if (ibv_modify_qp(ctx->qp, &attr,
			  IBV_QP_STATE              |
			  IBV_QP_SQ_PSN)) {
		fprintf(stderr, "Failed to modify QP to RTS\n");
		return 1;
	}

	ah_attr.is_global = 1;
	ah_attr.grh.hop_limit = 100;
	ah_attr.grh.dgid = rem_dest->gid;
	ah_attr.grh.sgid_index = sgid_idx;

	ctx->ah = ibv_create_ah(ctxGlobal->pd, &ah_attr);
	if (!ctx->ah) {
		fprintf(stderr, "Failed to create AH\n");
		return 1;
	}

	return 0;
}

int RDMAConnection::pp_post_send(struct pingpong_context *ctx, uint32_t qpn, unsigned int length, int wr_id, bool signal)
{
	struct ibv_sge list;
	memset(&list, 0, sizeof(list));

	list.addr = (uintptr_t) buf_send[wr_id];
	list.length = length;
	list.lkey	= mr_send[wr_id]->lkey;

	struct ibv_send_wr wr; 
	memset(&wr, 0, sizeof(wr));

	wr.wr_id	  = wr_id ;
	wr.sg_list    = &list;
	wr.num_sge    = 1;
	wr.opcode     = IBV_WR_SEND;
	if(signal == true) wr.send_flags = ctx->send_flags | IBV_SEND_SIGNALED; //need to have a signaled completion every signalInterval
	else wr.send_flags = ctx->send_flags;
	wr.wr.ud.ah = ctx->ah;
	wr.wr.ud.remote_qpn  = qpn;
	wr.wr.ud.remote_qkey = 0x11111111;

	struct ibv_send_wr *bad_wr;
	return ibv_post_send(ctx->qp, &wr, &bad_wr);
}

RDMAConnection::RDMAConnection(int id,  char *ib_devname_in, int gidx_in, char* servername, uint64_t numQueues, uint64_t numThreads)
{
	//printf("id = %d \n", id);
	strncpy(ib_devname, ib_devname_in, 7);

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get IB devices list");
	}	

	gidx = gidx_in;


	int b;
	for (b = 0; dev_list[b]; ++b)
		if (!strcmp(ibv_get_device_name(dev_list[b]), ib_devname)) {
			//printf("breaking! \n");
			break;
		}
	ib_dev = dev_list[b];
	if (!ib_dev) {
		fprintf(stderr, "IB device %s not found\n", ib_devname);
	}

	//printf("before init ctx, id = %d \n",id);
	ctx = pp_init_ctx(ib_dev, rx_depth, ib_port, use_event, id, numQueues, numThreads);
	if (!ctx) {
		printf("context creation invalid \n");
	}
	//printf("after init ctx, id = %d \n",id);

	/*
	if (use_event) {
		#if SHARED_CQ
			if (ibv_req_notify_cq(ctxGlobal->cq[0], 0)) {
				fprintf(stderr, "Couldn't request CQ notification\n");
			}
		#else
			if (ibv_req_notify_cq(ctx->cq, 0)) {
				fprintf(stderr, "Couldn't request CQ notification\n");
			}
		#endif
	}
	*/

	if (pp_get_port_info(ctxGlobal->context, ib_port, &ctx->portinfo)) {
		fprintf(stderr, "Couldn't get port info\n");
	}
	my_dest.lid = ctx->portinfo.lid;

	my_dest.qpn = ctx->qp->qp_num;
	my_dest.psn = lrand48() & 0x000000;

	if (gidx >= 0) {
		if (ibv_query_gid(ctxGlobal->context, ib_port, gidx, &my_dest.gid)) {
			fprintf(stderr, "Could not get local gid for gid index "
								"%d\n", gidx);
		}
	} else
		memset(&my_dest.gid, 0, sizeof my_dest.gid);

	inet_ntop(AF_INET6, &my_dest.gid, gid, sizeof gid);
	//printf("  local address:  LID 0x%04x, QPN 0x%06x, (int)QPN %d, PSN 0x%06x: GID %s\n", my_dest.lid, my_dest.qpn, my_dest.qpn, my_dest.psn, gid);

	if(id == 0) {
		rem_dest = pp_server_exch_dest(servername);

		if (!rem_dest) {
			printf("remote destination invalid \n");
		}

		inet_ntop(AF_INET6, &rem_dest->gid, gid, sizeof gid);

		printf("  local address:  LID 0x%04x, QPN 0x%06x, (int)QPN %d, PSN 0x%06x: GID %s\n", my_dest.lid, my_dest.qpn, my_dest.qpn, my_dest.psn, gid);
		printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",rem_dest->lid, rem_dest->qpn, rem_dest->psn, gid);
	}

	//printf("b4 connect ctx \n");

	if (pp_connect_ctx(ctx, ib_port, my_dest.psn, sl, gidx, id)) {
		fprintf(stderr, "Couldn't connect to remote QP\n");
		free(rem_dest);
		rem_dest = NULL;
		//goto out;
	}

	//printf("after connect ctx \n");

	uint16_t startbuf = id*recv_bufs_num/numQueues;
	uint16_t endbuf = startbuf + (recv_bufs_num/numQueues);
	for(int r = startbuf; r < endbuf; r++) {
		//printf("r = %d \n", r);
		if(!pp_post_recv(ctx, r+recv_bufs_num)) routs++;
	}

	//printf("after post recv \n");


	if (routs < recv_bufs_num/numQueues) {
		fprintf(stderr, "Couldn't post -recv_bufs_num- receive requests (%d)\n", routs);
	}
	//printf("outstanding recv requests %d\n", routs);

	scnt = 0;
	rcnt = 0;
}
#endif
